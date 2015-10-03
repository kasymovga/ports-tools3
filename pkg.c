#define _XOPEN_SOURCE 600
#define _POSIX_SOURCE
#include <kga/kga.h>
#include <kga/string.h>
#include <kga/exception.h>

#include "misc.h"
#include "pkg.h"
#include "kga_wrappers.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#define PKG_FILE_DIR 1
#define PKG_FILE_LNK 2

int (*pkg_confirm)(const char *fmt, ...) = NULL;
int string_compare(const void *, const void *);

exception_type_t exception_type_pkg_files_conflict = {};
exception_type_t exception_type_pkg_db_already_locked = {};
exception_type_t exception_type_pkg_already_installed = {};
exception_type_t pkg_aborted_by_user = {};

struct pkg_file {
	int flags;
	char *path;
};

struct pkg_fs_transaction {
	const char *from, *to, *backup;
};

struct pkg {
	const char *path;
	const char *name;
	const char *version;
	struct pkg_file *files;
};

struct pkg_info {
	const char *name;
	const char *version;
	char **files;
};

struct pkg_db {
	const char *lock_path;
	const char *path;
	const char *root;
	int lock_counter;
	pid_t lock_pid;
	struct pkg_info *pkgs;
};

struct pkg_db_conflict {
	struct pkg_info *info;
	char **files;
};

struct pkg_db *pkg_db_new(const char *root, const char *db_path) {
	struct pkg_db *db = new(struct pkg_db);
	db->path = string_new_fmt("%s/%s", root, db_path);
	db->root = root;
	db->lock_path = string_new_fmt("%s/.LOCK", db->path);
	db->lock_counter = 0;
	db->lock_pid = 0;
	db->pkgs = array_new(struct pkg_info, 0, ARRAY_NULL_TERMINATED);
	return db;
};

void pkg_db_drop(struct pkg_db *db, struct pkg_info *info, FILE *warning_stream);

void transaction_fs_transactions_commit(struct pkg_fs_transaction *transactions, FILE *warning_stream) {
	array_foreach(transactions, struct pkg_fs_transaction *, each_transaction) {
		if (each_transaction->backup && kga_file_exists(each_transaction->to)) {
			if (warning_stream) fprintf(warning_stream, "Renaming %s -> %s\n", each_transaction->to, each_transaction->backup);
			kga_rename(each_transaction->to, each_transaction->backup);
		};
		kga_rename(each_transaction->from, each_transaction->to);
		if (warning_stream) fprintf(warning_stream, "Renaming %s -> %s\n", each_transaction->from, each_transaction->to);
	};
	array_foreach(transactions, struct pkg_fs_transaction *, each_transaction) {
		if (each_transaction->backup) {
			if (warning_stream) fprintf(warning_stream, "Removing %s\n", each_transaction->backup);
			remove(each_transaction->backup);
		};
	};
};

void transaction_fs_transactions_rollback(struct pkg_fs_transaction *transactions, FILE *warning_stream) {
	array_foreach(transactions, struct pkg_fs_transaction *, each_transaction) {
		if (each_transaction->backup) {
			if (warning_stream) fprintf(warning_stream, "Renaming %s -> %s\n", each_transaction->backup, each_transaction->to);
			rename(each_transaction->backup, each_transaction->to);
		};
		if (warning_stream) fprintf(warning_stream, "Removing %s\n", each_transaction->from);
		remove(each_transaction->from);
	};
};

int pkg_db_installed(struct pkg_db *db, const char *name, const char *version) {
	int installed = 0;
	scope {
		char *pkg_info_path = string_new_fmt("%s/%s/%s", db->path, name, version);
		if (access(pkg_info_path, F_OK)) {
			if (errno != ENOENT) {
				throw_errno_verbose(pkg_info_path);
			};
		} else {
			installed = 1;
		};
	};
	return installed;
};

int pkg_installed(const char *pkg_root, const char *db_path, const char *name, const char *version) {
	int installed = 0;
	scope {
		struct pkg_db *db = pkg_db_new(pkg_root, db_path);
		installed = pkg_db_installed(db, name, version);
	};
	return installed;
};

void pkg_db_unlock(void *db_ptr) {
	if (!db_ptr) return;
	struct pkg_db *db = db_ptr;
	if (db->lock_pid != getpid()) return;
	if (!db->lock_counter) {
		return;
	};
	db->lock_counter--;
	if (!db->lock_counter) {
		rmdir(db->lock_path);
	};
};

static void pkg_db_lock(struct pkg_db *db) {
	kga_mkpath(db->path, 0755);
	if (!db->lock_pid) {
		db->lock_pid = getpid();
	};
	if (!db->lock_counter) {
		try {
			kga_mkdir(db->lock_path, 0700);
		};
		catch {
			if (exception_type_is(exception_type_mkdir_already_exists)) {
				throw(exception_type_pkg_db_already_locked, 1, "Package database already locked.", db->lock_path);
			} else {
				throw_proxy();
			};
		};
	};
	db->lock_counter++;
	scope_add(db, pkg_db_unlock);
};

void pkg_db_load_pkgs(struct pkg_db *db, int load_files) {
	scope_pool_t *start_scope = scope_current();
	scope {
		char *pkg_path = string_new();
		char *version_path = string_new();
		DIR *pkgs_dir = kga_opendir(db->path);
		DIR *versions_dir;
		struct dirent *dirent;
		struct pkg_info pkg_info;
		while ((dirent = readdir(pkgs_dir))) {
			if (dirent->d_name[0] == '.') continue;
			scope {
				string_fmt(pkg_path, "%s/%s", db->path, dirent->d_name);
				versions_dir = kga_opendir(pkg_path);
				struct dirent *version_dirent;
				while ((version_dirent = readdir(versions_dir))) {
					if (version_dirent->d_name[0] == '.') continue;
					string_fmt(version_path, "%s/%s", pkg_path, version_dirent->d_name);
					scope_use(start_scope) {
						pkg_info.name = string_new_set(dirent->d_name);
						pkg_info.version = string_new_set(version_dirent->d_name);
						if (load_files) {
							pkg_info.files = file_lines(version_path);
							array_sort(pkg_info.files, string_compare);
						} else {
							pkg_info.files = NULL;
						};
						array_push(db->pkgs, pkg_info);
					};
				};
			};
		};
	};
};

void pkg_db_remove_pkg(struct pkg_db *db, const char *name, const char *version) {
	important_check(db && name && version);
	scope {
		char *path = string_new_fmt("%s/%s/%s", db->path, name, version);
		remove(path);
	};
};

struct pkg_fs_transaction *pkg_db_write_pkg_info(struct pkg_db *db, struct pkg_info *pkg_info, struct pkg_fs_transaction *transactions) {
	scope {
		struct pkg_fs_transaction transaction;
		char *pkg_info_dir_path = string_new_fmt("%s/%s", db->path, pkg_info->name);
		scope_use_previous {
			transaction.from = string_new_fmt("%s/.%s.tmp", pkg_info_dir_path, pkg_info->version);
			transaction.to = string_new_fmt("%s/%s", pkg_info_dir_path, pkg_info->version);
			transaction.backup = NULL;
		};
		kga_mkpath(pkg_info_dir_path, 0755);
		lines_to_file(pkg_info->files, transaction.from);
		array_push(transactions, transaction);
	};
	return transactions;
};

struct pkg_fs_transaction *pkg_db_write_pkg(struct pkg_db *db, struct pkg *pkg, struct pkg_fs_transaction *transactions) {
	important_check(pkg->files);
	struct pkg_info pkg_info;
	pkg_info.files = array_new(char *, 0, 0);
	pkg_info.name = pkg->name;
	pkg_info.version = pkg->version;
	for (size_t i = 0, n = array_length(pkg->files); i < n; i++) {
		array_push(pkg_info.files, pkg->files[i].path);
	};
	array_push(db->pkgs, pkg_info);
	return pkg_db_write_pkg_info(db, &pkg_info, transactions);
};

struct pkg_db_conflict *pkg_db_find_conflicts(struct pkg_db *db, struct pkg *pkg) {
	struct pkg_db_conflict *conflicts = array_new(struct pkg_db_conflict, 0, ARRAY_NULL_TERMINATED);
	struct pkg_db_conflict conflict;
	important_check(db->pkgs);
	int cmp;
	for (size_t i = 0, n = array_length(db->pkgs); i < n; i++) {
		conflict.files = NULL;
		important_check(pkg->files);
		important_check(db->pkgs[i].files);
		for (size_t j = 0, k = 0, m = array_length(pkg->files), l = array_length(db->pkgs[i].files); j < m && k < l; j++) {
			if (pkg->files[j].flags & PKG_FILE_DIR) continue;
			cmp = strcmp(pkg->files[j].path, db->pkgs[i].files[k]);
			if (cmp > 0) {
				k++;
				j--;
				continue;
			} else if (cmp < 0) {
				continue;
			} else {
				if (!conflict.files) {
					conflict.files = array_new(char *, 0, ARRAY_NULL_TERMINATED);
				};
				array_push(conflict.files, db->pkgs[i].files[k]);
				k++;
			}
		};
		if (conflict.files) {
			conflict.info = &(db->pkgs[i]);
			array_push(conflicts, conflict);
		};
	};
	return conflicts;
};

struct pkg_fs_transaction *pkg_db_remove_conflicts(struct pkg_db *db, struct pkg_db_conflict *conflicts, struct pkg_fs_transaction *transactions) {
	important_check(conflicts);
	for (size_t i = 0, n = array_length(conflicts); i < n; i++) {
		important_check(conflicts[i].files);
		for (size_t j = 0, m = array_length(conflicts[i].files); j < m; j++) {
			//FIXME: Optimization needed
			for (size_t k = 0; k < array_length(conflicts[i].info->files); k++) {
				if (!strcmp(conflicts[i].info->files[k], conflicts[i].files[j])) {
					array_delete_interval(conflicts[i].info->files, k, 1);
				};
			};
		};
#if 0
		if (pkg_confirm) {
			scope {
				if (!pkg_confirm("Package info will be updated %s/%s:\n%s\nContinue?\n",
							conflicts[i].info->name,
							conflicts[i].info->version,
							string_join(conflicts[i].info->files, "\n", 0))) {
					throw(pkg_aborted_by_user, 1, "Aborted by user", NULL);
				};
			};
		};
		transactions = pkg_db_write_pkg_info(db, conflicts[i].info, transactions);
#endif
	};
	return transactions;
};

void pkg_regular_file_install(const char *from, const char *to, mode_t mode) {
	scope {
		FILE *from_file = kga_fopen(from, "r");
		FILE *to_file = kga_fopen(to, "w");
		char buffer[1024];
		size_t readed;
		while ((readed = kga_fread(buffer, 1, 1024, from_file))) {
			kga_fwrite(buffer, 1, readed, to_file);
		};
		//kga_fflush_and_fsync(to_file);
	};
	if (chmod(to, mode)) throw_errno_verbose(to);
};

void pkg_symlink_install(const char *from, const char *to) {
	scope {
		char *from_target = kga_readlink(from);
		remove(to);
		kga_symlink(from_target, to);
	};
};

struct pkg_fs_transaction *pkg_install_files(struct pkg_db *db, struct pkg *pkg, struct pkg_fs_transaction *transactions) {
	struct pkg_fs_transaction transaction;
	scope {
		char *file_path = string_new();
		char *mkpath_path = string_new();
		for (size_t i = 0, files_count = array_length(pkg->files); i < files_count; i++) {
			string_fmt(file_path, "%s/%s", pkg->path, pkg->files[i].path);
			if (pkg->files[i].flags & PKG_FILE_DIR) {
				string_fmt(mkpath_path, "%s/%s", db->root, pkg->files[i].path) ;
				kga_mkpath(mkpath_path, 0755);
			} else {
				scope_use_previous {
					transaction.to = string_new_fmt("%s/%s", db->root, pkg->files[i].path);
					transaction.from = string_new_fmt("%s/%s.pkg.transaction.new", db->root, pkg->files[i].path);
					transaction.backup = string_new_fmt("%s/%s.pkg.transaction.backup", db->root, pkg->files[i].path);
				};
				if (pkg->files[i].flags & PKG_FILE_LNK) {
					pkg_symlink_install(file_path, transaction.from);
				} else {
					struct stat st;
					kga_lstat(file_path, &st);
					pkg_regular_file_install(file_path, transaction.from, st.st_mode & 0777);
				};
				array_push(transactions, transaction);
			};
		};
	};
	return transactions;
};

int pkg_file_compare(const void *ptr1, const void *ptr2) {
	const struct pkg_file *pkg_file1 = ptr1;
	const struct pkg_file *pkg_file2 = ptr2;
	return strcmp(pkg_file1->path, pkg_file2->path);
};

struct pkg *pkg_load(struct pkg *pkg, const char *pkg_path, const char *sub_path) {
	int need_sort = 0;
	if (!pkg) {
		need_sort = 1;
		pkg = new(struct pkg);
		pkg->path = pkg_path;
		pkg->files = array_new(struct pkg_file, 0, ARRAY_NULL_TERMINATED);
		scope {
			char *name_path = string_new_fmt("%s/.name", pkg_path);
			char *version_path = string_new_fmt("%s/.version", pkg_path);
			scope_use_previous {
				pkg->name = string_from_file(name_path);
				pkg->version = string_from_file(version_path);
			};
		};
	};
	scope {
		char *file_full_path = string_new();
		struct pkg_file pkg_file;
		const char *files_path = sub_path ? string_new_fmt("%s/%s", pkg_path, sub_path) : pkg_path;
		char *file_path = NULL;
		DIR *files_dir = kga_opendir(files_path);
		struct dirent *dirent;
		while ((dirent = readdir(files_dir))) {
			if (!sub_path || !*sub_path) {
				if (dirent->d_name[0] == '.') continue;
			} else {
				if (dirent->d_name[0] == '.' && (dirent->d_name[1] == '\0' || (dirent->d_name[1] == '.' && dirent->d_name[2] == '\0'))) continue;
			};
			scope_use_previous {
				if (sub_path && *sub_path) {
					file_path = string_new_fmt("%s/%s", sub_path, dirent->d_name);
				} else {
					file_path = string_new_set(dirent->d_name);
				};
			};
			string_fmt(file_full_path, "%s/%s", pkg_path, file_path);
			pkg_file.flags = 0;
			struct stat st;
			kga_lstat(file_full_path, &st);
			if (S_ISDIR(st.st_mode)) {
				pkg_file.flags |= PKG_FILE_DIR;
				scope_use_previous {
					pkg_load(pkg, pkg_path, file_path);
				};
			};
			if (S_ISLNK(st.st_mode)) {
				pkg_file.flags |= PKG_FILE_LNK;
			};
			pkg_file.path = file_path;
			array_push(pkg->files, pkg_file);
		};
	};
	if (need_sort) {
		array_sort(pkg->files, pkg_file_compare);
	};
	return pkg;
};

void pkg_install(const char *pkg_path, const char *root, const char *db_path, int flags, FILE *warning_stream) {
	scope {
		struct pkg_fs_transaction *pkg_install_transactions = array_new(struct pkg_fs_transaction, 0, ARRAY_NULL_TERMINATED);
		struct pkg_db *db = pkg_db_new(root, db_path);
		struct pkg *pkg = pkg_load(NULL, pkg_path, NULL);
		pkg_db_lock(db);
		if (pkg_db_installed(db, pkg->name, pkg->version)) {
			throw(exception_type_pkg_already_installed, 1, "this package already installed", NULL);
		};
		pkg_db_load_pkgs(db, 1);
		char *file_fs_path = string_new();
		important_check(pkg->files);
		for (int i = 0, n = array_length(pkg->files); i < n; i++) {
			string_fmt(file_fs_path, "%s/%s", root, pkg->files[i].path);
			struct stat st;
			if (!kga_lstat_skip_enoent(file_fs_path, &st)) {
				if (S_ISDIR(st.st_mode)) {
					if (pkg->files[i].flags != PKG_FILE_DIR)
						throw(exception_type_pkg_files_conflict, 1, "file/directory mismatch", file_fs_path);
				} else {
					if (pkg->files[i].flags == PKG_FILE_DIR)
						throw(exception_type_pkg_files_conflict, 1, "file/directory mismatch", file_fs_path);
				};
			};
		};
		if (warning_stream) fprintf(warning_stream, "Searching conflicts\n");
		struct pkg_db_conflict *conflicts = pkg_db_find_conflicts(db, pkg);
		important_check(conflicts);
		size_t conflicts_len = array_length(conflicts);
		if (conflicts_len) {
			char *conflict_description = string_new();
			for (size_t i = 0; i < conflicts_len; i++) {
				string_fmt(conflict_description, "%s\n%s/%s:\n", conflict_description, conflicts[i].info->name, conflicts[i].info->version);
				important_check(conflicts[i].files);
				for (size_t j = 0, n = array_length(conflicts[i].files); j < n; j++) {
					string_fmt(conflict_description, "%s %s\n", conflict_description, conflicts[i].files[j]);
				};
			};
			if (!(flags & PKG_FORCE_INSTALL)) {
				if (flags & PKG_UPGRADE) {
					for (size_t i = 0; i < conflicts_len; i++) {
						if (strcmp(conflicts[i].info->name, pkg->name)) {
							throw(exception_type_pkg_files_conflict, 1, "Conflict with other packages", conflict_description);
						};
					};
				} else {
					throw(exception_type_pkg_files_conflict, 1, "Conflict with other packages", conflict_description);
				};
			};
			struct pkg_fs_transaction *conflict_delete_transactions = array_new(struct pkg_fs_transaction, 0, ARRAY_NULL_TERMINATED);
			if (array_length(conflict_delete_transactions) > 0) 
				if (warning_stream) fprintf(warning_stream, "Cleaning conflicts\n");
			conflict_delete_transactions = pkg_db_remove_conflicts(db, conflicts, conflict_delete_transactions);
			try {
				transaction_fs_transactions_commit(conflict_delete_transactions, warning_stream);
			};
			catch {
				transaction_fs_transactions_rollback(conflict_delete_transactions, warning_stream);
				throw_proxy();
			};
		};
		if (warning_stream) fprintf(warning_stream, "Preparing transaction\n");
		pkg_install_transactions = pkg_install_files(db, pkg, pkg_install_transactions);
		pkg_install_transactions = pkg_db_write_pkg(db, pkg, pkg_install_transactions);
		try {
			if (pkg_confirm && !pkg_confirm("Process fs transaction for %s/%s?", pkg->name, pkg->version)) {
				throw(pkg_aborted_by_user, 1, "Aborted by user", NULL);
			};
			transaction_fs_transactions_commit(pkg_install_transactions, warning_stream);
		};
		catch {
			transaction_fs_transactions_rollback(pkg_install_transactions, warning_stream);
			throw_proxy();
		};
		try {
			char *scripts_path = string_new_fmt("%s/usr/lib/pkg-hooks/*", db->root);
			char **script = kga_glob(scripts_path);
			for (; *script; script++) {
				int status;
				pid_t script_pid = kga_fork();
				if (!script_pid) {
					kga_chdir(pkg_path);
					if (unsetenv("ROOT")) throw_errno();
					const char *script_copy = strdup(*script);
					if (!script_copy) throw_errno();
					if (db->root && *db->root && setenv("ROOT", db->root, 1)) throw_errno();
					while (scope_current()) scope_end();
					execl(script_copy, script_copy, NULL);
					exit(EXIT_FAILURE);
				};
				waitpid(script_pid, &status, 0);
			};
		};
		catch {
			if (exception_type_is(exception_type_glob_aborted) || exception_type_is(exception_type_glob_nomatch)) {
				if (warning_stream) fprintf(warning_stream, "Post-install scripts not available\n");
			} else {
				throw_proxy();
			};
		};
	
		if (flags & PKG_UPGRADE) {
			array_foreach(db->pkgs, struct pkg_info *, each_pkg_info) {
				if (!strcmp(each_pkg_info->name, pkg->name) && strcmp(each_pkg_info->version, pkg->version)) {
					if (pkg_confirm && !pkg_confirm("Remove package %s/%s?", each_pkg_info->name, each_pkg_info->version)) continue;
					pkg_db_drop(db, each_pkg_info, warning_stream);
				};
			};
		};
	};
};

int string_compare(const void *foo, const void *bar) {
	return strcmp(*(char **)foo, *(char **)bar);
};

struct pkg_info *pkg_db_find_info(struct pkg_db *db, const char *name, const char *version) {
	return NULL;
};

void pkg_db_drop(struct pkg_db *db, struct pkg_info *pkg_info, FILE *warning_stream) {
	scope {
		char *pkg_info_path = string_new_fmt("%s/%s/%s", db->path, pkg_info->name, pkg_info->version);
		char *file_path = string_new();;
		array_foreach_reverse(pkg_info->files, char **, each_file) {
			array_foreach(db->pkgs, struct pkg_info *, each_pkg_info) {
				if (each_pkg_info == pkg_info) continue;
				array_foreach(each_pkg_info->files, char **, each_pkg_info_file) {
					if (!strcmp(*each_file, *each_pkg_info_file)) goto skip_file_remove;
				};
			};
			string_fmt(file_path, "%s/%s", db->root, *each_file);
			if (warning_stream) fprintf(warning_stream, "Removing %s\n", file_path);
			if (remove(file_path) && warning_stream) {
				fprintf(warning_stream, "remove: %s: %s\n", file_path, strerror(errno));
			};
			skip_file_remove:;
		};
		if (warning_stream) fprintf(warning_stream, "Removing %s\n", pkg_info_path);
		if (remove(pkg_info_path) && warning_stream) {
			fprintf(warning_stream, "remove: %s: %s\n", pkg_info_path, strerror(errno));
		};
	};
};

void pkg_drop(const char *root, const char *db_path, const char *name, const char *version, FILE *warning_stream) {
	scope {
		struct pkg_db *db = pkg_db_new(root, db_path);
		pkg_db_lock(db);
		pkg_db_load_pkgs(db, 1);
		array_foreach(db->pkgs, struct pkg_info *, each_pkg_info) {
			if (!strcmp(each_pkg_info->name, name) && (!version || !strcmp(each_pkg_info->version, version))) {
				pkg_db_drop(db, each_pkg_info, warning_stream);
			};
		};
	};
};

struct pkg_list_item *pkg_db_list(const char *pkg_root, const char *db_path) {
	struct pkg_list_item *list = array_new(struct pkg_list_item, 0, ARRAY_NULL_TERMINATED);
	struct pkg_db *db = pkg_db_new(pkg_root, db_path);
	pkg_db_load_pkgs(db, 0);
	struct pkg_list_item list_item;
	array_foreach(db->pkgs, struct pkg_info *, each_pkg_info) {
		list_item.name = each_pkg_info->name;
		list_item.version = each_pkg_info->version;
		array_push(list, list_item);
	};
	return list;
};
