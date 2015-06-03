#ifndef _KGA_SCOPE_H_
#define _KGA_SCOPE_H_

#include <kga/exception.h>
#include <alloca.h>
#include <stdlib.h>

struct scope_pool;
typedef struct scope_pool scope_pool_t;

scope_pool_t *scope_pool_new(int flags);

void scope_pool_add(scope_pool_t *pool, void *ptr, void (*destructor)(void *ptr));
void scope_pool_free(scope_pool_t *pool);

scope_pool_t *scope_set(scope_pool_t *pool);
scope_pool_t *scope_current();
scope_pool_t *scope_pool_previous(scope_pool_t *pool);

#define SCOPE_POOL_SET_AS_CURRENT 1

void scope_add(void *ptr, void (*destructor)(void *ptr));

static inline scope_pool_t *scope_start() { return scope_pool_new(SCOPE_POOL_SET_AS_CURRENT); };
static inline void scope_end() { scope_pool_free(scope_current()); };

static inline void *scope_malloc(size_t size) {
	void *ret = malloc(size);
	if (!ret) throw_errno();
	scope_pool_add(scope_current(), ret, free);
	return ret;
};

#endif
