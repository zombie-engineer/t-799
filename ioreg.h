#pragma once
#include <stdint.h>

typedef volatile uint8_t *ioreg8_t;
typedef volatile uint16_t *ioreg16_t;
typedef volatile uint32_t *ioreg32_t;

static inline uint8_t ioreg8_read(ioreg8_t reg)
{
	return *reg;
}

static inline uint16_t ioreg16_read(ioreg16_t reg)
{
	return *reg;
}

static inline uint32_t ioreg32_read(ioreg32_t reg)
{
	return *reg;
}

static inline void ioreg8_write(ioreg8_t reg, uint8_t v)
{
	*reg = v;
}

static inline void ioreg16_write(ioreg16_t reg, uint16_t v)
{
	*reg = v;
}

static inline void ioreg32_write(ioreg32_t reg, uint32_t v)
{
	*reg = v;
}
