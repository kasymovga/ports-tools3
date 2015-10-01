#include <kga/array.h>
#include <kga/kga.h>
#include <stdlib.h>
#include <getopt.h>
#include <signal.h>
#include "pkg.h"
#include "misc.h"
#include "config.h"
#include "main_common.h"

exception_type_t pkg_main_incorrect_cmd;

static const char *root = "";
static const char *db_path = PKG_DB_DEFAULT_PATH;

void usage(FILE *out) {
};

int main(int argc, char **argv) {
	int ret = EXIT_SUCCESS;
	int opt;
	kga_init();
	set_signal_handler(SIGHUP, interrupted);
	set_signal_handler(SIGINT, interrupted);
	set_signal_handler(SIGTERM, interrupted);
	try_scope {
		while ((opt = getopt(argc, argv, "r:d:ti")) != -1) {
			switch(opt) {
			case 'i':
				pkg_confirm = common_confirm;
				break;
			case 'r':
				root = optarg;
				break;
			case 'd':
				db_path = optarg;
				break;
			default:
				throw(pkg_main_incorrect_cmd, 1, "unknown option", NULL);
				break;
			};
		};
		int real_argc = argc - optind;
		char **real_argv = &argv[optind];
		if (!real_argc) {
			throw(pkg_main_incorrect_cmd, 1, "no subcommand", NULL);
		} else if (!strcmp(real_argv[0], "install")) {
			if (real_argc != 2) {
				throw(pkg_main_incorrect_cmd, 1, "incorrect subcommand", NULL);
			};
			pkg_install(real_argv[1], root, db_path, 0, stderr);
		} else if (!strcmp(real_argv[0], "upgrade")) {
			if (real_argc != 2) {
				throw(pkg_main_incorrect_cmd, 1, "incorrect subcommand", NULL);
			};
			pkg_install(real_argv[1], root, db_path, PKG_UPGRADE, stderr);
		} else if (!strcmp(real_argv[0], "drop")) {
			if (real_argc < 2 || real_argc > 3) {
				throw(pkg_main_incorrect_cmd, 1, "incorrect subcommand", NULL);
			};
			char *slash;
			if ((slash = strchr(real_argv[1], '/'))) {
				*slash = '\0';
				pkg_drop(root, db_path, real_argv[1], &slash[1], stderr);
				
			} else {
				pkg_drop(root, db_path, real_argv[1], real_argc == 2 ? NULL : real_argv[2], stderr);
			}
		} else {
			throw(pkg_main_incorrect_cmd, 1, "unknown subcommand", NULL);
		};
	};
	catch {
		if (exception()->type == &pkg_main_incorrect_cmd) {
			fprintf(stderr, "Invalid command, %s.\n", exception()->message);
			usage(stderr);
		} else {
			exception_print(stderr);
		};
		ret = EXIT_FAILURE;
	};
	return ret;
};
