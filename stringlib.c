#include <stddef.h>
#include <common.h>

void *memset(void *dst, int value, long unsigned int n)
{
	char *ptr = (char *)dst;
	while (n--) {
		*ptr++ = value;
	}
	return dst;
}

#define IS_ALIGNED(addr, sz) (!((unsigned long)addr % sz))
#define IS_8_ALIGNED(addr) IS_ALIGNED(addr, 8)
#define IS_4_ALIGNED(addr) IS_ALIGNED(addr, 4)

#define TYPEBYIDX(typ, addr, idx) (((typ*)addr)[i])
#define CPY_ARRAY_EL(type, dst, src, idx) TYPEBYIDX(type, dst, idx) = TYPEBYIDX(type, src, idx)

#define CPY_CHAR_EL(dst, src, idx) CPY_ARRAY_EL(char, dst, src, idx)
#define CPY_INT_EL(dst, src, idx) CPY_ARRAY_EL(int, dst, src, idx)
#define CPY_LONG_EL(dst, src, idx) CPY_ARRAY_EL(long, dst, src, idx)

void * memcpy_8aligned(void *dst, const void *src, size_t n)
{
	int i;
#ifndef TEST_STRING
	if (!IS_8_ALIGNED(dst) || !IS_8_ALIGNED(src) || n % 8)
		panic_with_log("memcpy not aligned");
#endif

	for (i = 0; i < n / 8; ++i)
		CPY_LONG_EL(dst, src, i);

	return dst;
}

void * memcpy(void *dst, const void *src, size_t n)
{
	unsigned int i, imax;
	if (!IS_8_ALIGNED(dst) || !IS_8_ALIGNED(src)) {
		if (!IS_4_ALIGNED(dst) || !IS_4_ALIGNED(src)) {
			/* copy 1 byte */
			for (i = 0; i < n; i++)
				CPY_CHAR_EL(dst, src, i);
		}
		/* copy 4 bytes aligned */
		else {
			imax = (n / 4) * 4;
			for (i = 0; i < imax / 4; ++i)
				CPY_INT_EL(dst, src, i);
			for (i = (n / 4) * 4; i < n; ++i)
				CPY_CHAR_EL(dst, src, i);
		}
	}
	else {
		memcpy_8aligned(dst, src, (n / 8) * 8);
		for (i = (n / 8) * 8; i < n; ++i)
			CPY_CHAR_EL(dst, src, i);
	}
	return dst;
}
