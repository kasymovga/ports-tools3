#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <kga/array.h>
#include <kga/kga.h>
#include "misc.h"
#include "port.h"
#include "pkg.h"
#include "config.h"
#include "main_common.h"

static const char *root;
static const char *port_db_path;
static const char *pkg_db_path;

exception_type_t port_main_incorrect_cmd;

void usage(FILE *out) {
};

int main(int argc, char **argv) {
	root = "";
	port_db_path = PORT_DB_DEFAULT_PATH;
	pkg_db_path = PKG_DB_DEFAULT_PATH;
	array_max_memory_size = 1024 * 1024;
	array_max_size = 128 * 1024;
	int ret = EXIT_SUCCESS;
	int opt;
	kga_init();
	set_signal_handler(SIGPIPE, SIG_IGN);
	set_signal_handler(SIGHUP, interrupted);
	set_signal_handler(SIGINT, interrupted);
	set_signal_handler(SIGTERM, interrupted);
	try_scope {
		while ((opt = getopt(argc, argv, "r:p:ti")) != -1) {
			switch(opt) {
			case 'i':
				port_confirm = common_confirm;
				pkg_confirm = common_confirm;
				break;
			case 'r':
				root = optarg;
				break;
			case 'p':
				port_db_path = optarg;
				break;
			default:
				throw(port_main_incorrect_cmd, 1, "unknown option", NULL);
				break;
			};

		};
		int real_argc = argc - optind;
		char **real_argv = &argv[optind];

		if (real_argc == 0) {
			throw(port_main_incorrect_cmd, 1, "no subcommand", NULL);
		};

		if (!strcmp(real_argv[0], "help")) {
			usage(stdout);
			continue;
		} else if (!strcmp(real_argv[0], "depends")) {
			if (real_argc > 1) throw(port_main_incorrect_cmd, 1, "too many arguments", NULL);
			port_db_t *db = port_db_new(root, port_db_path, pkg_db_path, stderr);
			port_db_prepare(db);
			const port_t *ports = port_db_get_ports(db);
			size_t m, j;
			for (size_t i = 0, n = array_length(ports); i < n; i++) {
				printf("[");
				if (ports[i].flags & PORT_ACTUAL) {
					printf("A");
				};
				if (ports[i].flags & PORT_BUILD_TIME) {
					printf("B");
				};
				printf("]");
				printf("%s (%s/%s-%s)\n", ports[i].path, ports[i].name, ports[i].version, ports[i].build);
				m = array_length(ports[i].depends);
				if (m) {
					printf("                            Depends:");
					for (j = 0; j < m; j++) {
						printf(" %s", ports[i].depends[j]);
					};
					printf("\n");
				};
				m = array_length(ports[i].version_depends);
				if (m) {
					printf("                    Version depends:");
					for (j = 0; j < m; j++) {
						printf(" %s", ports[i].version_depends[j]);
					};
					printf("\n");
				};
				m = array_length(ports[i].optional_depends);
				if (m) {
					printf("                   Optional depends:");
					for (j = 0; j < m; j++) {
						printf(" %s", ports[i].optional_depends[j]);
					};
					printf("\n");
				};
				m = array_length(ports[i].build_depends);
				if (m) {
					printf("                      Build depends:");
					for (j = 0; j < m; j++) {
						printf(" %s", ports[i].build_depends[j]);
					};
					printf("\n");
				};
			};
		} else if (!strcmp(real_argv[0], "upgrade")) {
			port_db_t *db = port_db_new(root, port_db_path, pkg_db_path, stderr);
			port_db_prepare(db);
			if (real_argc - 1 > 0) {
				char *need[real_argc];
				memcpy(need, &(real_argv[1]), real_argc - 1);
				need[real_argc - 1] = NULL;
				port_db_upgrade(db, need);
			} else {
				port_db_upgrade(db, NULL);
			};
		} else if (!strcmp(real_argv[0], "add") && real_argc > 1) {
			port_db_t *db = port_db_new(root, port_db_path, pkg_db_path, stderr);
			for (int i = 1; i < real_argc; i++)
				port_db_target_add(db, real_argv[i]);
			port_db_targets_save(db);
		} else if (!strcmp(real_argv[0], "delete")) {
			port_db_t *db = port_db_new(root, port_db_path, pkg_db_path, stderr);
			for (int i = 1; i < real_argc; i++)
				port_db_target_delete(db, real_argv[i]);
			port_db_targets_save(db);
		} else {
			throw(port_main_incorrect_cmd, 1, "unknown subcommand", NULL);
		};
	};
	catch {
		if (exception()->type == &port_main_incorrect_cmd) {
			fprintf(stderr, "Invalid command, %s.\n", exception()->message);
			usage(stderr);
		} else {
			exception_print(stderr);
		};
		ret = EXIT_FAILURE;
	};
	return ret;
};
