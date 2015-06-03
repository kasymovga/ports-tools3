#ifndef _KGA_TARRAY_H_
#define _KGA_TARRAY_H_
#include <sys/types.h>
#include <kga/exception.h>

enum array_flags {
	ARRAY_NULL_TERMINATED = 1
};

size_t array_max_size;
size_t array_max_memory_size;

exception_type_t exception_type_integer_overflow;
exception_type_t exception_type_index_out_of_bound;

void array_free(void *array);
void *array_new_real(size_t item_size, enum array_flags flags);
size_t array_length(const void *array);

void *array_add_real(void *array, const void *add, size_t length);
void *array_push_real(void *array, const void *push);
void *array_resize_real(void *array, size_t length);

void *array_delete_interval_real(void *array, size_t begin, size_t length);

void array_sort(void *array, int (*compar)(const void *, const void *));

void *array_delete_items_by_value_real(void *array, void *item);

void array_swap(void *array, size_t foo, size_t bar);

//Full of hacks, lol
#define array_foreach(array, iter_type, iter) for (iter_type _array_end##__LINE__ = &(array[array_length(array)]); _array_end##__LINE__; _array_end##__LINE__ = NULL) for (iter_type iter = array; iter < _array_end##__LINE__; iter++)

#define array_add(x,y,z) (x=array_add_real(x,y,z))
#define array_push(x,y) (x=array_push_real(x,&(y)))
#define array_resize(x,y) (x=array_resize_real(x,y))
#define array_clean(x) (x=array_resize_real(x,0))
#define array_delete_interval(x,y,z) (x=array_delete_interval_real(x,y,z))
#define array_new(x, y) array_new_real(sizeof(x), y)
#define array_delete_items_by_value(x, y) (x=array_delete_items_by_value_real(x, &(y)))

#endif
