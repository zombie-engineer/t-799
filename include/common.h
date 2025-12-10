#pragma once
#include <stdint.h>

#define ARRAY_SIZE(__a) (sizeof(__a)/sizeof(__a[0]))
#define ASSERT(__a) {}
#define MAX(__a, __b) (((__a) > (__b)) ? (__a) : (__b))
#define MIN(__a, __b) (__a) < (__b) ? (__a) : (__b)
#define ROUND_UP_DIV(__a, __b) (((__a) + (__b) - 1) / (__b))
#define ROUND_UP (__v, __to) (ROUND_UP_DIV((__v), (__to)) * (__to))
#define ALIGN_UP_4(__sz) (((__sz) + 3) & ~3)


void clear_reboot_request(void);
void request_reboot(void);

void reboot(void);

void panic(void);
void panic_with_log(const char *);

static inline uint64_t reverse_bytes64(uint64_t input) {
    uint64_t output;
    asm volatile (
        "rev %0, %1\n\t"  // Reverse byte order of input into output
        : "=r" (output)   // Output operand (output)
        : "r" (input)     // Input operand
    );
    return output;
}

#define BUG_IF(__cond, __msg) \
  do { if ((__cond)) panic_with_log(__msg); } while(0)

#define container_of(__var, __type, __member) \
  (__type *)(((char *)__var) - offsetof(__type, __member))
