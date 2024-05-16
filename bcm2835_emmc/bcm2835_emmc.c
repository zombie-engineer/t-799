#include <bcm2835/bcm2835_emmc.h>
#include <common.h>
#include <ioreg.h>
#include <stringlib.h>
#include <block_device.h>
#include "bcm2835_emmc_priv.h"
#include "bcm2835_emmc_regs.h"
#include "bcm2835_emmc_cmd.h"
#include "bcm2835_emmc_utils.h"
#include "bcm2835_emmc_initialize.h"
#include "bcm2835_emmc_regs_bits.h"
#include <mbox.h>
#include <mbox_props.h>
#include <gpio.h>
#include <bcm2835_dma.h>
#include <bcm2835/bcm2835_ic.h>
#include <irq.h>

struct bcm2835_emmc bcm2835_emmc = { 0 };
int bcm2835_emmc_log_level;

#define GPIO_PIN_CARD_DETECT 47

static inline void bcm2835_emmc_init_gpio(void)
{
  int gpio_num;

  /* GPIO_CD */
  gpio_set_pin_function(GPIO_PIN_CARD_DETECT, GPIO_FUNCTION_INPUT);

  /* GPIO_CD Pull-up */
  gpio_set_pin_pullupdown_mode(GPIO_PIN_CARD_DETECT, GPIO_PULLUPDOWN_MODE_UP);
  // gpio_set_detect_high(GPIO_PIN_CARD_DETECT);

  for (gpio_num = 48; gpio_num < 48 + 6; gpio_num++) {
    gpio_set_pin_function(gpio_num, GPIO_FUNCTION_ALT_3);
    gpio_set_pin_pullupdown_mode(gpio_num, GPIO_PULLUPDOWN_MODE_UP);
  }
}

static inline void bcm2835_emmc_debug_registers(void)
{
#define DUMP_REG(__reg_suffix) \
  do { \
    printf("BCM2835_EMMC_" #__reg_suffix "          : %08x\r\n",\
      ioreg32_read(BCM2835_EMMC_ ## __reg_suffix)); \
  } while(0)

  DUMP_REG(ARG2);
  DUMP_REG(BLKSIZECNT);
  DUMP_REG(ARG1);
  DUMP_REG(CMDTM);
  DUMP_REG(RESP0);
  DUMP_REG(RESP1);
  DUMP_REG(RESP2);
  DUMP_REG(RESP3);
  DUMP_REG(DATA);
  DUMP_REG(STATUS);
  DUMP_REG(CONTROL0);
  DUMP_REG(CONTROL1);
  DUMP_REG(INTERRUPT);
  DUMP_REG(IRPT_MASK);
  DUMP_REG(IRPT_EN);
  DUMP_REG(CONTROL2);
  DUMP_REG(CAPABILITIES_0);
  DUMP_REG(CAPABILITIES_1);
  DUMP_REG(FORCE_IRPT);
  DUMP_REG(BOOT_TIMEOUT);
  DUMP_REG(DBG_SEL);
  DUMP_REG(EXRDFIFO_CFG);
  DUMP_REG(EXRDFIFO_EN);
  DUMP_REG(TUNE_STEP);
  DUMP_REG(TUNE_STEPS_STD);
  DUMP_REG(TUNE_STEPS_DDR);
  DUMP_REG(SPI_INT_SPT);
  DUMP_REG(SLOTISR_VER);
#undef DUMP_REG
}

int bcm2835_emmc_init(void)
{
  int err;
  if (bcm2835_emmc.is_initialized)
    return SUCCESS;

  bcm2835_emmc_log_level = LOG_LEVEL_DEBUG2;
  bcm2835_emmc.is_blocking_mode = true;
  bcm2835_emmc.num_inhibit_waits = 0;
  err = bcm2835_emmc_io_init(&bcm2835_emmc.io, bcm2835_emmc_dma_irq_callback);
  if (err != SUCCESS) {
    BCM2835_EMMC_ERR("bcm2835_emmc_init failed at io: %d", err);
    return err;
  }

  // bcm2835_emmc_init_gpio();
  err = bcm2835_emmc_reset(bcm2835_emmc.is_blocking_mode, &bcm2835_emmc.rca,
    bcm2835_emmc.device_id);

  if (err != SUCCESS) {
    BCM2835_EMMC_ERR("bcm2835_emmc_init failed: %d", err);
    return err;
  }

  BCM2835_EMMC_LOG("bcm2835_emmc_init successfull");
  // bcm2835_emmc_debug_registers();
  bcm2835_emmc.is_initialized = true;
  return SUCCESS;
}

void bcm2835_emmc_report(void)
{
  uint32_t ver;
  int vendor, sdver;
  uint32_t clock_rate;

  ver = bcm2835_emmc_read_reg(BCM2835_EMMC_SLOTISR_VER);
  mbox_get_clock_rate(MBOX_CLOCK_ID_EMMC, &clock_rate);
  vendor = BCM2835_EMMC_SLOTISR_VER_GET_VENDOR(ver);
  sdver = BCM2835_EMMC_SLOTISR_VER_GET_SDVERSION(ver);

  BCM2835_EMMC_LOG("version %08x, VENDOR: %04x, SD: %04x, clock: %d", ver,
    vendor, sdver, clock_rate);
}

typedef enum {
  BCM2835_EMMC_IO_READ = 0,
  BCM2835_EMMC_IO_WRITE = 1
} bcm2835_emmc_io_type_t;

static inline int bcm2835_emmc_data_io(bcm2835_emmc_io_type_t io_type,
  char *buf, size_t start_sector, size_t num_blocks)
{
  int cmd_err;

  if (num_blocks > 1)
    cmd_err = bcm2835_emmc_cmd23(num_blocks, bcm2835_emmc.is_blocking_mode);

  if (io_type == BCM2835_EMMC_IO_READ) {
    /* READ_SINGLE_BLOCK */
    if (num_blocks > 1) {
      cmd_err = bcm2835_emmc_cmd18(start_sector, num_blocks, buf,
        bcm2835_emmc.is_blocking_mode);
    } else {
      cmd_err = bcm2835_emmc_cmd17(start_sector, buf,
        bcm2835_emmc.is_blocking_mode);
    }
    if (cmd_err)
      return -1;
  } else if (io_type == BCM2835_EMMC_IO_WRITE) {
    /* WRITE_BLOCK */
    if (num_blocks > 1) {
      cmd_err = bcm2835_emmc_cmd25(start_sector, num_blocks, buf,
        bcm2835_emmc.is_blocking_mode);
    } else {
      cmd_err = bcm2835_emmc_cmd24(start_sector, buf,
        bcm2835_emmc.is_blocking_mode);
    }
    if (cmd_err)
      return -1;
  } else {
    BCM2835_EMMC_ERR("bcm2835_emmc_data_io: unknown io type: %d", io_type);
    return -1;
  }

  return 0;
}

static int bcm2835_emmc_read(struct block_device *b, char *buf,
  size_t start_sector, size_t num_blocks)
{
  return bcm2835_emmc_data_io(BCM2835_EMMC_IO_READ, buf, start_sector,
    num_blocks);
}

static int bcm2835_emmc_write(struct block_device *b, const void *buf,
  size_t start_sector, size_t num_blocks)
{
  return bcm2835_emmc_data_io(BCM2835_EMMC_IO_WRITE, (char *)buf,
    start_sector, num_blocks);
}

int bcm2835_emmc_set_interrupt_mode(void)
{
  bcm2835_emmc.is_blocking_mode = false;
  irq_set(BCM2835_IRQNR_ARASAN_SDIO, bcm2835_emmc_irq_handler);
  bcm2835_ic_enable_irq(BCM2835_IRQNR_ARASAN_SDIO);
}

#ifdef ENABLE_JTAG
void bcm2835_emmc_write_kernel_to_card()
{
  bcm2835_emmc_reset();
}

#endif

int bcm2835_emmc_block_device_init(struct block_device *bdev)
{
  bdev->ops.read = bcm2835_emmc_read;
  bdev->ops.write = bcm2835_emmc_write;
}
