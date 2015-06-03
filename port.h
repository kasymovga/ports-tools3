#include "shell.h"

#define PORT_BUILD_TIME 1
#define PORT_MARK_TO_PROCESS 2
#define PORT_FINISHED 4
#define PORT_HAVE_ERROR 8
#define PORT_ACTUAL 16
#define PORT_MARK_TO_BUILD 32
#define PORT_BUILD_TIME_NEEDED 64

int port_test_mode;
int (*port_confirm)(const char *fmt, ...);

struct port_db;
typedef struct port_db port_db_t;

struct port;

struct port {
	char *path;
	char *version;
	char *name;
	char *sources_name;
	char *sources_version;
	char *build;
	char **depends;
	char **optional_depends;
	char **version_depends;
	char **build_depends;
	struct port **all_depends;
	short keep_old;
	long int flags;
};

typedef struct port port_t;

port_db_t *port_db_new(const char *root, const char *path, const char *pkg_db_path, FILE *warning_stream);
void port_db_prepare(port_db_t *db);
const port_t *port_db_get_ports(port_db_t *db);

void port_db_lock(port_db_t *db);
void port_db_unlock(port_db_t *db);


//TODO
void port_db_target_add(port_db_t *db, const char *target);
void port_db_target_delete(port_db_t *db, const char *target);
void port_db_upgrade(port_db_t *db, char **need);
void port_db_targets_save(port_db_t *db);
