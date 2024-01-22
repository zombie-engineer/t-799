#pragma once
#include <stddef.h>

/* This trick helps testing scenarios on a host machine, when we should */
#ifdef TEST_STRING
#define strcmp     _strcmp
#define strncmp    _strncmp
#define strlen     _strlen
#define strnlen    _strnlen
#define strcpy     _strcpy
#define strncpy    _strncpy
#define memset     _memset
#define memcpy     _memcpy
#define memcmp     _memcmp
#define strtoll    _strtoll
#define sprintf    _sprintf
#define snprintf   _snprintf
#define vsprintf   _vsprintf
#define vsnprintf  _vsnprintf
#define isprint    _isprint
#define isdigit    _isdigit
#define isspace  _isspace
#endif

int isprint(char ch);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char* ptr);
size_t strnlen(const char*, size_t n);
char *strcpy(char *, const char *);
char *strncpy(char *, const char *, size_t n);
void *memset(void *dst, int value, long unsigned int n);
void *memcpy(void *dst, const void *src, size_t n);

int vsprintf(char *dst, const char *fmt, __builtin_va_list *args);
int vsnprintf(char *dst, size_t n, const char *fmt, __builtin_va_list *args);
int sprintf(char *dst, const char *fmt, ...);
int snprintf(char *dst, size_t n, const char *fmt, ...);
