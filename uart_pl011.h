#pragma once
#include <stdbool.h>

bool uart_pl011_init(int baudrate);
int uart_pl011_send(const void *buf, int num);
