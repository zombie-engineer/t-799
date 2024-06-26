#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

struct spi_device;

typedef int (*spi_init_fn)(struct spi_device *d);
typedef int (*spi_deinit_fn)(struct spi_device *d);

typedef int (*spi_start_transfer_fn)(struct spi_device *d, int io_flags,
  int slave_addr);

typedef int (*spi_do_transfer_fn)(struct spi_device *d,
  const uint8_t *src,
  uint8_t *dst,
  size_t len,
  size_t *transfered);

typedef int (*spi_finish_transfer_fn)(struct spi_device *d);

struct spi_fns {
  spi_init_fn init;
  spi_deinit_fn deinit;
  spi_start_transfer_fn start_transfer;
  spi_do_transfer_fn do_transfer;
  spi_finish_transfer_fn finish_transfer;
};

struct spi_device {
  const struct spi_fns *fns;
  bool initialized;
  int io_flags;
};

typedef enum {
  SPI_DEVICE_ID_SPI_0 = 0,
  SPI_DEVICE_ID_BITBANG
} spi_device_id_t;
int spi0_init_dma(void);

struct spi_device *spi_get_device(spi_device_id_t device_id);

int spi_configure_bitbang(struct spi_device *d, int sck, int mosi,
  int miso, const int *cs, int num_cs_pins);
