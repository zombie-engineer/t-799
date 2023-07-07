#include "spi.h"
#include "memory_map.h"
#include "ioreg.h"
#include <stringlib.h>
#include "common.h"
#include "gpio.h"
#include <errcode.h>
#include "ioreg.h"
#include <bitops.h>
#include <io_flags.h>

#define SPI_BASE ((unsigned long long)BCM2835_MEM_PERIPH_BASE + 0x00204000)

/*
 * Description of SPI0 CS reg at SPI0_BASE + 0x00
 * 1:0 Chip Select (00, 01, 10 - chips 0, 1, 2. 11 -reserved)
 * 2   Clock Phase
 * 3   Clock Polarity
 * 5:4 CLEAR FIFO clear
 * 6   Chip Select Polarity
 * 7   Transfer active
 * 8   DMAEN DMA Enable
 * 9   INTD Interrupt on Done
 * 10  INTR Interrupt on RXR
 * 11  ADCS Auto Deassert Chip select
 * 12  REN Read Enable
 * 13  LEN LoSSI enable
 * 14  Unused
 * 15  Unused
 * 16  Done transfer Done
 * 17  RX FIFO contains Data
 * 18  TX FIFO can accept Data
 * 19  RX FIFO needs Reading (full)
 * 20  FX FIFO Full
 * 21  Chip Select 0 Polarity
 * 22  Chip Select 1 Polarity
 * 23  Chip Select 2 Polarity
 * 24  Enable DMA mode in Lossi mode
 * 25  Enable Long data word in Lossi mode if DMA_LEN set
 * 31:26 Write as 0
 */

#define SPI_CS_CS       BITS(0, 3)
#define SPI_CS_CPHA     BIT(2)
#define SPI_CS_CPOL     BIT(3)
#define SPI_CS_CLEAR    BITS(4, 3)
#define SPI_CS_CSPOL    BIT(6)
#define SPI_CS_TA       BIT(7)
#define SPI_CS_DMAEN    BIT(8)
#define SPI_CS_INTD     BIT(9)
#define SPI_CS_INTR     BIT(10)
#define SPI_CS_ADCS     BIT(11)
#define SPI_CS_REN      BIT(12)
#define SPI_CS_LEN      BIT(13)
#define SPI_CS_DONE     BIT(16)
#define SPI_CS_RXD      BIT(17)
#define SPI_CS_TXD      BIT(18)
#define SPI_CS_RXR      BIT(19)
#define SPI_CS_RXF      BIT(20)
#define SPI_CS_CSPOL0   BIT(21)
#define SPI_CS_CSPOL1   BIT(22)
#define SPI_CS_CSPOL2   BIT(23)
#define SPI_CS_DMA_LEN  BIT(24)
#define SPI_CS_LEN_LONG BIT(25)

#define SPI_CS   ((ioreg32_t)(SPI_BASE + 0x00))
#define SPI_FIFO ((ioreg32_t)(SPI_BASE + 0x04))
#define SPI_CLK  ((ioreg32_t)(SPI_BASE + 0x08))
#define SPI_DLEN ((ioreg32_t)(SPI_BASE + 0x0c))

struct spi_gpio_pin_desc {
	int pin;
	int function;
	int default_function;
};

struct spi_gpio_group_desc {
	struct spi_gpio_pin_desc pins[5];
};

#define PIN_DESC(__pin, __function, __default) \
	{ .pin = __pin, \
		.function = GPIO_FUNCTION_ ## __function, \
		.default_function = GPIO_FUNCTION_ ## __default \
	}

static const struct spi_gpio_group_desc gpio_desc = {
	.pins = {
		PIN_DESC(GPIO_PIN_7, ALT_0, INPUT),
		PIN_DESC(GPIO_PIN_8, ALT_0, INPUT),
		PIN_DESC(GPIO_PIN_9, ALT_0, INPUT),
		PIN_DESC(GPIO_PIN_10, ALT_0, INPUT),
		PIN_DESC(GPIO_PIN_11, ALT_0, INPUT)
	}
};

int spi0_init(struct spi_device *d)
{
	int i;
	const struct spi_gpio_pin_desc *p;

	if (!d)
		return ERR_GENERIC;

	if (d->initialized)
		return SUCCESS;

	for (i = 0; i < ARRAY_SIZE(gpio_desc.pins); ++i) {
		p = &gpio_desc.pins[i];
		gpio_set_pin_function(p->pin, p->function);
	}

	*SPI_CLK = 32;
	*SPI_CS |= SPI_CS_CLEAR;
	d->initialized = true;
	return SUCCESS;
}

int spi0_deinit(struct spi_device *d)
{
	int i;

	if (!d)
		return ERR_INVAL;

	if (!d->initialized)
		return ERR_INVAL;

	for (i = 0; i < ARRAY_SIZE(gpio_desc.pins); ++i) {
		const struct spi_gpio_pin_desc *p = &gpio_desc.pins[i];
		gpio_set_pin_function(p->pin, p->default_function);
	}

	*SPI_CS = 0;
	*SPI_CLK = 0;

	d->initialized = false;
	return SUCCESS;
}

static inline bool spi0_transfer_active(void)
{
	return ioreg32_read(SPI_CS) & SPI_CS_TA;
}

static inline bool spi0_transfer_in_progress(void)
{
	bool transfer_active = *SPI_CS & SPI_CS_TA;
	bool transfer_done = *SPI_CS & SPI_CS_DONE;
	return transfer_active && !transfer_done;
}

static int spi0_start_transfer(struct spi_device *d, int io_flags,
	int slave_addr)
{
	if (spi0_transfer_active())
		return ERR_BUSY;

	d->io_flags = io_flags;

	*SPI_CS |= SPI_CS_CLEAR;
	*SPI_CS |= SPI_CS_TA;

	return SUCCESS;
}

static bool spi0_write_possible(void)
{
	return *SPI_CS & SPI_CS_TXD;
}

static bool spi0_read_possible(void)
{
	return *SPI_CS & SPI_CS_RXD;
}

static int spi0_do_transfer(struct spi_device *d,
	const uint8_t *src,
	uint8_t *dst,
	unsigned int len,
	unsigned int *actual_len)
{
	int ret;
	int i;
	const uint8_t *src_byte = src;
	uint8_t *dst_byte = dst;
	uint8_t stub_src = 0;
	uint8_t stub_dst = 0;

	if (!d || !actual_len)
		return ERR_INVAL;

	if (d->io_flags & IO_FLAG_READ && !dst)
		return ERR_INVAL;

	if (d->io_flags & IO_FLAG_WRITE && !src)
		return ERR_INVAL;

	for (i = 0; i < len; ++i) {
		if (d->io_flags & IO_FLAG_READ)
			while(!spi0_read_possible());

		if (d->io_flags & IO_FLAG_WRITE)
			while(!spi0_write_possible());

		/* Write 1 byte to FIFO */
		*SPI_FIFO = src[i];

		/* Read 1 byte from FIFO */
		dst[i] = *SPI_FIFO;
		/* DONE - TX sent */
		while(spi0_transfer_in_progress());
	}
	*actual_len = len;
	return SUCCESS;
}

static int spi0_finish_transfer(struct spi_device *d)
{
	if (!spi0_transfer_active())
		return ERR_INVAL;

	while (spi0_transfer_in_progress());

	*SPI_CS |= SPI_CS_CLEAR;
	*SPI_CS &= ~SPI_CS_TA;

	return SUCCESS;
}

static const struct spi_fns spi0_fns = {
	.init = spi0_init,
	.deinit = spi0_deinit,
	.start_transfer = spi0_start_transfer,
	.do_transfer = spi0_do_transfer,
	.finish_transfer = spi0_finish_transfer
};

static struct spi_device spi0_dev = {
	.fns = &spi0_fns,
	.initialized = false
};

struct spi_device *spi_get_device(int spi)
{
	if (spi > 0)
		return NULL;
	return &spi0_dev;
}
