#pragma once

#define ARRAY_SIZE(__a) (sizeof(__a)/sizeof(__a[0]))
#define ASSERT(__a) {}


void clear_reboot_request(void);
void request_reboot(void);

void reboot(void);

void panic(void);
