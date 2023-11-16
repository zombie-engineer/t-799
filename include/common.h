#pragma once

#define ARRAY_SIZE(__a) (sizeof(__a)/sizeof(__a[0]))
#define ASSERT(__a) {}
#define MAX(__a, __b) (__a) > (__b) ? (__a) : (__b)
#define MIN(__a, __b) (__a) < (__b) ? (__a) : (__b)


void clear_reboot_request(void);
void request_reboot(void);

void reboot(void);

void panic(void);
void panic_with_log(const char *);

#define container_of(__var, __type, __member) \
  (__type *)(((char *)__var) - offsetof(__type, __member))
