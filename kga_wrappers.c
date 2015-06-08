#define _POSIX_SOURCE
#define _XOPEN_SOURCE 500
#define _POSIX_C_SOURCE 200809L
#include <libgen.h>
#include <glob.h>
#include <kga/kga.h>
#include <kga/string.h>
#include <kga/array.h>
#include "kga_wrappers.h"
#include "misc.h"

exception_type_t exception_type_fopen_no_such_file;
exception_type_t exception_type_mkdir_already_exists;
exception_type_t exception_type_glob_nospace;
exception_type_t exception_type_glob_aborted;
exception_type_t exception_type_glob_nomatch;
exception_type_t exception_type_glob_unknown;

void kga_free_pipe(void *ptr) {
	int *pipe = ptr;
	close(pipe[0]);
	close(pipe[1]);
	free(ptr);
};

void kga_free_file(void *ptr) {
	fclose((FILE *)ptr);
};

void kga_free_dir(void *ptr) {
	closedir((DIR *)ptr);
};

void kga_free_glob(void *ptr) {
	globfree((glob_t *)ptr);
};

int kga_fork() {
	pid_t ret;
	if ((ret = fork()) < 0) throw_errno();
	return ret;
};

void *kga_malloc(size_t size) {
	void *ret = malloc(size);
	if (!ret) throw_errno();
	return ret;
};

int *kga_pipe() {
	int *fds = kga_malloc(sizeof(int) * 2);
	fds[0] = -1;
	fds[1] = -1;
	scope_add(fds, kga_free_pipe);
	if (pipe(fds)) throw_errno();
	return fds;
};

void kga_dup2(int oldfd, int newfd) {
	if (dup2(oldfd, newfd) < 0) throw_errno();
};

FILE *kga_fdopen(int fd, const char *opt) {
	FILE *file = fdopen(fd, opt);
	if (!file) throw_errno();
	scope_add(file, kga_free_file);
	return file;
};

FILE *kga_fopen(const char *path, const char *opt) {
	FILE *file = fopen(path, opt);
	if (!file) {
		if (errno == ENOENT) {
			throw_errno_with_type_verbose(exception_type_fopen_no_such_file, path);
		} else {
			throw_errno();
		};
	};
	scope_add(file, kga_free_file);
	return file;
};

size_t kga_fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
	size_t readed = fread(ptr, size, nmemb, stream);
	if (!readed) {
		if (ferror(stream)) throw_errno();
	};
	return readed;
};

size_t kga_fwrite(void *ptr, size_t size, size_t nmemb, FILE *stream) {
	size_t writed = fwrite(ptr, size, nmemb, stream);
	while (writed < nmemb) {
		throw_errno();
	};
	return writed;
};

void kga_mkdir(const char *path, mode_t mode) {
	if (mkdir(path, mode)) {
		if (errno == EEXIST) {
			throw_errno_with_type_verbose(exception_type_mkdir_already_exists, path);
		} else {
			throw_errno_verbose(path);
		};
	};
};

void kga_mkpath(const char *path, mode_t mode) {
	if (!strcmp(path, ".") || !strcmp(path, "/")) return;
	scope {
		char *path_copy = string_new_set(path);
		path_copy = dirname(path_copy);
		kga_mkpath(path_copy, mode);
		try {
			mkdir(path, mode);
		};
		catch {
			if (!exception_type_is(exception_type_mkdir_already_exists)) throw_proxy();
		};
	};
};

void kga_rmdir(const char *path) {
	if (rmdir(path)) throw_errno();
};

DIR *kga_opendir(const char *path) {
	DIR *dir = opendir(path);
	if (!dir) throw_errno();
	scope_add(dir, kga_free_dir);
	return dir;
};

void kga_lstat(const char *path, struct stat *stat) {
	if (lstat(path, stat)) {
		throw_errno_verbose(path);
	};
};

int kga_lstat_skip_enoent(const char *path, struct stat *stat) {
	int r;
	if ((r = lstat(path, stat))) {
		if (errno != ENOENT)
			throw_errno_verbose(path);
	};
	return r;
};

void kga_chdir(const char *path) {
	if (chdir(path)) {
		throw_errno_verbose(path);
	};
};

void kga_mkdtemp(char *template) {
	if (!mkdtemp(template)) {
		throw_errno_verbose(template);
	};
};

void kga_chown(const char *path, uid_t uid, gid_t gid) {
	if (chown(path, uid, gid)) {
		throw_errno_verbose(path);
	};
};

void kga_rename(const char *old_path, const char *new_path) {
	if (rename(old_path, new_path)) throw_errno_verbose(new_path);
};

void kga_fflush_and_fsync(FILE *file) {
	if (fflush(file)) throw_errno();
	if (fsync(fileno(file))) throw_errno();
};

char *kga_readlink(const char *path) {
	char *link_target = array_new(char, 0, ARRAY_NULL_TERMINATED);
	array_resize(link_target, 64);
	size_t buffer_size;
	for (ssize_t readlink_result; (readlink_result = readlink(path, link_target, (buffer_size = array_length(link_target)))); ) {
		if (readlink_result < 0) throw_errno_verbose(path);
		if (readlink_result < buffer_size) {
			array_resize(link_target, readlink_result + 1);
			link_target[readlink_result] = '\0';
			break;
		};
		array_resize(link_target, buffer_size + 64);
	};
	return link_target;
};

void kga_symlink(const char *oldpath, const char *newpath) {
	if (symlink(oldpath, newpath)) throw_errno_verbose(newpath);
};

char **kga_glob(const char *pattern) {
	glob_t *glob_data = new(glob_t);
	int glob_ret;
	if ((glob_ret = glob(pattern, GLOB_ERR, NULL, glob_data))) {
		switch (glob_ret) {
		case GLOB_NOSPACE:
			throw(exception_type_glob_nospace, glob_ret, "GLOB_NOSPACE", pattern);
			break;
		case GLOB_ABORTED:
			throw(exception_type_glob_aborted, glob_ret, "GLOB_ABORTED", pattern);
			break;
		case GLOB_NOMATCH:
			throw(exception_type_glob_nomatch, glob_ret, "GLOB_NOMATCH", pattern);
			break;
		};
		throw(exception_type_glob_unknown, glob_ret, "GLOB_UNKNOWN_ERROR", pattern);
	};
	scope_add(glob_data, kga_free_glob);
	return glob_data->gl_pathv;
};

int kga_fprintf(FILE *file, const char *fmt, ...) {
	va_list args;
	int vprintf_ret;
	va_start(args, fmt);
	vprintf_ret = vfprintf(file, fmt, args);
	va_end(args);
	if (vprintf_ret < 0) throw_errno();
	return vprintf_ret;
};

int kga_file_exists(const char *path) {
	if (access(path, F_OK)) {
		if (errno == ENOENT) return 0;
		throw_errno_verbose(path);
	};
	return 1;
};
