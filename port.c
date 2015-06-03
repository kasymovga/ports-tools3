#define _POSIX_C_SOURCE 200112L
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <kga/kga.h>
#include <kga/scope.h>
#include <kga/string.h>
#include <kga/array.h>
#include "config.h"
#include "port.h"
#include "pkg.h"
#include "misc.h"
#include "kga_wrappers.h"
//#include "build_script.sh.h"
#include "shell.h"

exception_type_t port_aborted_by_user = {};
int (*port_confirm)(const char *fmt, ...) = NULL;

struct port_db {
	scope_pool_t *scope_pool;
	const char *root;
	const char *path;
	const char *pkg_db_path;
	const char *build_template;
	char **ignored_depends;
	char *arch;
	char **targets;
	//char **old_depends;
	struct port *ports;
	shell_t *shell;
	FILE *warning_stream;
};

static void port_db_free(void *ptr) {
	port_db_t *db = ptr;
	scope_pool_free(db->scope_pool);
	free(ptr);
};

port_db_t *port_db_new(const char *root, const char *path, const char *pkg_db_path, FILE *warning_stream) {
	important_check(root);
	important_check(path);
	struct port_db *db = kga_malloc(sizeof(struct port_db));
	db->scope_pool = NULL;
	scope_add(db, port_db_free);
	db->scope_pool = scope_pool_new(0);
	db->root = root;
	db->path = path;
	db->warning_stream = warning_stream;
	scope {
		char *targets_path = string_new_fmt("%s/%s/targets", db->root, db->path);
		scope_use(db->scope_pool) {
			db->build_template = string_new_fmt("%s/build_template.sh", path);
			db->shell = shell_new();
			db->pkg_db_path = pkg_db_path;
			struct utsname utsname;
			uname(&utsname);
			db->arch = string_new();
			string_set(db->arch, utsname.machine);
			db->targets = NULL;
			try db->targets = file_lines(targets_path);
			catch if (exception()->type != &exception_type_fopen_no_such_file) throw_proxy();
		};
	};
	return db;
};

port_t *port_db_get_port(port_db_t *db, const char *port_path) {
	important_check(db);
	important_check(port_path);
	for (int i = 0, n = array_length(db->ports); i < n; i++) {
		if (!strcmp(port_path, db->ports[i].path)) {
			return &db->ports[i];
		};
	};
	return NULL;
};

port_t *port_db_get_port_by_name(port_db_t *db, const char *port_name) {
	important_check(db);
	important_check(port_name);
	for (int i = 0, n = array_length(db->ports); i < n; i++) {
		if (!strcmp(port_name, db->ports[i].name)) {
			return &db->ports[i];
		};
	};
	return NULL;
};

void port_db_load_port(port_db_t *db, const char *port_path, int flags) {
	important_check(db);
	important_check(port_path);
	important_check(*port_path);
	if (port_db_get_port(db, port_path)) return;
	scope {
		char *port_script_path = string_new_fmt("/%s/pkgblds/%s/build.sh", db->path, port_path);
		char *prepare_script = string_new_fmt(
				"PORT_NAME='%s'"
				"NAME=''\n"
				"VERSION=''\n"
				"SOURCES_NAME=''\n"
				"SOURCES_VERSION=''\n"
				"BUILD=''\n"
				"DEPENDS=''\n"
				"OPTIONAL_DEPENDS=''\n"
				"BUILD_DEPENDS=''\n"
				"VERSION_DEPENDS=''\n"
				"KEEPOLD=''\n"
				"planned() {\n"
				"return 1\n"
				"}\n"
				"version() {\n"
				"return 1\n"
				"}\n",
				port_path);
		shell_process(db->shell, prepare_script);
		shell_process_file(db->shell, port_script_path);
		shell_process(db->shell,
				"if test -z \"$SOURCES_NAME\"\n"
				"then\n"
				"SOURCES_NAME=\"$NAME\"\n"
				"fi\n"
				"if test -z \"$SOURCES_VERSION\"\n"
				"then\n"
				"SOURCES_VERSION=\"$VERSION\"\n"
				"fi\n"
			     );
		char *depends = shell_get_var(db->shell, "DEPENDS");
		char *optional_depends = shell_get_var(db->shell, "OPTIONAL_DEPENDS");
		char *version_depends = shell_get_var(db->shell, "VERSION_DEPENDS");
		char *build_depends = shell_get_var(db->shell, "BUILD_DEPENDS");
		char *keep_old = shell_get_var(db->shell, "KEEPOLD");
		struct port port;
		scope_use(db->scope_pool) {
			port.path = string_new();
			string_set(port.path, port_path);
			port.name = shell_get_var(db->shell, "NAME");
			port.sources_name = shell_get_var(db->shell, "SOURCES_NAME");
			port.version = shell_get_var(db->shell, "VERSION");
			port.sources_version = shell_get_var(db->shell, "SOURCES_VERSION");
			port.depends = string_split(depends, " ", STRING_SPLIT_WITHOUT_EMPTY);
			port.version_depends = string_split(version_depends, " ", STRING_SPLIT_WITHOUT_EMPTY);
			port.optional_depends = string_split(optional_depends, " ", STRING_SPLIT_WITHOUT_EMPTY);
			port.build_depends = string_split(build_depends, " ", STRING_SPLIT_WITHOUT_EMPTY);
			if (keep_old && !strcmp(keep_old, "y")) {
				port.keep_old = 1;
			} else {
				port.keep_old = 0;
			};
			//too early calculate BUILD, make that later
			port.build = NULL;
			port.flags = flags;
			port.all_depends = array_new(struct port_depend *, 0);
			array_push(db->ports, port);
			for (int i = 0, n = array_length(port.depends); i < n; i++) {
				port_db_load_port(db, port.depends[i], flags);
			};
			if (flags & PORT_BUILD_TIME) {
				array_foreach(port.build_depends, char **, each_depend) {
					port_db_load_port(db, *each_depend, flags);
				};
			};
		};
	};
};

int port_db_is_ignored_depend(port_db_t *db, const char *depend) {
	int ret = 0;
	array_foreach(db->ignored_depends, char **, each_ignored_depend) {
		if (!strcmp(depend, *each_ignored_depend)) {
			ret = 1;
			break;
		};
	};
	return ret;
};

char *port_calculate_build_script(port_t *port, port_db_t *db) {
	char *script = string_new();
	string_fmt(script, "%splanned() {\n", script);
	array_foreach(port->optional_depends, char **, each_opt_dep) {
		if (port_db_is_ignored_depend(db, *each_opt_dep)) continue;
		if (port_db_get_port(db, *each_opt_dep)) {
			string_fmt(script, "%stest \"$1\" = '%s' && return 0\n", script, *each_opt_dep);
		};
	};
	string_fmt(script, "%sreturn 1\n}\n", script);
	string_fmt(script, "%sversion() {\n", script);
	port_t *dep_port;
	array_foreach(port->version_depends, char **, each_ver_dep) {
		if ((dep_port = port_db_get_port(db, *each_ver_dep))) {
			string_fmt(script, "%stest \"$1\" = '%s' && echo '%s'\n", script, dep_port->path, dep_port->version);
		};
	};
	string_fmt(script, "%secho -n\n}\n", script);
	return script;
};

void port_calculate_build(port_t *port, port_db_t *db) {
	scope {
		char *port_script_path = string_new_fmt("/%s/pkgblds/%s/build.sh", db->path, port->path);
		shell_process(db->shell, "BUILD=''\n");
		shell_process(db->shell, port_calculate_build_script(port, db));
		shell_process_file(db->shell, port_script_path);
		scope_use(db->scope_pool) {
			port->build = shell_get_var(db->shell, "BUILD");
		};
	};
};

void port_db_prepare(port_db_t *db) {
	important_check(db);
	scope {
		char *prepare_script = string_new();
		string_fmt(prepare_script,
			"IGNORED_DEPENDS=''\n"
			"ROOTDIR=%s\n"
			"PORTSBASEDIR=%s\n"
			"PORTS_ARCH=%s\n"
			"CONFPATH=\"/$PORTSBASEDIR/ports.conf\"\n"
			"CONFSDIR=\"/$PORTSBASEDIR/ports.conf.d\"\n"
			"SCRIPTSDIR=\"/$PORTSBASEDIR/pkgblds-scripts\"\n"
			"test -f \"$CONFPATH\" && . \"$CONFPATH\" >&2\n",
			db->root,
			db->path,
			db->arch);
		shell_process(db->shell, prepare_script);
		char *ignored_depends = shell_get_var(db->shell, "IGNORED_DEPENDS");
		db->ignored_depends = NULL;
		scope_use(db->scope_pool) {
			db->ignored_depends = string_split(ignored_depends, " ", STRING_SPLIT_WITHOUT_EMPTY);
			db->ports = array_new(port_t, ARRAY_NULL_TERMINATED);
			if (db->targets) {
				for (size_t i = 0, n = array_length(db->targets); i < n; i++) {
					port_db_load_port(db, db->targets[i], 0);
				};
				for (size_t i = 0, j, m; i < array_length(db->ports); i++) {
					for (j = 0, m = array_length(db->ports[i].build_depends); j < m; j++) {
						port_db_load_port(db, db->ports[i].build_depends[j], PORT_BUILD_TIME);
					};
				};
			};
			for (size_t i = 0, n = array_length(db->ports); i < n; i++) {
				port_calculate_build(&db->ports[i], db);
			};
		};
		char *version_build = string_new();
		array_foreach(db->ports, struct port *, port) {
			string_fmt(version_build, "%s-%s-%s", port->version, port->build, db->arch);
			if ((!port->version || !*port->version) || pkg_installed(db->root, db->pkg_db_path, port->name, version_build)) {
				port->flags |= PORT_ACTUAL;
#if 0
			} else {
				if (!(port->flags & PORT_BUILD_TIME))
					fprintf(db->warning_stream, "%s/%s not actual\n", port->name, version_build);
#endif
			};
		};
	};
};

void port_write_script(port_db_t *db, port_t *port, const char *script_path) {
	scope {
		FILE *file = kga_fopen(script_path, "w");
		kga_fprintf(file, "PORTSROOT='/%s'\n", db->path);
		kga_fprintf(file, "PORT_PATH='%s'\n%s", port->path, port_calculate_build_script(port, db));
		kga_fprintf(file, ". /%s \"$@\"\n", db->build_template);
	};
};

int port_run_script(port_db_t *db, port_t *port, const char *cmd) {
	int status = -1;
	scope {
		char *tmp_path = string_new_fmt("/%s/tmp/bld.XXXXXX", db->path);
		kga_mkdtemp(tmp_path);
		kga_chown(tmp_path, PORT_UID, PORT_GID);
		char *script_path = string_new_fmt("%s/script.sh", tmp_path);
		port_write_script(db, port, script_path);
		if (port_confirm && !port_confirm("Do you want run script %s?", script_path)) {
			throw(port_aborted_by_user, 1, "Aborted by user", NULL);
		};
		pid_t script_pid = kga_fork();
		if (!script_pid) {
			char script_path_copy[string_length(script_path) + 1];
			char cmd_copy[strlen(cmd) + 1];
			strcpy(script_path_copy, script_path);
			strcpy(cmd_copy, cmd);
			setenv("HOME", tmp_path, 1);
			kga_chdir(tmp_path);
			while (scope_current()) scope_end();
			setuid(PORT_UID);
			setgid(PORT_GID);
			execl("/bin/sh", "/bin/sh", script_path_copy, cmd_copy, NULL);
			fprintf(stderr, "exec: %s: %s\n", script_path, strerror(errno));
			exit(EXIT_FAILURE);
		};
		fprintf(db->warning_stream, "Waiting script pid %li\n", (long int)script_pid);
		waitpid(script_pid, &status, 0);
		if (!status) {
			char *pkg_path = string_new_fmt("%s/fr", tmp_path);
			try {
				if (port->keep_old) {
					if (!port_confirm || port_confirm("Install package %s/%s from %s?", port->name, port->version, pkg_path)) {
						pkg_install(pkg_path, db->root, db->pkg_db_path, 0, db->warning_stream);
					} else {
						status = -1;
					};
				} else {
					if (!port_confirm || port_confirm("Upgrade package %s/%s from %s?", port->name, port->version, pkg_path)) {
						pkg_install(pkg_path, db->root, db->pkg_db_path, PKG_UPGRADE, db->warning_stream);
					} else {
						status = -1;
					};
				};
			};
			catch {
				exception_print(stderr);
				status = -1;
			};
		};
		rmrf(tmp_path);
	};
	return status;
};

void port_db_upgrade(port_db_t *db, char **need) {
	important_check(db);
	if (need) {
		port_t *port;
		for(char **needed_port = need; *needed_port; needed_port++) {
			if ((port = port_db_get_port(db, *needed_port))) {
				port->flags |= PORT_MARK_TO_PROCESS;
			};
		};
	} else {
		array_foreach(db->ports, struct port *, each_port) {
			each_port->flags |= PORT_MARK_TO_PROCESS;
		};
	};
	port_t *depend_port; 
	array_foreach(db->ports, struct port *, each_port) {
		array_foreach(each_port->depends, char **, each_depend) {
			if ((depend_port = port_db_get_port(db, *each_depend))) {
				array_push(each_port->all_depends, depend_port);
			};
		};
		array_foreach(each_port->build_depends, char **, each_depend) {
			if ((depend_port = port_db_get_port(db, *each_depend))) {
				array_push(each_port->all_depends, depend_port);
			};
		};
		array_foreach(each_port->optional_depends, char **, each_depend) {
			if (port_db_is_ignored_depend(db, *each_depend)) continue;
			if ((depend_port = port_db_get_port(db, *each_depend))) {
				array_push(each_port->all_depends, depend_port);
			};
		};
	};

	scope {
		char *version_build = string_new();
		for(int changed = 1; changed; ) {
			changed = 0;
			array_foreach(db->ports, struct port *, port) {
				if (!(port->flags & PORT_MARK_TO_PROCESS) || (port->flags & PORT_FINISHED) || (port->flags & PORT_HAVE_ERROR)) continue;

				array_foreach(port->all_depends, struct port **, each_depend) {
					if (!((*each_depend)->flags & PORT_MARK_TO_PROCESS)) {
						(*each_depend)->flags |= PORT_MARK_TO_PROCESS;
						changed = 1;
					};
				};

				if (!(port->flags & PORT_ACTUAL)) {
					string_fmt(version_build, "%s-%s-%s", port->version, port->build, db->arch);
					if ((!port->version || !*port->version) || pkg_installed(db->root, db->pkg_db_path, port->name, version_build)) {
						port->flags |= PORT_ACTUAL;
						changed = 1;
					};
				};


				if (!(port->flags & PORT_FINISHED) && (port->flags & PORT_ACTUAL)) {
#if 0
					fprintf(db->warning_stream, "Check if %s can be finished cause actual.\n", port->name);
#endif
					int port_is_finished = 1;
					array_foreach (port->all_depends, struct port **, each_depend) {
						if ((*each_depend)->flags & PORT_BUILD_TIME) continue;
						if (!((*each_depend)->flags & PORT_FINISHED)) {
							port_is_finished = 0;
#if 0
							fprintf(db->warning_stream, "Port %s have unfinished depend: %s.\n", port->name, (*each_depend)->name);
#endif
							break;
						};
					};
					if (port_is_finished) {
#if 0
						fprintf(db->warning_stream, "Port %s finished.\n", port->name);
#endif
						port->flags |= PORT_FINISHED;
						changed = 1;
					};
				};

				if (!(port->flags & PORT_ACTUAL) && !(port->flags & PORT_MARK_TO_BUILD) && (!(port->flags & PORT_BUILD_TIME) || (port->flags & PORT_BUILD_TIME_NEEDED)) ) {
					int port_ready_to_update = 1;
					array_foreach (port->all_depends, struct port **, each_depend) {
						if ((*each_depend)->flags & PORT_BUILD_TIME) continue;
						if (!((*each_depend)->flags & PORT_FINISHED)) {
							port_ready_to_update = 0;
							break;
						};
					};
					if (port_ready_to_update) {
						fprintf(db->warning_stream, "Trying get package for %s.\n", port->name);
						if (port_run_script(db, port, "get_package")) {
							port->flags |= PORT_MARK_TO_BUILD;
						};
						changed = 1;
					};
				};

				if (!(port->flags & PORT_ACTUAL) && port->flags & PORT_MARK_TO_BUILD) {
					int port_ready_to_build = 1;
					if (!db->root || !*db->root) {
						array_foreach (port->all_depends, struct port **, each_depend) {
							if (!(*each_depend)->flags & PORT_BUILD_TIME) continue;
							if (!((*each_depend)->flags & PORT_BUILD_TIME_NEEDED)) {
								(*each_depend)->flags |= PORT_BUILD_TIME_NEEDED;
								changed = 1;
							};
							if (!((*each_depend)->flags & PORT_FINISHED)) {
								port_ready_to_build = 0;
							};
						};
					};
					if (port_ready_to_build) {
						if (port_run_script(db, port, "build_package")) {
							port->flags |= PORT_HAVE_ERROR;
						};
						changed = 1;
					};
				};

			};
		};
		if (db->warning_stream) {
			array_foreach(db->ports, struct port *, port) {
				if (!(port->flags & PORT_ACTUAL) && !(port->flags & PORT_BUILD_TIME)) {
					fprintf(db->warning_stream, "Package not actual: %s\n", port->name);
				};
			};
		};
		struct pkg_list_item *pkg_list = pkg_db_list(db->root, db->pkg_db_path);
		if (!db->root || !*db->root) {
			struct port *port;
			array_foreach(pkg_list, struct pkg_list_item *, each_pkg) {
				if (!(port = port_db_get_port_by_name(db, each_pkg->name)) || (port->flags & PORT_BUILD_TIME)) {
					if (!port_confirm || port_confirm("Drop package %s?", each_pkg->name)) {
						if (db->warning_stream) fprintf(db->warning_stream, "Remove package: %s\n", each_pkg->name);
						pkg_drop(db->root, db->pkg_db_path, each_pkg->name, NULL, db->warning_stream);
					};
				};
			};
		};
	};
};

const port_t *port_db_get_ports(port_db_t *db) {
	important_check(db);
	return db->ports;
};

void port_db_targets_save(port_db_t *db) {
	scope {
		char *targets_path = string_new_fmt("/%s/%s/targets", db->root, db->path);
		char *targets_new_path = string_new_fmt("%s.new", targets_path);
		lines_to_file(db->targets, targets_new_path);
		kga_rename(targets_new_path, targets_path);
	};
};

void port_db_target_add(port_db_t *db, const char *target) {
	array_foreach(db->targets, char **, each_target) {
		if (!strcmp(*each_target, target)) return;
	};
	array_push(db->targets, target);
};

void port_db_target_delete(port_db_t *db, const char *target) {
	for (size_t i = 0; i < array_length(db->targets); i++) {
		if (!strcmp(db->targets[i], target)) {
			array_delete_interval(db->targets, i, 1);
			i--;
		};
	};
};

