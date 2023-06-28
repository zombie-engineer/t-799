#pragma once
#include <stdbool.h>

bool uart_pl011_init(int baudrate);
int uart_pl011_send(const void *buf, int num);
int uart_pl011_recv(void *buf, int num);
