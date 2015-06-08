#include <kga/wstring.h>
#include <kga/kga.h>
#include <kga/array.h>
#include <c.h>
#include <stdio.h>
#include "exceptions.h"

#define STRING_DELTA 32

const wchar_t *wstring_empty = L"";

wchar_t *wstring_fmt_real(wchar_t *wstring, const wchar_t *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	try wstring = wstring_fmt_real_v(wstring, fmt, args);
	va_end(args);
	catch throw_proxy();
	return wstring;
};

wchar_t *wstring_fmt_real_v(wchar_t *wstring, const wchar_t *fmt, va_list args) {
	if (!fmt) throw(exception_type_null, EINVAL, second_arg_is_null, NULL);
	scope {
		wchar_t *new_wstring = array_new(wchar_t, 1, ARRAY_NULL_TERMINATED);
		int wanted_size;
		va_list args_copy;
		try {
			for (va_copy(args_copy, args);
					(wanted_size = vswprintf(new_wstring, array_length(new_wstring) + sizeof(wchar_t), fmt, args_copy)) > (ssize_t)array_length(new_wstring);
					va_copy(args_copy, args)) {
				va_end(args_copy);
				array_resize(new_wstring, wanted_size);
			};
			va_end(args_copy);
			if (wanted_size < 0) throw_errno();
			array_resize(new_wstring, wanted_size);
		};
		catch throw_proxy();
		wstring_set(wstring, new_wstring);
	};
	return wstring;
};

wchar_t *wstring_cut_real(wchar_t *wstring, size_t pos, size_t len) {
	try {
		if (pos + len < pos) throw(exception_type_integer_overflow, ERANGE, integer_overflow, NULL);
		if (pos + len >= array_length(wstring)) {
			throw(exception_type_index_out_of_bound, EINVAL, index_out_of_bound, NULL);
		};
		wmemmove(&wstring[pos], &wstring[pos + len], array_length(wstring) - len - pos);
		array_resize(wstring, array_length(wstring) - len);
	};
	catch throw_proxy();
	return wstring;
};

size_t wstring_length(wchar_t *wstring) {
	size_t ret = 0;
	try {
		ret = array_length(wstring);
	};
	catch throw_proxy();
	return ret;
};

wchar_t *wstring_new() {
	wchar_t *wstring = NULL;
	try {
		wstring = array_new(wchar_t, 0, ARRAY_NULL_TERMINATED);
	};
	catch throw_proxy();
	return wstring;
};

wchar_t *wstring_push_real(wchar_t *wstring, wchar_t push) {
	if (!push) throw(exception_type_null, EINVAL, second_arg_is_null, NULL);
	try {
		array_push(wstring, push);
	};
	catch throw_proxy();
	return wstring;
};

wchar_t *wstring_clean_real(wchar_t *wstring) {
	try {
		array_resize(wstring, 0);
	};
	catch throw_proxy();
	return wstring;
};

wchar_t *wstring_set_real(wchar_t *wstring, const wchar_t *set) {
	if (!set) throw(exception_type_null, EINVAL, second_arg_is_null, NULL);
	try {
		array_resize(wstring, wcslen(set));
	};
	catch throw_proxy();
	wcscpy(wstring, set);
	return wstring;
};

wchar_t *wstring_cat_real(wchar_t *wstring, const wchar_t *cat) {
	if (!cat) throw(exception_type_null, EINVAL, second_arg_is_null, NULL);
	try {
		array_add(wstring, cat, wcslen(cat));
	};
	catch throw_proxy();
	return wstring;
};

wchar_t *wstring_ncat_real(wchar_t *wstring, const wchar_t *cat, size_t cat_len) {
	if (!cat) throw(exception_type_null, EINVAL, second_arg_is_null, NULL);
	wchar_t *end_str;
	if ((end_str = wmemchr(cat, L'\0', cat_len))) {
		cat_len = end_str - cat;
	};
	try {
		array_add(wstring, cat, cat_len);
	};
	catch throw_proxy();
	return wstring;
};

wchar_t *wstring_new_fmt(const wchar_t *fmt, ...) {
	va_list va;
	wchar_t *ret = NULL;
	va_start(va, fmt);
	try {
		ret = wstring_new_fmt_v(fmt, va);
	};
	va_end(va);
	catch throw_proxy();
	return ret;
};

wchar_t *wstring_set_from_string_real(wchar_t *wstring, const char *string) {
	if (!string) throw(exception_type_null, EINVAL, second_arg_is_null, NULL);
	try {
		size_t need_size = mbstowcs(NULL, string, 0);
		array_resize(wstring, need_size);
		mbstowcs(wstring, string, need_size);
	};
	catch throw_proxy();
	return wstring;
};

wchar_t *wstring_new_set(const wchar_t *set) {
	wchar_t *string = NULL;
	try {
		string = array_new(wchar_t, set ? wcslen(set) : 0, ARRAY_NULL_TERMINATED);
		if (set) wcscpy(string, set);
	};
	catch throw_proxy();
	return string;
};

wchar_t *wstring_new_fmt_v(const wchar_t *fmt, va_list va) {
	return wstring_fmt_real_v(wstring_new(), fmt, va);
};

