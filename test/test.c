#include <stringlib.h>
#include <stdio.h>
#include <stdlib.h>
// #include <string.h>

void test_sprintf(const char *fmt, ...)
{
  int status;
  char actual[256];
  char expected[256];
  __builtin_va_list args;
  __builtin_va_start(args, fmt);
  _vsprintf(actual, fmt, args);
  __builtin_va_start(args, fmt);
  vsprintf(expected, fmt, args);
  if (strcmp(actual, expected)) {
    printf("assertion! strings not equal: expected: '%s', got '%s'\n",
      expected, actual);
    exit(1);
  }
  printf("success: '%s'\n", actual);
}

void test_snprintf(int n, const char *fmt, ...)
{
  int status;
  int n1, n2;
  char buf1[256];
  char buf2[256];
  __builtin_va_list args;
  __builtin_va_start(args, fmt);
  n1 = _vsnprintf(buf1, n, fmt, args);
  __builtin_va_start(args, fmt);
  n2 = vsnprintf(buf2, n, fmt, args);
  if (n1 != n2) {
    printf("assertion! vsnprintf ret values dont match: %d != %d\n", n1, n2);
    exit(1);
  }
  if (strcmp(buf1, buf2)) {
    printf("assertion! strings not equal: expected: '%s', got '%s'\n", buf1, buf2);
    exit(1);
  }
  printf("success: '%s'\n", buf1);
}

void run_cases_sprintf()
{
  test_sprintf("%p", 1234);
  test_sprintf("%16p", 1234);
  test_sprintf("%016p", 1234);
  test_sprintf("%d", 0);
  test_sprintf("%d", 1234);
  test_sprintf("%d", -1234);
  test_sprintf("%d", 0x7fffffff);
  test_sprintf("%d", 0x80000000);

  test_sprintf("%x", 1234);
  test_sprintf("%x", -1234);
  test_sprintf("%x", 0x7fffffff);
  test_sprintf("%x", 0x80000000);

  test_sprintf("%lld", 1234);
  test_sprintf("%lld", -1234);
  test_sprintf("%lld", 0x7fffffff);
  test_sprintf("%lld", 0x80000000);

  test_snprintf(0, "%lld", 0x2000);
  test_snprintf(1, "%lld", 0x2000);
  test_snprintf(2, "%lld", 0x2000);
  test_snprintf(3, "%lld", 0x2000);
  test_snprintf(4, "%lld", 0x2000);
  test_snprintf(20, "%lld", 0x2000);

  test_snprintf(0, "%llx", 0x2000);
  test_snprintf(1, "%llx", 0x2000);
  test_snprintf(2, "%llx", 0x2000);
  test_snprintf(3, "%llx", 0x2000);
  test_snprintf(4, "%llx", 0x2000);
  test_snprintf(20, "%llx", 0x2000);

  test_snprintf(0, "%016llx", 0x2000);
  test_snprintf(1, "%016llx", 0x2000);
  test_snprintf(2, "%016llx", 0x2000);
  test_snprintf(3, "%016llx", 0x2000);
  test_snprintf(4, "%016llx", 0x2000);
  test_snprintf(20, "%016llx", 0x2000);
}

int main()
{
  run_cases_sprintf();
}
