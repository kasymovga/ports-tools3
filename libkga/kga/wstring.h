#ifndef _KGA_TSTRING_H_
#define _KGA_TSTRING_H_

#include <stdarg.h>
#include <kga/exception.h>
#include <kga/array.h>
#include <sys/types.h>
#include <wchar.h>

const wchar_t *wstring_empty;
wchar_t *wstring_fmt_real(wchar_t *wstring, const wchar_t *fmt, ...);
wchar_t *wstring_fmt_real_v(wchar_t *wstring, const wchar_t *fmt, va_list va);
wchar_t *wstring_push_real(wchar_t *wstring, const wchar_t push);
wchar_t *wstring_cat_real(wchar_t *wstring, const wchar_t *cat);
wchar_t *wstring_ncat_real(wchar_t *wstring, const wchar_t *cat, size_t cat_len);
size_t wstring_length(wchar_t *wstring);
wchar_t *wstring_cut_real(wchar_t *wstring, size_t pos, size_t len);
wchar_t *wstring_clean_real(wchar_t *wstring);
wchar_t *wstring_set_real(wchar_t *wstring, const wchar_t *set);
wchar_t *wstring_new();
wchar_t *wstring_new_fmt(const wchar_t *fmt, ...);
wchar_t *wstring_set_from_string_real(wchar_t *wstring, const char *string);
wchar_t *wstring_new_set(const wchar_t *set);
wchar_t *wstring_new_fmt_v(const wchar_t *fmt, va_list va);

static inline void wstring_free(wchar_t *x) {array_free(x); };

#define wstring_fmt(x,y,...) (x=wstring_fmt_real(x,y,__VA_ARGS__))
#define wstring_fmt_v(x,y,z) (x=wstring_fmt_real_v(x,y,z))
#define wstring_cut(x,y,z) (x=wstring_cut_real(x,y,z))
#define wstring_cat(x,y) (x=wstring_cat_real(x,y))
#define wstring_ncat(x,y,z) (x=wstring_ncat_real(x,y,z))
#define wstring_push(x,y) (x=wstring_push_real(x,y))
#define wstring_clean(x) (x=wstring_clean_real(x))
#define wstring_set(x,y) (x=wstring_set_real(x,y))

#define wstring_set_from_string(x,y) (x=wstring_set_from_string_real(x,y))

#endif
