#ifndef _PKG_H_
#define _PKG_H_

#include <kga/exception.h>

#define PKG_UPGRADE 1
#define PKG_FORCE_INSTALL 2

exception_type_t exception_type_pkg_files_conflict;
exception_type_t exception_type_pkg_db_already_locked;
exception_type_t exception_type_pkg_already_installed;

int (*pkg_confirm)(const char *fmt, ...);

struct pkg_list_item {
	const char *name, *version;
};

void pkg_install(const char *pkg_path, const char *root, const char *db_path, int flags, FILE *warning_stream);
void pkg_finish_installs(const char *root);
void pkg_drop(const char *root, const char *db_path, const char *name, const char *version, FILE *warning_stream);
int pkg_installed(const char *pkg_root, const char *db_path, const char *name, const char *version);
struct pkg_list_item *pkg_db_list(const char *pkg_root, const char *db_path);
#endif
