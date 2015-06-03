#ifdef _PTHREAD_ENABLE
#include <pthread.h>
#endif
#include <setjmp.h>
#include <sys/types.h>
#include <kga/kga.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct try_item {
	jmp_buf jmp;
	const char *file, *func;
	int line;
	void (*callback)(void *callback_data);
	void *callback_data;
};

struct try_stack {
	struct try_item *i;
	size_t n, s, top;
};

#define JMP_DELTA 16

#define EXCEPTION_INIT_FAIL { .code = ENOMEM, .message = "Init fail", .verbose = NULL, .success = 0 , .file = "unknown", .func = "unknown", .type = &exception_type_init_fail}
#define EMPTY_TRY_STACK { .i = NULL, .n = 0, .s = 0, .top = 0 }

struct thread_local_data {
	struct exception exception;
	struct try_stack try_stack;
};

exception_type_t exception_type_init_fail;
exception_type_t exception_type_memory;
exception_type_t exception_type_unknown;
exception_type_t exception_type_errno;
exception_type_t exception_type_null;
exception_type_t exception_type_important_check;

//struct exception empty_exception = EXCEPTION_INIT_FAIL;


#ifdef _PTHREAD_ENABLE
static pthread_key_t thread_key;
#else
struct thread_local_data nonthread_local_data;
#endif

#ifdef _PTHREAD_ENABLE
static void thread_local_data_free(void *ptr) {
	struct thread_local_data *thread_local_data = ptr;
	if (thread_local_data) {
		free(thread_local_data->try_stack.i);
		free(thread_local_data->exception.verbose);
		free(thread_local_data);
	};
};

static int thread_init() {
	int ret = -1;
	struct thread_local_data *thread_local_data = malloc(sizeof(struct thread_local_data));
	if (!thread_local_data) goto finish;
	thread_local_data->try_stack = (struct try_stack)EMPTY_TRY_STACK;
	thread_local_data->exception = (struct exception)EXCEPTION_INIT_FAIL;
	if (pthread_setspecific(thread_key, thread_local_data)) {
		thread_local_data_free(thread_local_data);
		goto finish;
	};
	ret = 0;
finish:
	return ret;
};
#endif

static struct thread_local_data* get_thread_local_data() {
#ifdef _PTHREAD_ENABLE
	struct thread_local_data* thread_local_data = pthread_getspecific(thread_key);
	if (!thread_local_data) {
		fprintf(stderr, "Oh fuck, thread_local_data is NULL! Did somebody don't make kga_init()? Calling exit(EXIT_FAILURE)\n");
		exit(EXIT_FAILURE);
	};
	return thread_local_data;
#else
	return &nonthread_local_data;
#endif
};

static struct try_stack *get_thread_try_stack() {
	struct thread_local_data *thread_local_data = get_thread_local_data();
	if (thread_local_data) {
		return &thread_local_data->try_stack;
	};
	return NULL;
};

static struct exception *get_thread_exception() {
	struct thread_local_data *thread_local_data = get_thread_local_data();
	if (thread_local_data) {
		return &thread_local_data->exception;
	};
	return NULL;
};

int exception_init() {
#ifdef _PTHREAD_ENABLE
	int ret = pthread_key_create(&thread_key, thread_local_data_free);
	if (ret) return ret;
	return thread_init();
#else
	nonthread_local_data.exception = (struct exception)EXCEPTION_INIT_FAIL;
	nonthread_local_data.try_stack = (struct try_stack)EMPTY_TRY_STACK;
	return 0;
#endif
};

int exception_thread_init() {
#ifdef _PTHREAD_ENABLE
	return thread_init();
#endif
	return 0;
};

jmp_buf *exception_jmpbuf(const char *file, const char *func, int line) {
	if (exception_grow()) {
		throw_errno();
	};
	struct exception *exception = get_thread_exception();
	struct try_stack *try_stack = get_thread_try_stack();
	exception->success = 1;
	try_stack->i[try_stack->n].file = file;
	try_stack->i[try_stack->n].func = func;
	try_stack->i[try_stack->n].line = line;
	if (kga_debug) {
		fprintf(stderr, "Try %s: %s: %i", try_stack->i[try_stack->n].file, try_stack->i[try_stack->n].func, try_stack->i[try_stack->n].line);
	};
	try_stack->n++;
	if (kga_debug) {
		fprintf(stderr, ", exception stack depth is %lli\n", (long long int)try_stack->n);
	};
	try_stack->top = try_stack->n;
	return &try_stack->i[try_stack->n-1].jmp;
};

int exception_grow() {
	int ret = -1;
	struct try_stack *try_stack = get_thread_try_stack();
	if (!try_stack) {
		goto finish;
	};
	if (try_stack->s >= try_stack->n) {
		size_t new_s = try_stack->s + JMP_DELTA;
		void *new_i = realloc(try_stack->i, new_s * sizeof(struct try_item));
		if(!new_i) {
			throw(exception_type_memory, errno, strerror(errno), NULL);
			goto finish;
		};
		try_stack->i = new_i;
		try_stack->s = new_s;
	};
	ret = 0;
finish:
	return ret;
};

void exception_print(FILE *out) {
	const exception_t *exc = exception();
	struct try_stack *try_stack = get_thread_try_stack();
	fprintf(out, "Exception: %s:%s:%i: #%i: `%s': `%s'\n",
			exc->file,
			exc->func,
			exc->line,
			exc->code,
			exc->message ? exc->message : "undefined",
			exc->verbose ? exc->verbose : "undefined");
	fprintf(out, "Try stack (%zi, %zi):\n", try_stack->top, try_stack->n);
	for(size_t i = 0; i < try_stack->top; i++) {
		fprintf(out, "\t%s: %s: %i\n", try_stack->i[i].file, try_stack->i[i].func, try_stack->i[i].line);
	};
};

void exception_snprint(char* out, size_t out_len) {
	const exception_t *exc = exception();
	struct try_stack *try_stack = get_thread_try_stack();
	size_t printed, printed_total = 0;
	printed = snprintf(out, out_len, "Exception: %s:%s:%i: #%i: `%s': `%s'\n",
			exc->file,
			exc->func,
			exc->line,
			exc->code,
			exc->message ? exc->message : "undefined",
			exc->verbose ? exc->verbose : "undefined");
	if (printed >= out_len) return;
	out_len -= printed;
	printed_total += printed;
	printed = snprintf(&out[printed_total], out_len, "Try stack (Top: %zi, Position: %zi):\n", try_stack->top, try_stack->n);
	if (printed >= out_len) return;
	out_len -= printed;
	printed_total += printed;
	for(size_t i = 0; i < try_stack->top; i++) {
		printed = snprintf(&out[printed_total], out_len, "\t%s: %s: %i\n", try_stack->i[i].file, try_stack->i[i].func, try_stack->i[i].line);
		if (printed >= out_len) return;
		out_len -= printed;
		printed_total += printed;
	};
};

void exception_throw(exception_type_t *type, int code, const char *message, const char *file, const char *func, int line, char *verbose) {
	if (kga_debug) {
		fprintf(stderr, "Exception throw: %s:%s:%i: `%s', `%s', type 0x%llx, code %i\n", file, func, line, message ? message : "", verbose ? verbose : "", (long long int)type, code);
	};
	struct try_stack *try_stack = get_thread_try_stack();
	struct exception *exception = get_thread_exception();
	exception->success = 0;
	exception->type = type ? type : &exception_type_unknown;
	if (verbose != exception->verbose)
		free(exception->verbose);
	exception->code = code;
	exception->message = message;
	exception->file = file;
	exception->func = func;
	exception->line = line;
	exception->verbose = verbose;
	if (!try_stack->n) {
		exception_print(stderr);
		/*
		 * What should we do?
		 */
		//exit(EXIT_FAILURE);
		exit(EXIT_FAILURE);
		return;
	} else if (kga_debug) {
		exception_print(stderr);
	};
	try_stack->n--;
	longjmp(try_stack->i[try_stack->n].jmp, exception->code);
};

void exception_finish() {
	struct try_stack *try_stack = get_thread_try_stack();
	struct exception *exception = get_thread_exception();
	if (!try_stack->n) throw(exception_type_unknown, EINVAL, "Try stack empty, what you try finish?", NULL);
	exception->success = 1;
	try_stack->n--;
	if (kga_debug) {
		fprintf(stderr, "Try %s: %s: %i success, exception stack depth is %lli\n", try_stack->i[try_stack->n].file, try_stack->i[try_stack->n].func, try_stack->i[try_stack->n].line, (long long int)try_stack->n);
	};
	try_stack->top = try_stack->n;
	return;
};

const exception_t *exception() {
	return get_thread_exception();
};
