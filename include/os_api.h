#pragma once
#include <stdint.h>

/*
 * os_wait_ms - put current task to sleep for a given number of milliseconds
 * ms - number of milliseconds to sleep
 */
void os_wait_ms(uint32_t ms);
void svc_handler(uint32_t imm);
