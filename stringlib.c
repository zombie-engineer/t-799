#include <stddef.h>

void *memset(void *dst, int value, long unsigned int n)
{
  char *ptr = (char *)dst;
  while (n--) {
    *ptr++ = value;
  }
  return dst;
}
