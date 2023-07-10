#pragma once
#include <stddef.h>

int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
int memcmp(const void *a, const void *b, size_t n);
int strlen(const char* ptr);
int strnlen(const char*, size_t n);
char *strcpy(char *, const char *);
char *strncpy(char *, const char *, size_t n);
void *memset(void *dst, int value, long unsigned int n);
void *memcpy(void *dst, const void *src, size_t n);
