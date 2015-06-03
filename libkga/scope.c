#ifdef _PTHREAD_ENABLE
#include <pthread.h>
#endif
#include <c.h>
#include <kga/kga.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include "exceptions.h"

#define SCOPE_POOL_DELTA 64

struct scope_pool_item {
	void *i;
	void (*destructor)(void *i);
};

struct scope_pool {
	size_t n, s;
	struct scope_pool_item *i;
	struct scope_pool *previous;
};

struct thread_local_data {
	scope_pool_t *current_pool;
};

#ifdef _PTHREAD_ENABLE
static pthread_once_t thread_once = PTHREAD_ONCE_INIT;
static pthread_key_t thread_key;
#else
static struct thread_local_data nonthread_local_data = {
	.current_pool = NULL
};
#endif

#ifdef _PTHREAD_ENABLE
static void thread_local_data_free(void *ptr) {
	struct thread_local_data *thread_local_data = ptr;
	free(thread_local_data);
};

static void thread_init() {
	if (pthread_key_create(&thread_key, thread_local_data_free)) throw_errno();
};
#endif

static struct thread_local_data* get_thread_local_data() {
#ifdef _PTHREAD_ENABLE
	if (pthread_once(&thread_once, thread_init)) throw_errno();
	struct thread_local_data *thread_local_data = pthread_getspecific(thread_key);
	if (!thread_local_data) {
		thread_local_data = malloc(sizeof(struct thread_local_data));
		if (thread_local_data) {
			thread_local_data->current_pool = NULL;
			pthread_setspecific(thread_key, thread_local_data);
		};
	};
	if (!thread_local_data) {
		throw_errno();
	};
	return thread_local_data;
#else
	return &nonthread_local_data;
#endif
};

static scope_pool_t *get_thread_current_pool() {
	return get_thread_local_data()->current_pool;
};

static void set_thread_current_pool(scope_pool_t *pool) {
	get_thread_local_data()->current_pool = pool;
};

void scope_pool_free(scope_pool_t *pool) {
	if (!pool) return;
	if (get_thread_current_pool() == pool)
		set_thread_current_pool(get_thread_current_pool()->previous);
	for(size_t i = pool->n; i; ) {
		i--;
		if (pool->i[i].destructor) {
			pool->i[i].destructor(pool->i[i].i);
			if (kga_debug)
				fprintf(stderr, "Pool %lli : Call destructor %lli for %lli\n", (long long int)pool, (long long int)pool->i[i].destructor, (long long int)pool->i[i].i);
		};
	};
	free(pool->i);
	if (kga_debug)
		fprintf(stderr, "Pool %lli destroyed\n", (long long int)pool);
	free(pool);
};

scope_pool_t *scope_pool_new(int flags) {
	scope_pool_t *pool = NULL;
	pool = c_malloc(sizeof(scope_pool_t));
	if (flags & SCOPE_POOL_SET_AS_CURRENT) {
		*pool = (struct scope_pool){ .n = 0, .s = 0, .i = NULL, .previous = get_thread_current_pool() };
		set_thread_current_pool(pool);
	} else {
		*pool = (struct scope_pool){ .n = 0, .s = 0, .i = NULL, .previous = NULL };
	};
	if (kga_debug)
		fprintf(stderr, "Pool %lli created\n", (long long int)pool);
	return pool;
};

void scope_add(void *ptr, void (*destructor)(void *ptr)) {
	if (!get_thread_current_pool()) return;
	if (!ptr) return;
	if (!destructor) throw(exception_type_null, EINVAL, second_arg_is_null, NULL);
	scope_pool_add(get_thread_current_pool(), ptr, destructor);
};

void scope_pool_add(scope_pool_t *pool, void *ptr, void (*destructor)(void *ptr)) {
	if (!pool) throw(exception_type_null, EINVAL, first_arg_is_null, NULL);
	if (pool->n >= pool->s) {
		size_t new_s;
		for (new_s = (pool->s > 0 ? pool->s : 1); new_s <= pool->n; new_s *= 2);
		void *new_i = NULL;
		try new_i = c_realloc(pool->i, new_s * sizeof(struct scope_pool_item));
		catch {
			destructor(ptr);
			throw_errno();
		};
		pool->i = new_i;
		pool->s = new_s;
		if (kga_debug)
			fprintf(stderr, "Pool %lli size %zi\n", (long long int)pool, pool->s);
	};
	pool->i[pool->n].i = ptr;
	pool->i[pool->n].destructor = destructor;
	pool->n++;
};

scope_pool_t *scope_set(scope_pool_t *pool) {
	scope_pool_t *old_pool = get_thread_current_pool();
	set_thread_current_pool(pool);
	return old_pool;
};

scope_pool_t *scope_pool_previous(scope_pool_t *pool) {
	if (!pool) return NULL;
	return pool->previous;
};

scope_pool_t *scope_current() {
	return get_thread_current_pool();
};

#if 0
scope_free(void *ptr) {
	for(size_t i=current_pool->n-1; i>=0; i--) {
		if (current_pool->i[i].destructor && current_pool->i[i].destructor=ptr)
			current_pool->i[i].destructor(current_pool->i[i].i);
			current_pool->i[i].destructor=NULL;
			current_pool->i[i].ptr=NULL;
	};
};
#endif
