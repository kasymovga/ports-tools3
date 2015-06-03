#ifndef _KGA_TSTRING_H_
#define _KGA_TSTRING_H_

#include <stdarg.h>
#include <kga/exception.h>
#include <kga/array.h>
#include <sys/types.h>
#include <wchar.h>

#define STRING_SPLIT_WITHOUT_EMPTY 1

#define STRING_JOIN_WITHOUT_EMPTY 1

const char *string_empty;
char *string_fmt_real(char *string, const char *fmt, ...);
char *string_fmt_real_v(char *string, const char *fmt, va_list va);
char *string_push_real(char *string, const char push);
char *string_cat_real(char *string, const char *cat);
char *string_ncat_real(char *string, const char *cat, size_t cat_len);
size_t string_length(char *string);
char *string_cut_real(char *string, size_t pos, size_t len);
char *string_clean_real(char *string);
char *string_set_real(char *string, const char *set);
char *string_new();
char *string_new_fmt(const char *fmt, ...);
char *string_set_from_wstring_real(char *string, const wchar_t *wstring);
char *string_new_fmt_v(const char *fmt, va_list va);
char *string_new_set(const char *set);

char **string_split(const char *string, const char *delimeter, int flags);
char *string_join(char **strings, const char *delimeter, int flags);

static inline void string_free(char *x) { array_free(x); };

#define string_fmt(x,y,...) (x=string_fmt_real(x,y,__VA_ARGS__))
#define string_fmt_v(x,y,z) (x=string_fmt_real_v(x,y,z))
#define string_cut(x,y,z) (x=string_cut_real(x,y,z))
#define string_cat(x,y) (x=string_cat_real(x,y))
#define string_ncat(x,y,z) (x=string_ncat_real(x,y,z))
#define string_push(x,y) (x=string_push_real(x,y))
#define string_clean(x) (x=string_clean_real(x))
#define string_set(x,y) (x=string_set_real(x,y))
#define string_set_from_wstring(x,y) (x=string_set_from_wstring_real(x,y))

#endif
