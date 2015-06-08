#include <kga/string.h>
#include <kga/array.h>
#include <c.h>
#include <kga/kga.h>
#include <stdio.h>
#include "exceptions.h"

#define STRING_DELTA 32

const char *string_empty = "";

char *string_fmt_real(char *string, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	try string = string_fmt_real_v(string, fmt, args);
	va_end(args);
	catch throw_proxy();
	return string;
};

char *string_fmt_real_v(char *string, const char *fmt, va_list args) {
	if (!fmt) throw(exception_type_null, EINVAL, second_arg_is_null, NULL);
	scope {
		char *new_string = array_new(char, 1, ARRAY_NULL_TERMINATED);
		int wanted_size;
		va_list args_copy;
		try {
			for (va_copy(args_copy, args);
					(wanted_size = vsnprintf(new_string, array_length(new_string) + sizeof(char), fmt, args_copy)) > (ssize_t)array_length(new_string);
					va_copy(args_copy, args)) {
				va_end(args_copy);
				array_resize(new_string, wanted_size);
			};
			va_end(args_copy);
			if (wanted_size < 0) throw_errno();
			array_resize(new_string, wanted_size);
		};
		catch throw_proxy();
		string_set(string, new_string);
	};
	return string;
};

char *string_cut_real(char *string, size_t pos, size_t len) {
	try {
		if (pos + len < pos) throw(exception_type_integer_overflow, ERANGE, integer_overflow, NULL);
		if (pos + len > array_length(string)) {
			throw(exception_type_index_out_of_bound, EINVAL, index_out_of_bound, NULL);
		};
		memmove(&string[pos], &string[pos + len], array_length(string) - len - pos);
		array_resize(string, array_length(string) - len);
	};
	catch throw_proxy();
	return string;
};

size_t string_length(char *string) {
	return array_length(string);
};

char *string_new() {
	char *string = NULL;
	try {
		string = array_new(char, 0, ARRAY_NULL_TERMINATED);
	};
	catch throw_proxy();
	return string;
};

char *string_push_real(char *string, char push) {
	if (!push) throw(exception_type_null, EINVAL, second_arg_is_null, NULL);
	try {
		array_push(string, push);
	};
	catch throw_proxy();
	return string;
};

char *string_clean_real(char *string) {
	try {
		array_resize(string, 0);
	};
	catch throw_proxy();
	return string;
};

char *string_set_real(char *string, const char *set) {
	if (!set) throw(exception_type_null, EINVAL, second_arg_is_null, NULL);
	try {
		array_resize(string, strlen(set));
		strcpy(string, set);
	};
	catch throw_proxy();
	return string;
};

char *string_cat_real(char *string, const char *cat) {
	if (!cat) throw(exception_type_null, EINVAL, second_arg_is_null, NULL);
	try {
		array_add(string, cat, strlen(cat));
	};
	catch throw_proxy();
	return string;
};

char *string_ncat_real(char *string, const char *cat, size_t cat_len) {
	if (!cat) throw(exception_type_null, EINVAL, second_arg_is_null, NULL);
	char *end_str;
	if ((end_str = memchr(cat, '\0', cat_len))) {
		cat_len = end_str - cat;
	};
	try {
		array_add(string, cat, cat_len);
	};
	catch throw_proxy();
	return string;
};

char *string_new_fmt(const char *fmt, ...) {
	va_list va;
	char *ret = NULL;
	va_start(va, fmt);
	try {
		ret = string_new_fmt_v(fmt, va);
	};
	va_end(va);
	catch throw_proxy();
	return ret;
};

char *string_set_from_wstring_real(char *string, const wchar_t *wstring) {
	if (!wstring) throw(exception_type_null, EINVAL, second_arg_is_null, NULL);
	try {
		size_t need_size = wcstombs(NULL, wstring, 0);
		array_resize(string, need_size);
		wcstombs(string, wstring, need_size);
	};
	catch throw_proxy();
	return string;
};

char *string_new_set(const char *set) {
	char *string = NULL;
	try {
		string = array_new(char, set ? strlen(set) : 0, ARRAY_NULL_TERMINATED);
		if (set) strcpy(string, set);
	};
	catch throw_proxy();
	return string;
};

char *string_new_fmt_v(const char *fmt, va_list va) {
	return string_fmt_real_v(string_new(), fmt, va);
};

char **string_split(const char *string, const char *delimeter, int flags) {
	if (!string) throw(exception_type_null, EINVAL, first_arg_is_null, NULL);
	if (!delimeter) throw(exception_type_null, EINVAL, second_arg_is_null, NULL);
	char **out = array_new(char *, 0, ARRAY_NULL_TERMINATED);
	size_t delimeter_len = strlen(delimeter);
	if (!delimeter_len) throw(exception_type_null, EINVAL, "Delimeter empty", NULL);

	char *item = NULL;

	for (const char *c = string ; *c; c++) {
		if (!strncmp(c, delimeter, delimeter_len)) {
			if (!(flags & STRING_SPLIT_WITHOUT_EMPTY)) {
				item = string_new();
			};
			if (item) {
				array_push(out, item);
				item = NULL;
			};
			c += delimeter_len - 1;
		} else {
			if (!item) item = string_new();
			string_push(item, *c);
		};
	};
	if (item) {
		array_push(out, item);
	};
	return out;
};

char *string_join(char **strings, const char *delimeter, int flags) {
	char *join = string_new();
	size_t strings_count = array_length(strings);
	if (!strings_count) return join;
	string_set(join, strings[0]);
	for (size_t i = 1; i < strings_count; i++) {
		if ((flags & STRING_JOIN_WITHOUT_EMPTY) && (!(strings[i]) || !*(strings[i]))) continue;
		if (delimeter) string_cat(join, delimeter);
		string_cat(join, strings[i] ? strings[i] : "");
	};
	return join;
};
