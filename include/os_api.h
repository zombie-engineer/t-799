#pragma once
#include <stdint.h>

/*
 * os_wait_ms - put current task to sleep for a given number of milliseconds
 * ms - number of milliseconds to sleep
 */
void os_wait_ms(uint32_t ms);

/*
 * os_yield - stop executing current task and give up rest of the time slice
 * for the management of scheduler
 */
void os_yield(void);

void svc_handler(uint32_t imm);
