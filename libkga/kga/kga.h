#ifndef _KGA_H_
#define _KGA_H_
#include <kga/exception.h>
#include <kga/scope.h>

int kga_debug;

static inline int kga_init() { return exception_init(); };
static inline int kga_thread_init() { return exception_thread_init(); };

//Just little hacks for macro
static inline int _scope_end_ret0() { scope_pool_free(scope_current()); return 0; };
static inline int _scope_rollback(scope_pool_t *pool) { scope_set(pool); return 0; };
static inline int _exception_throw_ret0(exception_type_t *type, int code, const char *message, const char *file, const char *func, int line, char *verbose) {
	exception_throw(type, code, message, file, func, line, verbose);
	return 0;
};

#define try_scope \
		for (volatile int _scope_created = 0, _exception_finish = 0 ; \
		(!_exception_finish  || (_scope_created && _scope_end_ret0())) && \
		!exception_grow() && \
		(!setjmp(*exception_jmpbuf(__FILE__, __func__, __LINE__)) || (_scope_created && _scope_end_ret0())) \
		&& scope_start() \
		&& (_scope_created = 1); \
		_exception_finish = 1, exception_finish() )

#define scope \
		for (volatile int _scope_created = 0, _exception_finish = 0 ; \
		(!_exception_finish  || (_scope_created && _scope_end_ret0())) && \
		!exception_grow() && \
		(!setjmp(*exception_jmpbuf(__FILE__, __func__, __LINE__)) || (_scope_created && _scope_end_ret0()) || \
		_exception_throw_ret0(exception()->type, exception()->code, exception()->message, exception()->file, exception()->func, exception()->line, exception()->verbose)) \
		&& scope_start() \
		&& (_scope_created = 1); \
		_exception_finish = 1, exception_finish() )

#define scope_use(x) \
		volatile scope_pool_t *_prev_pool ## __LINE__ = scope_set(x); \
		for(volatile int _exception_finish = 0; \
		!_exception_finish && \
		(!setjmp(*exception_jmpbuf(__FILE__, __func__, __LINE__)) || _scope_rollback((scope_pool_t *)_prev_pool ## __LINE__) || \
		_exception_throw_ret0(exception()->type, exception()->code, exception()->message, exception()->file, exception()->func, exception()->line, exception()->verbose)); \
		_exception_finish = 1, scope_set((scope_pool_t *)_prev_pool ## __LINE__), exception_finish() )

#define try_with_scope_pool(x) \
		volatile scope_pool_t *_prev_pool ## __LINE__ = scope_set(x); \
		for(volatile int _exception_finish = 0; \
		!_exception_finish && \
		(!setjmp(*exception_jmpbuf(__FILE__, __func__, __LINE__)) || _scope_rollback((scope_pool_t *)_prev_pool ## __LINE__)); \
		_exception_finish = 1, scope_set((scope_pool_t *)_prev_pool ## __LINE__), exception_finish() )

#ifdef KGA_TRACE
#define kga_trace \
		for (volatile int _exception_finish = 0 ; \
		!_exception_finish  && \
		(!setjmp(*exception_jmpbuf(__FILE__, __func__, __LINE__)) || \
		_exception_throw_ret0(exception()->type, exception()->code, exception()->message, exception()->file, exception()->func, exception()->line, exception()->verbose)) ; \
		_exception_finish = 1, exception_finish() )
#else
#define kga_trace ;
#endif

 
#define try_with_scope_previous_pool try_with_scope_pool(scope_pool_previous(scope_current()))
#define scope_use_previous scope_use(scope_pool_previous(scope_current()))

#define new(x) scope_malloc(sizeof(x));

#endif
