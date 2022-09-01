#pragma once

int uart_pl011_init(void);
int uart_pl011_send(const void *buf, int num);
