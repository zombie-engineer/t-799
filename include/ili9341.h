#pragma once
#include <stddef.h>
#include <stdint.h>

void ili9341_init(void);
void ili9341_draw_bitmap(const uint8_t *data, size_t data_sz);
