#include <c.h>
#include <kga/kga.h>
#include "exceptions.h"
#include <kga/array.h>

struct real_array {
	void *i;
	size_t n, item_size, size_limit, size_in_bytes;
	int flags;
};

size_t array_max_memory_size = 0;
size_t array_max_size = 0;

exception_type_t exception_type_integer_overflow;
exception_type_t exception_type_index_out_of_bound;

static void real_array_free_scope(void *ptr);
static void real_array_free(struct real_array *real_array);

static inline void *real_array_array(struct real_array *real_array) {
	return (void *)(&((struct real_array **)real_array->i)[1]);
};

static inline struct real_array *array_real_array(const void *array) {
	return (((struct real_array **)array)[-1]);
};

static size_t real_array_calculate_size(struct real_array *real_array, size_t new_size) {
	return sizeof(struct real_array *) + (real_array->item_size * (new_size + (real_array->flags & ARRAY_NULL_TERMINATED ? 1 : 0)));
};

static size_t powof2(size_t size) {
	size_t x, tries;
	for (x = 1, tries = 0; x < size && tries < 60; x *= 2);
	if (tries == 60)
		throw(exception_type_memory, ENOMEM, "Need to much memory for array, aborting", NULL);
	return x;
}

static struct real_array *real_array_new(size_t item_size, enum array_flags flags) {
	struct real_array *real_array = NULL;
	try {
		real_array = c_malloc(sizeof(struct real_array));
		real_array->i = NULL;
		real_array->flags = flags;
		real_array->n = 0;
		real_array->item_size = item_size;
		real_array->size_in_bytes = powof2(real_array_calculate_size(real_array, 0));
		real_array->i = c_malloc(real_array->size_in_bytes);
		((struct real_array **)real_array->i)[0] = real_array;
		if (real_array->flags & ARRAY_NULL_TERMINATED) {
			memset(&((char*)real_array->i)[sizeof(struct real_array *)], 0, real_array->item_size);
		};
	};
	catch {
		real_array_free(real_array);
		throw_proxy();
	};
	scope_add(real_array, real_array_free_scope);
	return real_array;
};

static void real_array_grow(struct real_array *real_array, size_t grow) {
	if(!grow) return;
	size_t need_n = real_array->n + grow;
	if (need_n <= real_array->n) {
		char *message = malloc(1024);
		if (message)
			snprintf(message, 1024, "real_array->n = %zi, grow = %zi", real_array->n, grow);
		throw(exception_type_integer_overflow, ERANGE, integer_overflow, message);
	};
	size_t need_size_in_bytes = powof2(real_array_calculate_size(real_array, need_n));
	if (real_array->size_in_bytes < need_size_in_bytes) {
		if (array_max_memory_size && need_size_in_bytes > array_max_memory_size)
			throw(exception_type_memory, ENOMEM, "Array memory limit exceeded", NULL);
		void *old_i = real_array->i;
		real_array->i = c_malloc(need_size_in_bytes);
		memcpy(real_array->i, old_i, real_array_calculate_size(real_array, real_array->n));
		free(old_i);
		real_array->size_in_bytes = need_size_in_bytes;
	};
};

static void real_array_resize(struct real_array *real_array, size_t resize) {
	if (array_max_size && resize > array_max_size) {
		char *verbose = malloc(1024);
		snprintf(verbose, 1024, "Requested array size: %zi", resize);
		throw(exception_type_memory, ENOMEM, "Array size limit exceeded", NULL);
	}
	try {
		if (resize > real_array->n) {
			real_array_grow(real_array, resize - real_array->n);
		};
		real_array->n = resize;
		if (real_array->flags & ARRAY_NULL_TERMINATED) {
			memset(&((char*)real_array->i)[real_array->n * real_array->item_size + sizeof(struct real_array *)], 0, real_array->item_size);
		};
	};
	catch throw_proxy();
};

static void real_array_add(struct real_array *real_array, const void *add, size_t length) {
	void *add_copy = NULL;
	if (((const char*)add <= (char*)real_array->i && &((const char*)add)[length*real_array->item_size] >= (char*)real_array->i) ||
		((const char*)add <= &((char*)real_array->i)[real_array->n * real_array->item_size]
		 && &((const char*)add)[length*real_array->item_size] >= &((char*)real_array->i)[real_array->n*real_array->item_size]) ||
		((const char*)add >= (char*)real_array->i && &((const char*)add)[length*real_array->item_size] <= &((char*)real_array->i)[real_array->n * real_array->item_size])) {
		add_copy = c_malloc(real_array->item_size * length);
		memcpy(add_copy, add, real_array->item_size * length);
		add = add_copy;
	};
	try {
		size_t n_save = real_array->n;
		real_array_resize(real_array, real_array->n + length);
		memcpy(&((char*)real_array->i)[n_save * real_array->item_size + sizeof(struct real_array *)], 
				add,
				real_array->item_size * length);
	};
	free(add_copy);
	catch throw_proxy();
};

static void real_array_free(struct real_array *real_array) {
	if (!real_array) return;
	free(real_array->i);
	free(real_array);
};

static void real_array_free_scope(void *ptr) {
	real_array_free((struct real_array *)ptr);
};

void *array_new_real(size_t item_size, enum array_flags flags) {
	struct real_array *real_array = real_array_new(item_size, flags);
	return real_array_array(real_array);
};

void *array_push_real(void *array, const void *push) {
	if(!array) throw(exception_type_null, EINVAL, first_arg_is_null, NULL);
	if(!push) throw(exception_type_null, EINVAL, second_arg_is_null, NULL);
	struct real_array *real_array = array_real_array(array);
	real_array_add(real_array, push, 1);
	return real_array_array(real_array);
};

void *array_add_real(void *array, const void *add, size_t length) {
	if(!array) throw(exception_type_null, EINVAL, first_arg_is_null, NULL);
	if(!add) throw(exception_type_null, EINVAL, second_arg_is_null, NULL);
	struct real_array *real_array = array_real_array(array);
	real_array_add(real_array, add, length);
	array = real_array_array(real_array);
	return array;
};

void *array_delete_interval_real(void *array, size_t begin, size_t length) {
	if (!array) throw(exception_type_null, EINVAL, first_arg_is_null, NULL);
	struct real_array *real_array = array_real_array(array);
	if (begin >= real_array->n) throw(exception_type_index_out_of_bound, EINVAL, index_out_of_bound, NULL);
	if (length > real_array->n - begin) throw(exception_type_index_out_of_bound, EINVAL, index_out_of_bound, NULL);
	memmove(&((char*)array)[begin * real_array->item_size], &((char*)array)[(begin + length) * real_array->item_size], (real_array->n - begin - length) * real_array->item_size);
	real_array_resize(real_array, real_array->n - length);
	return real_array_array(real_array);
};

void *array_resize_real(void *array, size_t resize) {
	struct real_array *real_array = NULL;
	try {
		if (!array) throw(exception_type_index_out_of_bound, EINVAL, first_arg_is_null, NULL);
		real_array = array_real_array(array);
		real_array_resize(real_array, resize);
	};
	catch throw_proxy();
	return real_array_array(real_array);
};

size_t array_length(const void *array) {
	if (!array) throw(exception_type_index_out_of_bound, EINVAL, first_arg_is_null, NULL);
	struct real_array *real_array = array_real_array(array);
	return real_array->n;
};

void array_free(void *array) {
	if(array)
		real_array_free(array_real_array(array));
};

void array_sort(void *array, int (*compar)(const void *, const void *)) {
	struct real_array *real_array = array_real_array(array);
	qsort(array, real_array->n, real_array->item_size, compar);
};

void *array_delete_items_by_value_real(void *array, void *item) {
	struct real_array *real_array = array_real_array(array);
	for (size_t i = 0; i < real_array->n; i++) {
		if (!memcmp(&(((char *)real_array->i)[i * real_array->item_size]), item, real_array->item_size)) {
			array_delete_interval_real(real_array_array(real_array), i, 1);
			i--;
		};
	};
	return real_array_array(real_array);
};

void array_swap(void *array, size_t foo, size_t bar) {
	if (!array) throw(exception_type_null, EINVAL, first_arg_is_null, NULL);
	struct real_array *real_array = array_real_array(array);
	if (foo >= real_array->n || bar >= real_array->n) throw(exception_type_index_out_of_bound, EINVAL, first_arg_is_null, NULL);
	char buf[real_array->item_size];
	memcpy(buf, &((char *)array)[real_array->item_size * foo], real_array->item_size);
	memcpy(&((char *)array)[real_array->item_size * foo], &((char *)array)[real_array->item_size * bar], real_array->item_size);
	memcpy(&((char *)array)[real_array->item_size * bar], buf, real_array->item_size);
};
