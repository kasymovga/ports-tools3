#define _XOPEN_SOURCE 500
#include <kga/exception.h>
#include <stdlib.h>

static inline void *c_malloc(size_t size) {
	void *ret = malloc(size);
	if(!ret) {
		throw_errno();
	};
	return ret;
};

static inline void *c_realloc(void *ptr, size_t size) {
	void *ret = realloc(ptr, size);
	if(!ret) {
		throw_errno();
	};
	return ret;
};
