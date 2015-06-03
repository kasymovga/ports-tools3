#define KGA_IMPORTANT_CHECK
#include <kga/array.h>
#include <kga/kga.h>

int main(int argc, char **argv) {
	scope {
		double *test_array = array_new(double);
		array_resize(test_array, 10);
		important_check(array_length(test_array) == 10);
		for (int i = 0; i < 10; i++) test_array[i] = i;
		array_add(test_array, 10);
	}
};
