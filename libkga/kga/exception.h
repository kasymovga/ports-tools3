#ifndef _KGA_EXCEPTION_H_
#define _KGA_EXCEPTION_H_
#include <setjmp.h>

//For throw_errno macro
#include <errno.h>

//For throw_errno_verbose macro
#include <string.h>

//For FILE
#include <stdio.h>

//For malloc
#include <stdlib.h>

struct exception_type {
	void *dymmy;
};

typedef struct exception_type exception_type_t;

exception_type_t exception_type_memory;
exception_type_t exception_type_errno;
exception_type_t exception_type_unknown;
exception_type_t exception_type_null;
exception_type_t exception_type_important_check;

struct exception {
	int code;
	const char *func;
	const char *file;
	int line;
	const char *message;
	char *verbose;
	short success;
	exception_type_t *type;
	void *extra;
};

typedef struct exception exception_t;

const exception_t *exception();

int exception_debug(int debug);
int exception_grow();
int exception_init();
int exception_thread_init();
void exception_throw(exception_type_t *type, int code, const char *message, const char *file, const char *func, int line, char *verbose);
void exception_finish();
void exception_print(FILE *out);
void exception_snprint(char* out, size_t out_len);
jmp_buf *exception_jmpbuf();

static inline int exception_type_is_real(exception_type_t *type) {
	if (type == exception()->type) return 1;
	return 0;
};

static inline char *exception_strdup(const char *str) {
	if (!str) return NULL;
	int errno_copy = errno;
	char *str_copy = malloc(strlen(str));
	if (str_copy) strcpy(str_copy, str);
	errno = errno_copy;
	return str_copy;
};

#ifdef KGA_IMPORTANT_CHECK
#define important_check(x) if (!(x)) exception_throw(&exception_type_important_check, 1, "Important check failed", __FILE__, __func__, __LINE__, NULL);
#else
#define important_check(x) ;
#endif

#define try for (volatile int _exception_finish = 0 ; !_exception_finish && !setjmp(*exception_jmpbuf(__FILE__, __func__, __LINE__)) ; _exception_finish = 1, exception_finish() )


#define exception_type_is(x) exception_type_is_real(&x)
#define catch if ( !exception()->success )

#define throw(u,x,y,z)  exception_throw(&u, x, y, __FILE__, __func__, __LINE__, exception_strdup(z))
#define throw_errno()  exception_throw(&exception_type_errno, errno, strerror(errno), __FILE__, __func__, __LINE__, NULL)
#define throw_errno_with_type(x)  exception_throw(&x, errno, strerror(errno), __FILE__, __func__, __LINE__, NULL)
#define throw_errno_verbose(x) {\
	int _errno_copy=errno;\
	const char *_strerror_copy=strerror(errno);\
	char *_x_copy = malloc(strlen(x));\
	if (_x_copy) strcpy(_x_copy, x); \
	exception_throw(&exception_type_errno, _errno_copy, _strerror_copy, __FILE__, __func__, __LINE__, _x_copy);\
	}
#define throw_errno_with_type_verbose(y, x) {\
	int _errno_copy=errno;\
	const char *_strerror_copy=strerror(errno);\
	char *_x_copy = malloc(strlen(x));\
	if (_x_copy) strcpy(_x_copy, x); \
	exception_throw(&y, _errno_copy, _strerror_copy, __FILE__, __func__, __LINE__, _x_copy);\
	}
#define throw_proxy()  exception_throw(exception()->type, exception()->code, exception()->message, exception()->file, exception()->func, exception()->line, exception()->verbose)
#define throw_proxy_verbose(x)  exception_throw(exception()->type, exception()->code, exception()->message, exception()->file, exception()->func, exception()->line, exception_strdup(x))
#define throw_repeat() exception_throw(exception()->type, exception()->code, exception()->message, __FILE__, __func__, __LINE__, exception()->verbose)

#endif
