#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

exception_type_t exception_type_fopen_no_such_file;
exception_type_t exception_type_mkdir_already_exists;
exception_type_t exception_type_glob_nospace;
exception_type_t exception_type_glob_aborted;
exception_type_t exception_type_glob_nomatch;

int kga_fork();
int *kga_pipe();
void kga_dup2(int oldfd, int newfd);
void kga_mkdir(const char *path, mode_t mode);
void kga_rmdir(const char *path);
void *kga_malloc(size_t size);
FILE *kga_fdopen(int fd, const char *opt);
FILE *kga_fopen(const char *path, const char *opt);
size_t kga_fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t kga_fwrite(void *ptr, size_t size, size_t nmemb, FILE *stream);
DIR *kga_opendir(const char *path);
void kga_mkpath(const char *path, mode_t mode);
void kga_chdir(const char *path);
void kga_mkdtemp(char *template);
void kga_chown(const char *path, uid_t uid, gid_t gid);
void kga_rename(const char *old_path, const char *new_path);
char **kga_glob(const char *pattern);
void kga_lstat(const char *path, struct stat *stat);
int kga_lstat_skip_enoent(const char *path, struct stat *stat);
void kga_fflush_and_fsync(FILE *file);
char *kga_readlink(const char *path);
void kga_symlink(const char *oldpath, const char *newpath);
int kga_fprintf(FILE *file, const char *fmt, ...);
int kga_file_exists(const char *path);
