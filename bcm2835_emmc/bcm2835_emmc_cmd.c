#include <bitops.h>
#include <stringlib.h>
#include <bcm2835/bcm2835_emmc.h>
#include <bcm2835/bcm2835_systimer.h>
#include "bcm2835_emmc_priv.h"
#include "bcm2835_emmc_cmd.h"
#include "bcm2835_emmc_utils.h"
#include "bcm2835_emmc_regs_bits.h"
#include "bcm2835_emmc_regs.h"
#include <bcm2835_dma.h>
#include <memory_map.h>
#include <os_api.h>
#include <sched.h>
#include <cpu.h>

extern uint64_t ts2;

/* P1 Physical Layer Simplified Specification.  * 4.9 Responces */

/* 4.9.1 R1 (normal response command) */
#define RESP_TYPE_R1  BCM2835_EMMC_RESPONSE_TYPE_48_BITS

/* 4.9.2 R1b */
#define RESP_TYPE_R1b  BCM2835_EMMC_RESPONSE_TYPE_48_BITS_BUSY

/* 4.9.3 R2 (CID, CSD register) */
#define RESP_TYPE_R2  BCM2835_EMMC_RESPONSE_TYPE_136_BITS

/* 4.9.4 R3 (OCR register) */
#define RESP_TYPE_R3  BCM2835_EMMC_RESPONSE_TYPE_48_BITS

/* 4.9.5 R6 (Published RCA response) */
#define RESP_TYPE_R6  BCM2835_EMMC_RESPONSE_TYPE_48_BITS

/* 4.9.5 R7 (Card interface condition) */
#define RESP_TYPE_R7  BCM2835_EMMC_RESPONSE_TYPE_48_BITS

/* Stub response type */
#define RESP_TYPE_NA  BCM2835_EMMC_RESPONSE_TYPE_NONE

/* Transfer type host to card */
#define BCM2835_EMMC_TRANS_TYPE_DATA_HOST_TO_CARD 0

/* Transfer type stub */
#define BCM2835_EMMC_TRANS_TYPE_DATA_NA 0

/* Transfer type card to host */
#define BCM2835_EMMC_TRANS_TYPE_DATA_CARD_TO_HOST 1

/*
 * Check SD Host Controller's STATUS register if CMD and DAT lines are not
 * used by controller and available for next commands
 */
#define __SDHC_STATUS_LINES_AVAIL_MASK \
  (BCM2835_EMMC_STATUS_MASK_CMD_INHIBIT | BCM2835_EMMC_STATUS_MASK_DAT_INHIBIT)

#define SDHC_STATUS_LINES_AVAILABLE(__status) \
  (!(__status & __SDHC_STATUS_LINES_AVAIL_MASK))


#define CMDTM_GEN(__n, __resp, __crc, __trans, __is_data, __multiblock)\
  ((__n << BCM2835_EMMC_CMDTM_SHIFT_CMD_INDEX) |\
  (RESP_TYPE_ ## __resp << BCM2835_EMMC_CMDTM_SHIFT_CMD_RSPNS_TYPE) |\
  (__crc << BCM2835_EMMC_CMDTM_SHIFT_CMD_CRCCHK_EN) |\
  (BCM2835_EMMC_TRANS_TYPE_DATA_ ## __trans << BCM2835_EMMC_CMDTM_SHIFT_TM_DAT_DIR) |\
  (__is_data << BCM2835_EMMC_CMDTM_SHIFT_CMD_ISDATA)|\
  (__multiblock << BCM2835_EMMC_CMDTM_SHIFT_TM_MULTI_BLOCK))

static uint32_t sd_commands[] = {
  CMDTM_GEN(0,  NA,      0, NA, 0, 0),
  CMDTM_GEN(1,  NA,      0, NA, 0, 0),
  CMDTM_GEN(2,  R2,      1, NA, 0, 0),
  CMDTM_GEN(3,  R6,      1, NA, 0, 0),
  CMDTM_GEN(4,  NA,      0, NA, 0, 0),
  CMDTM_GEN(5,  R1b,     0, NA, 0, 0),
  CMDTM_GEN(6,  R1,      0, CARD_TO_HOST, 1, 0),
  CMDTM_GEN(7,  R1b,     1, NA, 0, 0),
  CMDTM_GEN(8,  R7,      1, NA, 0, 0),
  CMDTM_GEN(9,  R2,      0, NA, 0, 0),
  CMDTM_GEN(10, R2,      0, NA, 0, 0),
  CMDTM_GEN(11, R1,      1, NA, 0, 0),
  CMDTM_GEN(12, R1b,     1, NA, 0, 0),
  CMDTM_GEN(13, R1,      1, NA, 0, 0),
  CMDTM_GEN(14, NA,      0, NA, 0, 0),
  CMDTM_GEN(15, NA,      0, NA, 0, 0),
  CMDTM_GEN(16, R1,      1, NA, 0, 0),
  CMDTM_GEN(17, R1,      1, CARD_TO_HOST, 1, 0),
  CMDTM_GEN(18, R1,      1, CARD_TO_HOST, 1, 1),
  CMDTM_GEN(19, R1,      1, NA, 0, 0),
  CMDTM_GEN(20, R1b,     1, NA, 0, 0),
  CMDTM_GEN(21, R1,      1, NA, 0, 0),
  CMDTM_GEN(22, R1,      1, NA, 0, 0),
  CMDTM_GEN(23, R1,      1, NA, 0, 0),
  CMDTM_GEN(24, R1,      1, HOST_TO_CARD, 1, 0),
  CMDTM_GEN(25, R1,      1, HOST_TO_CARD, 1, 1),
  CMDTM_GEN(26, NA,      0, NA, 0, 0),
  CMDTM_GEN(27, R1,      1, NA, 0, 0),
  CMDTM_GEN(28, R1b,     1, NA, 0, 0),
  CMDTM_GEN(29, R1b,     1, NA, 0, 0),
  CMDTM_GEN(30, R1,      1, NA, 0, 0),
  CMDTM_GEN(31, NA,      0, NA, 0, 0),
  CMDTM_GEN(32, R1,      1, NA, 0, 0),
  CMDTM_GEN(33, R1,      1, NA, 0, 0),
  CMDTM_GEN(34, NA,      0, NA, 0, 0),
  CMDTM_GEN(35, NA,      0, NA, 0, 0),
  CMDTM_GEN(36, NA,      0, NA, 0, 0),
  CMDTM_GEN(37, NA,      0, NA, 0, 0),
  CMDTM_GEN(38, R1b,     1, NA, 0, 0),
  CMDTM_GEN(39, NA,      0, NA, 0, 0),
  CMDTM_GEN(40, R1,      1, NA, 0, 0),
  CMDTM_GEN(41, NA,      0, NA, 0, 0),
  CMDTM_GEN(42, R1,      1, NA, 0, 0),
  CMDTM_GEN(43, NA,    0, NA, 0, 0),
  CMDTM_GEN(44, NA,    0, NA, 0, 0),
  CMDTM_GEN(45, NA,    0, NA, 0, 0),
  CMDTM_GEN(46, NA,    0, NA, 0, 0),
  CMDTM_GEN(47, NA,    0, NA, 0, 0),
  CMDTM_GEN(48, NA,    0, NA, 0, 0),
  CMDTM_GEN(49, NA,    0, NA, 0, 0),
  CMDTM_GEN(50, NA,    0, NA, 0, 0),
  CMDTM_GEN(51, NA,    0, NA, 0, 0),
  CMDTM_GEN(52, NA,    0, NA, 0, 0),
  CMDTM_GEN(53, NA,    0, NA, 0, 0),
  CMDTM_GEN(54, NA,    0, NA, 0, 0),
  CMDTM_GEN(55, R1,      1, NA, 0, 0),
  CMDTM_GEN(56, R1,      1, NA, 0, 0),
  CMDTM_GEN(57, NA,    0, NA, 0, 0),
  CMDTM_GEN(58, R3,    0, NA, 0, 0),
  CMDTM_GEN(59, NA,    0, NA, 0, 0)
};

static uint32_t sd_acommands[] = {
  CMDTM_GEN(0, NA,    0, NA, 0, 0),
  CMDTM_GEN(1, NA,    0, NA, 0, 0),
  CMDTM_GEN(2, NA,    0, NA, 0, 0),
  CMDTM_GEN(3, NA,    0, NA, 0, 0),
  CMDTM_GEN(4, NA,    0, NA, 0, 0),
  CMDTM_GEN(5, NA,    0, NA, 0, 0),
  CMDTM_GEN(6, R1,    1, NA, 0, 0),
  CMDTM_GEN(7, NA,    0, NA, 0, 0),
  CMDTM_GEN(8, NA,    0, NA, 0, 0),
  CMDTM_GEN(9, NA,    0, NA, 0, 0),
  CMDTM_GEN(10, NA,    0, NA, 0, 0),
  CMDTM_GEN(11, NA,    0, NA, 0, 0),
  CMDTM_GEN(12, NA,    0, NA, 0, 0),
  CMDTM_GEN(13, NA,    0, NA, 0, 0),
  CMDTM_GEN(14, NA,    0, NA, 0, 0),
  CMDTM_GEN(15, NA,    0, NA, 0, 0),
  CMDTM_GEN(16, NA,    0, NA, 0, 0),
  CMDTM_GEN(17, NA,    0, NA, 0, 0),
  CMDTM_GEN(18, NA,    0, NA, 0, 0),
  CMDTM_GEN(19, NA,    0, NA, 0, 0),
  CMDTM_GEN(20, NA,    0, NA, 0, 0),
  CMDTM_GEN(21, NA,    0, NA, 0, 0),
  CMDTM_GEN(22, NA,    0, NA, 0, 0),
  CMDTM_GEN(23, NA,    0, NA, 0, 0),
  CMDTM_GEN(24, NA,    0, NA, 0, 0),
  CMDTM_GEN(25, NA,    0, NA, 0, 0),
  CMDTM_GEN(26, NA,    0, NA, 0, 0),
  CMDTM_GEN(27, NA,    0, NA, 0, 0),
  CMDTM_GEN(28, NA,    0, NA, 0, 0),
  CMDTM_GEN(29, NA,    0, NA, 0, 0),
  CMDTM_GEN(30, NA,    0, NA, 0, 0),
  CMDTM_GEN(31, NA,    0, NA, 0, 0),
  CMDTM_GEN(32, NA,    0, NA, 0, 0),
  CMDTM_GEN(33, NA,    0, NA, 0, 0),
  CMDTM_GEN(34, NA,    0, NA, 0, 0),
  CMDTM_GEN(35, NA,    0, NA, 0, 0),
  CMDTM_GEN(36, NA,    0, NA, 0, 0),
  CMDTM_GEN(37, NA,    0, NA, 0, 0),
  CMDTM_GEN(38, NA,    0, NA, 0, 0),
  CMDTM_GEN(39, NA,    0, NA, 0, 0),
  CMDTM_GEN(40, NA,    0, NA, 0, 0),
  CMDTM_GEN(41, R3,    0, NA, 0, 0),
  CMDTM_GEN(42, NA,    0, NA, 0, 0),
  CMDTM_GEN(43, NA,    0, NA, 0, 0),
  CMDTM_GEN(44, NA,    0, NA, 0, 0),
  CMDTM_GEN(45, NA,    0, NA, 0, 0),
  CMDTM_GEN(46, NA,    0, NA, 0, 0),
  CMDTM_GEN(47, NA,    0, NA, 0, 0),
  CMDTM_GEN(48, NA,    0, NA, 0, 0),
  CMDTM_GEN(49, NA,    0, NA, 0, 0),
  CMDTM_GEN(50, R1,    1, NA, 0, 0),
  CMDTM_GEN(51, R1,    1, CARD_TO_HOST, 1, 0),
  CMDTM_GEN(52, NA,    0, NA, 0, 0),
  CMDTM_GEN(53, NA,    0, NA, 0, 0),
  CMDTM_GEN(54, NA,    0, NA, 0, 0),
  CMDTM_GEN(55, NA,    0, NA, 0, 0),
  CMDTM_GEN(56, NA,    0, NA, 0, 0),
  CMDTM_GEN(57, NA,    0, NA, 0, 0),
  CMDTM_GEN(58, NA,    0, NA, 0, 0),
  CMDTM_GEN(59, NA,    0, NA, 0, 0)
};

static struct event bcm2835_emmc_event;

extern struct bcm2835_emmc bcm2835_emmc;

bool dma_done;

void bcm2835_emmc_dma_irq_callback(void)
{
  ts2 = bcm2835_systimer_get_time_us();
  os_log("dma_irq at %ld\r\n", ts2);
  ioreg32_write(BCM2835_EMMC_IRPT_EN, 0x17f0000 | 2);
  dma_done = true;
}

static void bcm2835_emmc_setup_dma_transfer(int dma_channel, int control_block,
  void *mem_addr, size_t block_size, size_t num_blocks, bool is_write)
{
  struct bcm2835_dma_request_param r = { 0 };
  uint32_t data_reg = PERIPH_ADDR_TO_DMA(BCM2835_EMMC_DATA);

  r.dreq       = DMA_DREQ_EMMC;
  r.len        = block_size * num_blocks;
  r.enable_irq = true;
  if (is_write) {
    r.dreq_type  = BCM2835_DMA_DREQ_TYPE_DST;
    r.dst_type   = BCM2835_DMA_ENDPOINT_TYPE_NOINC;
    r.dst        = data_reg;
    r.src_type   = BCM2835_DMA_ENDPOINT_TYPE_INCREMENT;
    r.src        = RAM_PHY_TO_BUS_UNCACHED(mem_addr);
  }
  else {
    r.dreq_type  = BCM2835_DMA_DREQ_TYPE_SRC;
    r.dst_type   = BCM2835_DMA_ENDPOINT_TYPE_INCREMENT;
    r.dst        = RAM_PHY_TO_BUS_UNCACHED(mem_addr);
    r.src_type   = BCM2835_DMA_ENDPOINT_TYPE_NOINC;
    r.src        = data_reg;
  }
  bcm2835_dma_program_cb(&r, control_block);
}

bool emmc_should_log = false;

#define CUR_CMD_REG bcm2835_emmc.io.cmdreg
#define CUR_CMD_IDX (BCM2835_EMMC_CMDTM_GET_CMD_INDEX(CUR_CMD_REG))
#define CUR_CMD_ISDATA (BCM2835_EMMC_CMDTM_GET_CMD_ISDATA(CUR_CMD_REG))

void bcm2835_emmc_irq_handler(void)
{
  uint32_t r;
  int cmd_idx;
  bool cmd_done = false;
  bcm2835_emmc.io.num_irqs++;

  r = ioreg32_read(BCM2835_EMMC_INTERRUPT);

#if 0
  if (emmc_should_log) {
    printf("bcm2835_emmc_irq_handler: CMD%d(%08x),i:%d,r:%08x\r\n",
      CUR_CMD_IDX, CUR_CMD_REG, bcm2835_emmc.io.num_irqs, r);
  }
#endif

  ioreg32_write(BCM2835_EMMC_INTERRUPT, r);

  if (r & BCM2835_EMMC_INTERRUPT_MASK_ERR) {
    printf("before: rsp:%08x,%08x,%08x,%08x,sta:%08x\r\n",
    ioreg32_read(BCM2835_EMMC_RESP0),
    ioreg32_read(BCM2835_EMMC_RESP1),
    ioreg32_read(BCM2835_EMMC_RESP2),
    ioreg32_read(BCM2835_EMMC_RESP3),
    ioreg32_read(BCM2835_EMMC_STATUS));
    BCM2835_EMMC_LOG("bcm2835_emmc_irq_handler: CMD_ERR,%08x\r\n", r);
    bcm2835_emmc.io.err = ERR_GENERIC;
    cmd_done = true;
  }

  if (r & BCM2835_EMMC_INTERRUPT_MASK_CMD_DONE) {
    // printf(",CMD_DONE");

    if (!CUR_CMD_ISDATA) {
      bcm2835_emmc.io.err = SUCCESS;
      cmd_done = true;
    }
  }

  if (r & BCM2835_EMMC_INTERRUPT_MASK_DATA_DONE) {
    // printf(",DATA_DONE");
    bcm2835_emmc.io.err = SUCCESS;
    cmd_done = true;
  }

  if (cmd_done) {
    ts2 = bcm2835_systimer_get_time_us();
    // os_log("cmd_done at %ld\r\n", ts2);
    os_event_notify(&bcm2835_emmc_event);
  }

#if 0
  if (emmc_should_log)
    printf("\r\nbcm2835_emmc_irq_handler: CMD%d,after:%08x,done:%d\r\n",
      CUR_CMD_IDX, ioreg32_read(BCM2835_EMMC_INTERRUPT), cmd_done);
#endif
}

static inline void OPTIMIZED bcm2835_emmc_cmd_process_single_block(char *buf,
  int size, int is_write)
{
  uint32_t *ptr, *end;

  ptr = (uint32_t*)buf;
  end = ptr + (size / sizeof(*ptr));

  if (is_write) {
    while(ptr != end)
      ioreg32_write(BCM2835_EMMC_DATA, *ptr++);
  } else {
    while(ptr != end)
      *ptr++ = ioreg32_read(BCM2835_EMMC_DATA);
  }
}

static inline int bcm2835_emmc_report_interrupt_error(const char *tag,
  uint32_t intval)
{
  char buf[512] = { 0 };

  bcm2835_emmc_interrupt_bitmask_to_string(buf, sizeof(buf), intval);
  BCM2835_EMMC_ERR("%s: error bits in INTERRUPT register: %08x '%s'", intval,
    buf);

  return ERR_GENERIC;
}

static inline int bcm2835_emmc_wait_process_interrupts(const char *tag,
  uint32_t intbits,
  uint64_t timeout_usec)
{
  uint32_t intval;
  if (bcm2835_emmc_wait_reg_value(BCM2835_EMMC_INTERRUPT, 0, intbits,
    timeout_usec, &intval))
    return ERR_TIMEOUT;

  /* clear received interrupts */
  bcm2835_emmc_write_reg(BCM2835_EMMC_INTERRUPT, intval);
  if (BCM2835_EMMC_INTERRUPT_GET_ERR(intval)) {
    if (BCM2835_EMMC_INTERRUPT_GET_CTO_ERR(intval))
      return ERR_TIMEOUT;
    return bcm2835_emmc_report_interrupt_error(tag, intval);
  }
  return SUCCESS;
}

static inline int OPTIMIZED bcm2835_emmc_polling_data_io(
  struct sd_cmd *c,
  uint32_t cmdreg,
  uint64_t timeout_usec)
{
  int status;
  int block;
  bool is_write;
  uint32_t intbits;
  char *buf;

  /*
   * How to work with BCM2835_EMMC_INTERRUPT reg:
   * 0xffff0000 - is a bitmask to error interrupt bits.
   * bit 15 - is ERR bit - means some error has happened.
   * bits through 16-24 - details of error, hence 0xffff0000 - is a mask to
   * all error types.
   * bit 0 - CMD_DONE. To check that command has been received by SD card,
   * CMD_DONE and ERR bits must be polled simultaniously to distinguish between
   * 2 possible results - command succeeded or failed
   * If CMD_DONE is set - command has been processed successfully.
   * If command also implies DATA, then we need a couple of other interrupt
   * bits:
   *
   * bit 4 - WRITE_RDY - we poll for this bit if we want to write to data port.
   * At WRITE_RDY start writing to data port. The bit is set for each new block
   *
   * bit 5 - READ_RDY - we poll for this if we want to read some data from
   * data port.
   * At READ_RDY start fetching words from data port. The bit is set for each
   * new block
   *
   * bit 1 - DATA_DONE - controller sets this bit, when full amount of bytes,
   * ordered in BLOCKSIZELEN register, has been processed. Ex: block size = 8
   * and number of blocks = 2, so the total amount of data is 16 bytes. After
   * all 16 bytes have been processed by writing/reading them to/from data
   * port this bit will be set. Another logic would be to do data port IOs
   * until this bit is set, but I haven't tried it.
   */
  is_write = BCM2835_EMMC_CMDTM_GET_TM_DAT_DIR(cmdreg) ==
    BCM2835_EMMC_TRANS_TYPE_DATA_HOST_TO_CARD;

  /*
   * Decide which bits we should expect in the interrupt register.
   * for read  operations this is 0x8020 (bit 15 ERR, bit 4 READ_RDY )
   * for write operations this is 0x8010 (bit 15 ERR, bit 4 WRITE_RDY)
   */
  intbits = 0;
  BCM2835_EMMC_INTERRUPT_CLR_SET_ERR(intbits, 1);
  if (is_write) {
    /* 0x8010 */
    BCM2835_EMMC_INTERRUPT_CLR_SET_WRITE_RDY(intbits, 1);
  } else {
    /* 0x8020 */
    BCM2835_EMMC_INTERRUPT_CLR_SET_READ_RDY(intbits, 1);
  }

  for (block = 0; block < c->num_blocks; ++block) {
    status = bcm2835_emmc_wait_process_interrupts("emmc_cmd_process_data.block",
      intbits, timeout_usec);
    if (status != SUCCESS) {
      BCM2835_EMMC_ERR("error status during data ready wait: %d", status);
      return status;
    }

    buf = c->databuf + block * c->block_size;
    bcm2835_emmc_cmd_process_single_block(buf, c->block_size, is_write);
  }

  intbits = 0;
  BCM2835_EMMC_INTERRUPT_CLR_SET_ERR(intbits, 1);
  BCM2835_EMMC_INTERRUPT_CLR_SET_DATA_DONE(intbits, 1);
  /* 0x8002 */
  return bcm2835_emmc_wait_process_interrupts("emmc_cmd_process_data.final",
    intbits, timeout_usec);
}

static inline int bcm2835_emmc_cmd_interrupt_based(struct sd_cmd *c)
{
  uint32_t cmdreg = bcm2835_emmc.io.cmdreg;

  bool is_data = BCM2835_EMMC_CMDTM_GET_CMD_ISDATA(cmdreg);
  bool is_write;

  if (is_data) {
    is_write = BCM2835_EMMC_CMDTM_GET_TM_DAT_DIR(cmdreg) ==
      BCM2835_EMMC_TRANS_TYPE_DATA_HOST_TO_CARD;
    bcm2835_emmc_setup_dma_transfer(bcm2835_emmc.io.dma_channel,
      bcm2835_emmc.io.dma_control_block_idx,
      bcm2835_emmc.io.c->databuf,
      bcm2835_emmc.io.c->block_size,
      bcm2835_emmc.io.c->num_blocks, is_write);

    bcm2835_dma_set_cb(bcm2835_emmc.io.dma_channel,
      bcm2835_emmc.io.dma_control_block_idx);

    ioreg32_write(BCM2835_EMMC_IRPT_EN, 0x17f0000 | (1<<15));
  }
  else {
    ioreg32_write(BCM2835_EMMC_IRPT_EN, 0x17f0000 | 3 | (1<<15));
  }

#if 0
  if (emmc_should_log)
    BCM2835_EMMC_LOG("CMD%d: writing to CMDTM: %08x\r\n",
      BCM2835_EMMC_CMDTM_GET_CMD_INDEX(cmdreg), cmdreg);
#endif

  ioreg32_write(BCM2835_EMMC_CMDTM, cmdreg);

  if (is_data) {
    dma_done = false;
    bcm2835_dma_activate(bcm2835_emmc.io.dma_channel);
  }

  os_event_wait(&bcm2835_emmc_event);
  os_event_clear(&bcm2835_emmc_event);
  if (is_data) {
    while(!dma_done)
      asm volatile ("wfe");
    dma_done = false;
  }

  // printf("ret:%d\r\n",bcm2835_emmc.io.err);
  return bcm2835_emmc.io.err;
}

static inline void bcm2835_emmc_read_response(struct sd_cmd *c,
  struct bcm2835_emmc_io *io)
{
  switch(BCM2835_EMMC_CMDTM_GET_CMD_RSPNS_TYPE(io->cmdreg)) {
    case BCM2835_EMMC_RESPONSE_TYPE_NONE:
      break;
    case BCM2835_EMMC_RESPONSE_TYPE_136_BITS:
      c->resp0 = bcm2835_emmc_read_reg(BCM2835_EMMC_RESP0);
      c->resp1 = bcm2835_emmc_read_reg(BCM2835_EMMC_RESP1);
      c->resp2 = bcm2835_emmc_read_reg(BCM2835_EMMC_RESP2);
      c->resp3 = bcm2835_emmc_read_reg(BCM2835_EMMC_RESP3);
      break;
    case BCM2835_EMMC_RESPONSE_TYPE_48_BITS:
      c->resp0 = bcm2835_emmc_read_reg(BCM2835_EMMC_RESP0);
    break;
    case BCM2835_EMMC_RESPONSE_TYPE_48_BITS_BUSY:
      c->resp0 = bcm2835_emmc_read_reg(BCM2835_EMMC_RESP0);
    break;
  }
}

#define BCM2835_EMMC_INTERRUPT_ALL_ERR \
    ( BCM2835_EMMC_INTERRUPT_MASK_ERR      \
    | BCM2835_EMMC_INTERRUPT_MASK_CTO_ERR  \
    | BCM2835_EMMC_INTERRUPT_MASK_CCRC_ERR \
    | BCM2835_EMMC_INTERRUPT_MASK_CEND_ERR \
    | BCM2835_EMMC_INTERRUPT_MASK_CBAD_ERR \
    | BCM2835_EMMC_INTERRUPT_MASK_DTO_ERR  \
    | BCM2835_EMMC_INTERRUPT_MASK_DCRC_ERR \
    | BCM2835_EMMC_INTERRUPT_MASK_DEND_ERR \
    | BCM2835_EMMC_INTERRUPT_MASK_ACMD_ERR)

static inline int bcm2835_emmc_polled_wait_cmd_done_or_err(
  uint64_t timeout_us, uint32_t *out_interrupt_value)
{
  uint32_t intr = 0;
  uint32_t intr_test;
  const uint64_t dt = 100;
  const uint32_t mask = BCM2835_EMMC_INTERRUPT_MASK_CMD_DONE
    | BCM2835_EMMC_INTERRUPT_ALL_ERR;

  while (1) {
    intr = bcm2835_emmc_read_reg(BCM2835_EMMC_INTERRUPT);
    if (intr & mask)
      break;

    bcm2835_emmc_read_reg(BCM2835_EMMC_STATUS);
    delay_us(dt);
    if (timeout_us < dt)
      return ERR_TIMEOUT;

    timeout_us -= dt;
  }

  *out_interrupt_value = intr;
  bcm2835_emmc_write_reg(BCM2835_EMMC_INTERRUPT, intr & mask);
  if (intr & BCM2835_EMMC_INTERRUPT_MASK_ERR)
    return ERR_GENERIC;

  intr_test = bcm2835_emmc_read_reg(BCM2835_EMMC_INTERRUPT);
  if (intr_test & mask) {
    printf("Cleared INTR still pending w:%08x->r:%08x\r\n", intr & mask,
      intr_test);
    return ERR_GENERIC;
  }

  return SUCCESS;
}

static inline int bcm2835_emmc_polled_exit_on_error(int cmd_idx,
  uint32_t interrupt)
{
  char intbuf[256];

  if (interrupt & BCM2835_EMMC_INTERRUPT_MASK_CTO_ERR) {
    BCM2835_EMMC_ERR("emmc_issue_cmd: exit by timeout");
    return ERR_TIMEOUT;
  }

  bcm2835_emmc_interrupt_bitmask_to_string(intbuf, sizeof(intbuf),
    interrupt);
  BCM2835_EMMC_ERR("CMD%d INTERRUPT ERR bits: %08x", interrupt);
  return ERR_GENERIC;
}

static inline int bcm2835_emmc_cmd_polled(struct sd_cmd *c,
  uint64_t timeout_us)
{
  int data_status;
  int err;
  uint32_t intval, intval_cmp;
  uint32_t cmdreg = bcm2835_emmc.io.cmdreg;

  if (irq_is_enabled()) {
    BCM2835_EMMC_LOG("bcm2835_emmc runs in polled mode and interrups enabled");
    return ERR_GENERIC;
  }

  bcm2835_emmc_write_reg(BCM2835_EMMC_CMDTM, cmdreg);


  bcm2835_emmc.io.err = bcm2835_emmc_polled_wait_cmd_done_or_err(timeout_us,
    &bcm2835_emmc.io.interrupt);

#if 0
  if (c->cmd_idx == 8 || c->cmd_idx == 0 || 1)
    printf("err:%08x intr:%08x\r\n", bcm2835_emmc.io.err, bcm2835_emmc.io.interrupt);
#endif

  if (bcm2835_emmc.io.err != SUCCESS)
    return bcm2835_emmc_polled_exit_on_error(c->cmd_idx,
      bcm2835_emmc.io.interrupt);

  bcm2835_emmc_read_response(c, &bcm2835_emmc.io);

  if (BCM2835_EMMC_CMDTM_GET_CMD_ISDATA(cmdreg)) {
    data_status = bcm2835_emmc_polling_data_io(c, cmdreg, timeout_us);

    if (data_status != SUCCESS) {
      BCM2835_EMMC_ERR("emmc_issue_cmd: data_status: %d", data_status);
      return data_status;
    }
  }

  if (BCM2835_EMMC_CMDTM_GET_CMD_RSPNS_TYPE(cmdreg) ==
    BCM2835_EMMC_RESPONSE_TYPE_48_BITS_BUSY) {
    if (!(bcm2835_emmc.io.interrupt & BCM2835_EMMC_INTERRUPT_MASK_DATA_DONE)) {
      err = bcm2835_emmc_interrupt_wait_done_or_err(timeout_us, 0, 1,
        &intval);

      if (err)
        return ERR_TIMEOUT;
    }

    bcm2835_emmc_write_reg(BCM2835_EMMC_INTERRUPT, intval);
  }
  return SUCCESS;
}

#define SDHC_WAIT_CMD_DAT_AVAIL() \
  while (!SDHC_STATUS_LINES_AVAILABLE(ioreg32_read(BCM2835_EMMC_STATUS))) { \
    BCM2835_EMMC_LOG("CMD%d wait inhibit", \
      BCM2835_EMMC_CMDTM_GET_CMD_INDEX(cmdreg)); \
    delay_us(100); \
  }

#if 0
#define SDHC_LOG_CMD() \
  emmc_should_log = true; \
  if (emmc_should_log) \
  { \
    BCM2835_EMMC_LOG("CMD%d, arg:%d, blocking:%d, cmdreg:%08x, irq_enabled:%d", \
      BCM2835_EMMC_CMDTM_GET_CMD_INDEX(cmdreg), \
      c->arg, polling, cmdreg, irq_is_enabled()); \
  }
#else
#define SDHC_LOG_CMD() ;
#endif

static inline int bcm2835_emmc_run_cmd(struct sd_cmd *c, uint32_t cmdreg,
  uint64_t timeout_usec, bool polling)
{
  uint32_t blksizecnt;
  uint32_t status;

  int i;

  SDHC_LOG_CMD();
  SDHC_WAIT_CMD_DAT_AVAIL();

  bcm2835_emmc.io.c = c;
  bcm2835_emmc.io.num_irqs = 0;
  blksizecnt = 0;
  BCM2835_EMMC_BLKSIZECNT_CLR_SET_BLKSIZE(blksizecnt, c->block_size);
  BCM2835_EMMC_BLKSIZECNT_CLR_SET_BLKCNT(blksizecnt, c->num_blocks);
  bcm2835_emmc_write_reg(BCM2835_EMMC_BLKSIZECNT, blksizecnt);
  bcm2835_emmc_write_reg(BCM2835_EMMC_ARG1, c->arg);
  if (c->num_blocks > 1)
    BCM2835_EMMC_CMDTM_CLR_SET_TM_BLKCNT_EN(cmdreg, 1);

  bcm2835_emmc.io.cmdreg = cmdreg;

  if (polling)
    return bcm2835_emmc_cmd_polled(c, timeout_usec);

  return bcm2835_emmc_cmd_interrupt_based(c);
}

static int bcm2835_emmc_run_acmd(struct sd_cmd *c,
  uint64_t timeout_usec, bool mode_polling)
{
  int acmd_idx;
  int status;
  struct sd_cmd acmd;

  uint32_t cmd_idx = c->cmd_idx & (((uint32_t)1)<<31) - 1;
  sd_cmd_init(&acmd, BCM2835_EMMC_CMD55, c->rca << 16);

  acmd.num_blocks = c->num_blocks;
  acmd.block_size = c->block_size;

  status = bcm2835_emmc_run_cmd(&acmd, sd_commands[BCM2835_EMMC_CMD55],
    timeout_usec, mode_polling);

  if (status != SUCCESS)
    return status;

  acmd_idx = BCM2835_EMMC_ACMD_RAW_IDX(c->cmd_idx);
  return bcm2835_emmc_run_cmd(c, sd_acommands[acmd_idx], timeout_usec,
    mode_polling);
}

int bcm2835_emmc_cmd(struct sd_cmd *c, uint64_t timeout_usec,
  bool mode_polling)
{
  uint32_t intval;

  int is_acmd = (c->cmd_idx >> 31) & 1;
  int cmd_idx = c->cmd_idx & (((uint32_t)1)<<31) - 1;

  intval = bcm2835_emmc_read_reg(BCM2835_EMMC_INTERRUPT);
  if (intval) {
    BCM2835_EMMC_WARN("pre-%s%d: interrupts detected: %08x",
      is_acmd ? "ACMD" : "CMD", cmd_idx, intval);
    bcm2835_emmc_write_reg(BCM2835_EMMC_INTERRUPT, intval);
  }

  if (BCM2835_EMMC_CMD_IS_ACMD(c->cmd_idx))
    return bcm2835_emmc_run_acmd(c, timeout_usec, mode_polling);

  return bcm2835_emmc_run_cmd(c, sd_commands[c->cmd_idx], timeout_usec,
    mode_polling);
}
