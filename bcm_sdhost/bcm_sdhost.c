#include <bcm2835/bcm_sdhost.h>
#include <bcm2835/bcm_cm.h>
#include <stdint.h>
#include <stdbool.h>
#include <ioreg.h>
#include <delay.h>
#include <bitops.h>
#include <printf.h>
#include <errcode.h>
#include <sdhc.h>
#include <sdhc_cmd.h>
#include <sdhc_io.h>
#include <gpio.h>
#include <bcm2835_dma.h>
#include "bcm_sdhost_cmd_reg.h"
#include "bcm_sdhost_log.h"
#include <mbox_props.h>
#include "memory_map.h"
#include <os_api.h>
#include <sched.h>
#include <irq.h>
#include <bcm2835/bcm2835_ic.h>

/*
 * In manual mode we set CDIV register by calculating required CDIV from
 * the required SDHOST clock frequency and the known source clock
 * But we can also let raspberry pi firmware control CDIV register by itself.
 * This might be important since it is known that Videocore might throttle
 * VPU clock to slower speed, at this point it could be handy it Videocore
 * also checked that CDIV receives a corrected value to upkeep the proper
 * frequency
 */
#define BCM_SDHOST_SET_CLOCK_MANUAL      0
#define BCM_SDHOST_SET_CLOCK_BY_FIRMWARE 1
#define BCM_SDHOST_SET_CLOCK BCM_SDHOST_SET_CLOCK_BY_FIRMWARE

#define PERIPHERAL_BASE  0x3f000000
#define SDHOST_BASE      (PERIPHERAL_BASE + 0x202000)

#define SDHOST_CMD   ((ioreg32_t)(SDHOST_BASE + 0x00))
#define SDHOST_ARG   ((ioreg32_t)(SDHOST_BASE + 0x04))
#define SDHOST_TOUT  ((ioreg32_t)(SDHOST_BASE + 0x08))
#define SDHOST_CDIV  ((ioreg32_t)(SDHOST_BASE + 0x0c))
#define SDHOST_RESP0 ((ioreg32_t)(SDHOST_BASE + 0x10))
#define SDHOST_RESP1 ((ioreg32_t)(SDHOST_BASE + 0x14))
#define SDHOST_RESP2 ((ioreg32_t)(SDHOST_BASE + 0x18))
#define SDHOST_RESP3 ((ioreg32_t)(SDHOST_BASE + 0x1c))
#define SDHOST_HSTS  ((ioreg32_t)(SDHOST_BASE + 0x20))
#define SDHOST_CNT   ((ioreg32_t)(SDHOST_BASE + 0x28))
#define SDHOST_VDD   ((ioreg32_t)(SDHOST_BASE + 0x30))
#define SDHOST_EDM   ((ioreg32_t)(SDHOST_BASE + 0x34))
#define SDHOST_HCFG  ((ioreg32_t)(SDHOST_BASE + 0x38))
#define SDHOST_HBCT  ((ioreg32_t)(SDHOST_BASE + 0x3c))
#define SDHOST_DATA  ((ioreg32_t)(SDHOST_BASE + 0x40))
#define SDHOST_HBLC  ((ioreg32_t)(SDHOST_BASE + 0x50))

#define SDHOST_MAX_CDIV 0x7ff

#define SDHOST_CFG_BUSY_IRPT_EN  BIT(10)
#define SDHOST_CFG_BLOCK_IRPT_EN BIT(8)
#define SDHOST_CFG_SDIO_IRPT_EN  BIT(5)
#define SDHOST_CFG_DATA_IRPT_EN  BIT(4)
#define SDHOST_CFG_SLOW_CARD     BIT(3)
#define SDHOST_CFG_WIDE_EXT_BUS  BIT(2)
#define SDHOST_CFG_WIDE_INT_BUS  BIT(1)
#define SDHOST_CFG_REL_CMD_LINE  BIT(0)

#define FIFO_BUG_NUM_DRAIN_WORDS 2
#define FIFO_READ_THRESHOLD   4
#define FIFO_WRITE_THRESHOLD  4
#define SDDATA_FIFO_PIO_BURST 8
#define SDDATA_FIFO_WORDS 16

#define SDHOST_HSTS_BUSY_IRPT    0x400
#define SDHOST_HSTS_BLOCK_IRPT   0x200
#define SDHOST_HSTS_SDIO_IRPT    0x100
#define SDHOST_HSTS_REW_TIME_OUT 0x80
#define SDHOST_HSTS_CMD_TIME_OUT 0x40
#define SDHOST_HSTS_CRC16_ERROR  0x20
#define SDHOST_HSTS_CRC7_ERROR   0x10
#define SDHOST_HSTS_FIFO_ERROR   0x08
#define SDHSTS_DATA_FLAG         0x01

#define SDHOST_EDM_FORCE_DATA_MODE   (1<<19)
#define SDHOST_EDM_CLOCK_PULSE       (1<<20)
#define SDHOST_EDM_BYPASS            (1<<21)

#define SDHOST_EDM_WRITE_THRESHOLD_SHIFT 9
#define SDHOST_EDM_READ_THRESHOLD_SHIFT 14
#define SDHOST_EDM_THRESHOLD_MASK     0x1f

#define SDHOST_EDM_FSM_MASK           0xf
#define SDHOST_EDM_FSM_IDENTMODE      0x0
#define SDHOST_EDM_FSM_DATAMODE       0x1
#define SDHOST_EDM_FSM_READDATA       0x2
#define SDHOST_EDM_FSM_WRITEDATA      0x3
#define SDHOST_EDM_FSM_READWAIT       0x4
#define SDHOST_EDM_FSM_READCRC        0x5
#define SDHOST_EDM_FSM_WRITECRC       0x6
#define SDHOST_EDM_FSM_WRITEWAIT1     0x7
#define SDHOST_EDM_FSM_POWERDOWN      0x8
#define SDHOST_EDM_FSM_POWERUP        0x9
#define SDHOST_EDM_FSM_WRITESTART1    0xa
#define SDHOST_EDM_FSM_WRITESTART2    0xb
#define SDHOST_EDM_FSM_GENPULSES      0xc
#define SDHOST_EDM_FSM_WRITEWAIT2     0xd
#define SDHOST_EDM_FSM_STARTPOWDOWN   0xf

#define SDHOST_HSTS_TRANSFER_ERROR_MASK \
  (SDHOST_HSTS_CRC7_ERROR|SDHOST_HSTS_CRC16_ERROR \
  |SDHOST_HSTS_REW_TIME_OUT|SDHOST_HSTS_FIFO_ERROR)

#define SDHOST_HSTS_ERROR_MASK \
  (SDHOST_HSTS_CMD_TIME_OUT| SDHOST_HSTS_TRANSFER_ERROR_MASK)

#define BCM_SDHOST_CHECK_ERR(__fmt, ...)\
  do {\
    if (err != SUCCESS) {\
      BCM_SDHOST_LOG_ERR("%s err %d, " __fmt, __func__,  err, ## __VA_ARGS__);\
      return err;\
    }\
  } while(0)

static int bcm_sdhost_log_level = LOG_LEVEL_NONE;
static struct event bcm_sdhost_event;

typedef enum {
  BCM_SDHOST_STATE_IDLE          = 0x0,
  BCM_SDHOST_STATE_CMD_INIT      = 0x1,
  BCM_SDHOST_STATE_CMD_TRANSMIT  = 0x2,
  BCM_SDHOST_STATE_CMD_RESPONSE  = 0x3,
  BCM_SDHOST_STATE_CMD_CRC       = 0x4,
  BCM_SDHOST_STATE_DATA_INIT     = 0x5,
  BCM_SDHOST_STATE_DATA_TRANSMIT = 0x6,
  BCM_SDHOST_STATE_DATA_RECEIVE  = 0x7,
  BCM_SDHOST_STATE_DATA_CRC      = 0x8,
  BCM_SDHOST_STATE_DATA_END      = 0x9,
  BCM_SDHOST_STATE_WAIT          = 0xa,
  BCM_SDHOST_STATE_STOP_CMD      = 0xb,
  BCM_SDHOST_STATE_STOP_RESPONSE = 0xc,
  BCM_SDHOST_STATE_STOP_CRC      = 0xd,
  BCM_SDHOST_STATE_AUTO_CMD      = 0xe,
  BCM_SDHOST_STATE_AUTO_RESPONSE = 0xf,
  BCM_SDHOST_STATE_UNKNOWN       = 0x10 
} bcm_sdhost_state_t;

const char* bcm_sdhost_edm_state_to_string(uint32_t edm)
{
  bcm_sdhost_state_t state = (sd_card_state_t)(edm & 0xf);

  switch (state) {
    case BCM_SDHOST_STATE_IDLE:          return "IDLE";
    case BCM_SDHOST_STATE_CMD_INIT:      return "CMD_INIT";
    case BCM_SDHOST_STATE_CMD_TRANSMIT:  return "CMD_TRANSMIT";
    case BCM_SDHOST_STATE_CMD_RESPONSE:  return "CMD_RESPONSE";
    case BCM_SDHOST_STATE_CMD_CRC:       return "CMD_CRC";
    case BCM_SDHOST_STATE_DATA_INIT:     return "DATA_INIT";
    case BCM_SDHOST_STATE_DATA_TRANSMIT: return "DATA_TRANSMIT";
    case BCM_SDHOST_STATE_DATA_RECEIVE:  return "DATA_RECEIVE";
    case BCM_SDHOST_STATE_DATA_CRC:      return "DATA_CRC";
    case BCM_SDHOST_STATE_DATA_END:      return "DATA_END";
    case BCM_SDHOST_STATE_WAIT:          return "WAIT";
    case BCM_SDHOST_STATE_STOP_CMD:      return "STOP_CMD";
    case BCM_SDHOST_STATE_STOP_RESPONSE: return "STOP_RESPONSE";
    case BCM_SDHOST_STATE_STOP_CRC:      return "STOP_CRC";
    case BCM_SDHOST_STATE_AUTO_CMD:      return "AUTO_CMD";
    case BCM_SDHOST_STATE_AUTO_RESPONSE: return "AUTO_RESPONSE";
    default:                             return "UNKNOWN";
  }
}

static void bcm_sdhost_dump_regs(bool full)
{
  BCM_SDHOST_LOG_INFO("cmd:0x%08x", ioreg32_read(SDHOST_CMD));
  BCM_SDHOST_LOG_INFO("hsts:0x%08x", ioreg32_read(SDHOST_HSTS));
  if (!full)
    return;

  BCM_SDHOST_LOG_INFO("edm:0x%08x", ioreg32_read(SDHOST_EDM));
  BCM_SDHOST_LOG_INFO("tout:0x%08x", ioreg32_read(SDHOST_TOUT));
  BCM_SDHOST_LOG_INFO("cdiv:0x%08x", ioreg32_read(SDHOST_CDIV));
  BCM_SDHOST_LOG_INFO("cfg:0x%08x", ioreg32_read(SDHOST_HCFG));
  BCM_SDHOST_LOG_INFO("cnt:0x%08x", ioreg32_read(SDHOST_CNT));
  BCM_SDHOST_LOG_INFO("resp0:0x%08x", ioreg32_read(SDHOST_RESP0));
  BCM_SDHOST_LOG_INFO("resp1:0x%08x", ioreg32_read(SDHOST_RESP1));
  BCM_SDHOST_LOG_INFO("resp2:0x%08x", ioreg32_read(SDHOST_RESP2));
  BCM_SDHOST_LOG_INFO("resp3:0x%08x", ioreg32_read(SDHOST_RESP3));
}

static void bcm_sdhost_irq(void)
{
  uint32_t hsts = ioreg32_read(SDHOST_HSTS);
  if (hsts & SDHOST_HSTS_BLOCK_IRPT) {
    uint32_t hcfg = ioreg32_read(SDHOST_HCFG);
    hcfg &= ~SDHOST_CFG_BLOCK_IRPT_EN;
    ioreg32_write(SDHOST_HCFG, hcfg);
    os_event_notify(&bcm_sdhost_event);
  }
  if (hsts & ~SDHOST_HSTS_BLOCK_IRPT) {
    printf("Unexpected irq: HSTS: %08x\r\n", hsts);
  }
}

static void bcm_sdhost_setup_dma_transfer(struct sdhc_io *io,
  int control_block, void *mem_addr, size_t block_size, size_t num_blocks,
  bool is_write)
{
  struct bcm2835_dma_request_param r = { 0 };
  uint32_t data_reg = PERIPH_ADDR_TO_DMA(SDHOST_DATA);
  uint32_t num_drain_bytes = sizeof(uint32_t) * FIFO_BUG_NUM_DRAIN_WORDS;

  r.dreq       = DMA_DREQ_SD_HOST;
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
    uint8_t *end = (uint8_t *)mem_addr + block_size * num_blocks;
    io->drain_addr = end - num_drain_bytes;
    r.len -= num_drain_bytes;
    r.dreq_type  = BCM2835_DMA_DREQ_TYPE_SRC;
    r.dst_type   = BCM2835_DMA_ENDPOINT_TYPE_INCREMENT;
    r.dst        = RAM_PHY_TO_BUS_UNCACHED(mem_addr);
    r.src_type   = BCM2835_DMA_ENDPOINT_TYPE_NOINC;
    r.src        = data_reg;
  }
  bcm2835_dma_program_cb(&r, control_block);
}

static int bcm_sdhost_dma_io(struct sd_cmd *c, bool is_write)
{
  uint32_t hsts;

  while(1) {
    hsts = ioreg32_read(SDHOST_HSTS);
    BCM_SDHOST_LOG_INFO("data HSTS: %08x EDM:%08x", hsts,
      ioreg32_read(SDHOST_EDM));

    if ((hsts & SDHSTS_DATA_FLAG) == 0)
      break;
  }

  while(1) {
    BCM_SDHOST_LOG_INFO("edm:%08x,hsts:%08x",
      ioreg32_read(SDHOST_EDM), ioreg32_read(SDHOST_HSTS));
  }

  /* If is read, invalidate cache now when DMA is done */
  if (!is_write)
    dcache_invalidate(c->databuf, c->num_blocks * c->block_size);

  return SUCCESS;
}

static inline void bcm_sdhost_wait_bytes_in_fifo(int i, bool dump)
{
  uint32_t edm;
  uint32_t hsts;
  int num_words;

  while(1) {
    edm = ioreg32_read(SDHOST_EDM);
    num_words = (edm >> 4) & 0x1f;
    if (dump) {
      hsts = ioreg32_read(SDHOST_HSTS);
      BCM_SDHOST_LOG_DBG2("#%d HSTS: %08x, EDM:%08x/%d", i, hsts, edm,
        num_words);
    }

    if (num_words)
      return;
  }
}

static int bcm_sdhost_data_write_sw(struct sdhc *s, const uint32_t *ptr,
  const uint32_t *end)
{
  int err;
  int wr_idx = 0;
  uint32_t hsts;
  uint32_t data;

  while (ptr != end) {
    data = *ptr++;

    while (1) {
      hsts = ioreg32_read(SDHOST_HSTS);

      if (hsts & SDHSTS_DATA_FLAG)
        break;

      BCM_SDHOST_LOG_DBG2("wr#%d(wait): hsts:%08x, edm:%08x", wr_idx, hsts,
        ioreg32_read(SDHOST_EDM));
    }

    ioreg32_write(SDHOST_DATA, data);

    BCM_SDHOST_LOG_DBG2("wr#%d(done): %08x, hsts:%08x,edm:%08x", wr_idx, data,
        ioreg32_read(SDHOST_HSTS),
        ioreg32_read(SDHOST_EDM));
    wr_idx++;
  }

#if 0
  for (int i = 0; i < 32; ++i) {
    BCM_SDHOST_LOG_DBG2("wr_compl hsts:%08x, edm:%08x\r\n",
      ioreg32_read(SDHOST_HSTS),
      ioreg32_read(SDHOST_EDM));
  }
#endif

  return SUCCESS;
}

static int bcm_sdhost_data_read_sw(struct sdhc *s, uint32_t *ptr,
  uint32_t *end)
{
  int num_words;
  uint32_t edm;
  int err;
  uint32_t data;

  int i = 0;
  while (ptr != end) {
    bcm_sdhost_wait_bytes_in_fifo(i++, /* log_hsts */ true);
    edm = ioreg32_read(SDHOST_EDM);
    num_words = (edm >> 4) & 0x1f;
    data = ioreg32_read(SDHOST_DATA);
    BCM_SDHOST_LOG_DBG("EDM:%08x, num_words:%d", edm, num_words);
    BCM_SDHOST_LOG_DBG2("data:%08x", data);
    *ptr++ = data;
  }

  return SUCCESS;
}

static int bcm_sdhost_data_blocking(struct sdhc *s, struct sd_cmd *c,
  bool is_write)
{
  int ret;
  uint32_t *ptr;
  uint32_t *end;

  if (s->io_mode == SDHC_IO_MODE_BLOCKING_DMA)
    return bcm_sdhost_dma_io(c, is_write);

  /* Do data IO by manually copying bytes from/to data register */
  ptr = (uint32_t *)c->databuf;
  end = ptr + (c->block_size * c->num_blocks) / 4;

  if (is_write)
    return bcm_sdhost_data_write_sw(s, ptr, end);

  return bcm_sdhost_data_read_sw(s, ptr, end);
}

static void bcm_sdhost_cmd_prep_dma(struct sdhc *s, struct sd_cmd *c,
  bool is_write)
{
  s->io.c = c;

  /* Setup DMA control block: destination, source, number of copies, etc. */
  bcm_sdhost_setup_dma_transfer(
    &s->io,
    s->io.dma_control_block_idx,
    s->io.c->databuf,
    s->io.c->block_size,
    s->io.c->num_blocks, is_write);

  /* Assign DMA control block to channel */
  bcm2835_dma_set_cb(s->io.dma_channel, s->io.dma_control_block_idx);

  if (is_write)
    dcache_invalidate(c->databuf, c->num_blocks * c->block_size);

  /*
   * Activates DMA channel, DMA will copy 4byte word each time sdhost asserts
   * DREQ signal
   */
  bcm2835_dma_activate(s->io.dma_channel);
}

/*
 * There is a bug in bcm2835 sdhost. When when READ_MULTIPLE_BLOCKS(CMD18)
 * or WRITE_MULTIPLE_BLOCKS(CMD25) commands are used in combination with
 * SET_BLOCK_COUNT (CMD23), after all data has been read-out, HSTS will first
 * switch to 0, but then will show 0x80 (TIMEOUT). EDM will show that 0 words
 * are in fifo and FSM state == 4.
 * So this 4 means READWAIT, but actually FSM should go to state 1, which is
 * DATAMODE. So we force sd controller FSM to DATAMODE by FORCE_DATA_MODE bit
 * each time MULTI-io is completed
 */
static void bcm_sdhost_on_data_done(void)
{
  uint32_t edm = ioreg32_read(SDHOST_EDM);
  if ((edm & SDHOST_EDM_FSM_MASK) != SDHOST_EDM_FSM_READWAIT)
    return;

  ioreg32_write(SDHOST_EDM, edm | SDHOST_EDM_FORCE_DATA_MODE);
  BCM_SDHOST_LOG_DBG2("read done, FORCE_DATA_MODE:edm:%08x->%08x", edm,
    ioreg32_read(SDHOST_EDM));
}

static int bcm_sdhost_cmd_step0(struct sdhc *s, struct sd_cmd *c,
  uint64_t timeout_usec)
{
  int ret;
  uint32_t reg_cmd;
  uint32_t reg_hsts;
  bool is_write;
  bool has_data;

  reg_cmd = s->is_acmd_context
    ? sd_acommands[c->cmd_idx]
    : sd_commands[c->cmd_idx];

  reg_cmd |= BIT(SDHOST_CMD_BIT_NEW);
  has_data = reg_cmd & SDHOST_CMD_HAS_DATA_MASK;

  if (has_data) {
    ioreg32_write(SDHOST_HBCT, c->block_size);
    ioreg32_write(SDHOST_HBLC, c->num_blocks);
    is_write = reg_cmd & SDHOST_CMD_WRITE_CMD;
    if (s->io_mode == SDHC_IO_MODE_BLOCKING_DMA
      || s->io_mode == SDHC_IO_MODE_IT_DMA)
      bcm_sdhost_cmd_prep_dma(s, c, is_write);
  }

  os_event_clear(&bcm_sdhost_event);

  if (bcm_sdhost_log_level >= LOG_LEVEL_DEBUG2)
    BCM_SDHOST_LOG_DBG2("CMD: old:%08x,set:0x%08x, ARG:old:0x%08x,new:%08x",
      ioreg32_read(SDHOST_CMD), reg_cmd, ioreg32_read(SDHOST_ARG), c->arg);

  ioreg32_write(SDHOST_ARG, c->arg);
  ioreg32_write(SDHOST_CMD, reg_cmd);

  if (has_data && s->io_mode == SDHC_IO_MODE_IT_DMA) {
    os_event_wait(&bcm_sdhost_event);
    if (is_write) {
      uint32_t hsts = ioreg32_read(SDHOST_HSTS);
      if (hsts & SDHSTS_DATA_FLAG) {
        uint32_t hcfg;
        irq_disable();
        os_event_clear(&bcm_sdhost_event);
        hcfg = ioreg32_read(SDHOST_HCFG);
        hcfg |= SDHOST_CFG_BUSY_IRPT_EN | SDHOST_CFG_BLOCK_IRPT_EN;
        ioreg32_write(SDHOST_HCFG, hcfg);
        irq_enable();
        os_event_wait(&bcm_sdhost_event);
      }
    } else {
      /* IS DMA READ */
      /* Read steps should be carefully arranged, because we will need to
       * invalidate cache + clear FSM in time + add 2 last 32bit words to the
       * read results. Invalidate cache is a time-consuming operation, that's
       * why we have to finalize FIFO FSM before timeout and 0x80 in HSTS.
       * After FIFO FSM has been completed, HSTS will be ok, so we can
       * invalidate at this point, but we can only add last 2 words to result
       * only after invalide cache has been done, for this reason we need to
       * first read the data to temporary variables.
       *
       * 1. Read 2 last words from FIFO which DMA could not (silicon bug#0 
       *    workaround) to clear SDHOST's internal states
       * 2. reset SDHOST FSM back to IDLE (silicon bug#0 workaround)
       * 3. invalidate data cache above DMA-copied addresses to make data
       *    available from CPU side.
       * 4. Copy last two words on top of invalidated cache to actualize
       *    cache line with data also from CPU.
       */
      uint32_t data0 = ioreg32_read(SDHOST_DATA);
      uint32_t data1 = ioreg32_read(SDHOST_DATA);
      bcm_sdhost_on_data_done();
      dcache_invalidate(c->databuf, c->num_blocks * c->block_size);
      ((uint32_t *)s->io.drain_addr)[0] = data0;
      ((uint32_t *)s->io.drain_addr)[1] = data1;
    }

    ret = SUCCESS;
    goto data_completion;
  }

  while(1) {
    reg_cmd = ioreg32_read(SDHOST_CMD);
    if (BIT_IS_CLEAR(reg_cmd, SDHOST_CMD_BIT_NEW))
      break;

    BCM_SDHOST_LOG_DBG2("wait_new,cmd:%08x,edm:%08x,hsts:%08x",
      reg_cmd, ioreg32_read(SDHOST_EDM), ioreg32_read(SDHOST_HSTS));
  }

  /* Clear possible error state from previous command */
  if (BIT_IS_SET(reg_cmd, SDHOST_CMD_BIT_FAIL)) {
    ret = ERR_GENERIC;
    goto out;
  }

  if (has_data)
    ret = bcm_sdhost_data_blocking(s, c, is_write);
  else
    ret = SUCCESS;

data_completion:
  if (has_data)
    bcm_sdhost_on_data_done();

  c->resp0 = ioreg32_read(SDHOST_RESP0);
  c->resp1 = ioreg32_read(SDHOST_RESP1);
  c->resp2 = ioreg32_read(SDHOST_RESP2);
  c->resp3 = ioreg32_read(SDHOST_RESP3);

out:
  if (ret != SUCCESS)
    bcm_sdhost_dump_regs(false);
  return ret;
}

static int bcm_sdhost_acmd(struct sdhc *s, struct sd_cmd *c,
  uint64_t timeout_usec)
{
  int ret;
  int acmd_idx;
  int status;
  struct sd_cmd acmd;
  struct sd_cmd cmd;

  sd_cmd_init(&acmd, SDHC_CMD55, c->rca << 16);

  acmd.num_blocks = c->num_blocks;
  acmd.block_size = c->block_size;

  ret = s->ops->cmd(s, &acmd, timeout_usec);
  if (ret != SUCCESS)
    return ret;

  s->is_acmd_context = true;
  memcpy(&cmd, c, sizeof(cmd));
  cmd.cmd_idx &= 0x7fffffff;
  ret = s->ops->cmd(s, &cmd, timeout_usec);
  c->resp0 = cmd.resp0;
  c->resp1 = cmd.resp1;
  c->resp2 = cmd.resp2;
  c->resp3 = cmd.resp3;
  c->status = cmd.status;
  s->is_acmd_context = false;
  return ret;
}

int bcm_sdhost_cmd(struct sdhc *s, struct sd_cmd *c, uint64_t timeout_usec)
{
  int ret;
  uint32_t reg_cmd;
  uint32_t reg_hsts;

  const bool is_acmd = c->cmd_idx & 0x80000000;

  BCM_SDHOST_LOG_DBG("%sCMD%d started (%d bytes block x %d)",
    is_acmd ? "A" : "", c->cmd_idx & 0x7fffffff, c->block_size, c->num_blocks);

  if (is_acmd)
    return bcm_sdhost_acmd(s, c, timeout_usec);

  /* Wait for prev CMD to finish */
  while (1) {
    reg_cmd = ioreg32_read(SDHOST_CMD);
    if (BIT_IS_CLEAR(reg_cmd, SDHOST_CMD_BIT_NEW))
      break;
    BCM_SDHOST_LOG_DBG2("--wait_prev:-cmd %08x, hsts:%08x", reg_cmd,
      ioreg32_read(SDHOST_HSTS));
  }

  /* Clear prev CMD errors */
  reg_hsts = ioreg32_read(SDHOST_HSTS);
  if (reg_hsts & SDHOST_HSTS_ERROR_MASK) {
    BCM_SDHOST_LOG_WARN("HSTS had errors: %08x", reg_hsts);
    ioreg32_write(SDHOST_HSTS, reg_hsts);
  }

  ret = bcm_sdhost_cmd_step0(s, c, timeout_usec);

  BCM_SDHOST_LOG_DBG("%sCMD%d done, ret: %d, resp:%08x|%08x|%08x|%08x",
    is_acmd ? "A" : "", c->cmd_idx & 0x7fffffff, ret,
    c->resp0, c->resp1, c->resp2, c->resp3);

  return ret;
}

static inline void bcm_sdhost_sleep_ms(uint32_t ms)
{
  /* If scheduler is running, use os sleep, else use baremetal busy wait */
  if (sched_get_current_task())
    os_wait_ms(ms);
  else
    delay_us(ms * 1000);
}

static void bcm_sdhost_reset_registers(void)
{
  uint32_t r;

  const uint32_t hcfg = SDHOST_CFG_WIDE_INT_BUS | SDHOST_CFG_SLOW_CARD;
  const uint32_t cdiv_slow_clock = 0x3e6;

  const uint32_t edm_clear_mask =
    (SDHOST_EDM_THRESHOLD_MASK << SDHOST_EDM_READ_THRESHOLD_SHIFT) |
    (SDHOST_EDM_THRESHOLD_MASK << SDHOST_EDM_WRITE_THRESHOLD_SHIFT);

  const uint32_t edm_set_mask = 
    (FIFO_READ_THRESHOLD << SDHOST_EDM_READ_THRESHOLD_SHIFT) |
    (FIFO_WRITE_THRESHOLD << SDHOST_EDM_WRITE_THRESHOLD_SHIFT);

  bcm_sdhost_dump_regs(true);

  /* Power down */
  ioreg32_write(SDHOST_VDD, 0);


  ioreg32_write(SDHOST_CMD, 0);
  ioreg32_write(SDHOST_ARG, 0);
  ioreg32_write(SDHOST_TOUT, 0xf00000);
  ioreg32_write(SDHOST_CDIV, 0);
  ioreg32_write(SDHOST_HSTS, 0x7f8);
  ioreg32_write(SDHOST_HCFG, 0);
  ioreg32_write(SDHOST_HBCT, 0);
  ioreg32_write(SDHOST_HBLC, 0);

  /* Workaround for undocumented silicon bug, copied from linux kernel code */
  r = ioreg32_read(SDHOST_EDM);
  r &= ~edm_clear_mask;
  r |= edm_set_mask;
  BCM_SDHOST_LOG_DBG2("reset: EDM=%08x", r);
  ioreg32_write(SDHOST_EDM, r);

  bcm_sdhost_sleep_ms(20);

  /* Power up */
  ioreg32_write(SDHOST_VDD, 1);

  bcm_sdhost_sleep_ms(20);

  BCM_SDHOST_LOG_DBG2("slow_hcfg=%08x, slow_clock=%08x", hcfg, cdiv_slow_clock);
  ioreg32_write(SDHOST_HCFG, hcfg);
  ioreg32_write(SDHOST_CDIV, cdiv_slow_clock);
}

static inline void bcm_sdhost_dump_fsm_state(void)
{
  uint32_t r = ioreg32_read(SDHOST_EDM);
  int state = r & SDHOST_EDM_FSM_MASK;

  BCM_SDHOST_LOG_INFO("sdhost FSM state: %d (%s)", state,
    bcm_sdhost_edm_state_to_string(r));
}

static void bcm_sdhost_set_bus_width4(void)
{
  uint32_t hcfg;

  hcfg = ioreg32_read(SDHOST_HCFG);
  hcfg |= SDHOST_CFG_WIDE_EXT_BUS;
  ioreg32_write(SDHOST_HCFG, hcfg);
  BCM_SDHOST_LOG_DBG2("set_bus_width4: HCFG=%08x", hcfg);
  BCM_SDHOST_LOG_DBG("set_bus_width4 done");
}

static void bcm_sdhost_get_max_clock(void)
{
  bool clk_enabled;
  int parent_clock_id;
  int clock_div;
  const char *parent_clock_name;

  if (!bcm_cm_get_clock_info(BCM2835_CLOCK_VPU, &clk_enabled,
    &clock_div, &parent_clock_id, &parent_clock_name)) {
    BCM_SDHOST_LOG_ERR("Failed to get clock source");
    return;
  }

  BCM_SDHOST_LOG_DBG("VCPU clock: ena:%d, div:%08x, src:%d(%s)",
    clk_enabled, clock_div, parent_clock_id, parent_clock_name);

}

static void bcm_sdhost_set_high_speed_clock(void)
{
#if BCM_SDHOST_SET_CLOCK == BCM_SDHOST_SET_CLOCK_MANUAL
  const uint32_t cdiv = 6;
  ioreg32_write(SDHOST_CDIV, cdiv);
#elif BCM_SDHOST_SET_CLOCK == BCM_SDHOST_SET_CLOCK_BY_FIRMWARE
  uint32_t clock_freq = 50000000;
  if (!mbox_set_sdhost_clock_freq(&clock_freq)) {
    BCM_SDHOST_LOG_ERR("Failed to set clock via mbox: %d, CDIV:%08x",
      clock_freq, ioreg32_read(SDHOST_CDIV));
  }
#endif
  BCM_SDHOST_LOG_DBG2("highspeed_clock=%d, hcfg:%08x",
    ioreg32_read(SDHOST_CDIV), ioreg32_read(SDHOST_HCFG));
}

static void bcm_sdhost_init_gpio(void)
{
  int i;
  for (i = 48; i < 48 + 6; i++) {
    gpio_set_pin_function(i, GPIO_FUNCTION_ALT_0);
    gpio_set_pin_pullupdown_mode(i, GPIO_PULLUPDOWN_MODE_UP);
  }
}

static int bcm_sdhost_init(void)
{
  bcm_sdhost_log_level = LOG_LEVEL_INFO;
  bcm_sdhost_get_max_clock();

  irq_set(BCM2835_IRQNR_SDHOST, bcm_sdhost_irq);
  bcm2835_ic_disable_irq(BCM2835_IRQNR_SDHOST);

  os_event_init(&bcm_sdhost_event);
  return SUCCESS;
}

static int bcm_sdhost_set_io_mode(struct sdhc *s, sdhc_io_mode_t mode)
{
  uint32_t hcfg = 0;

  BCM_SDHOST_LOG_DBG("bcm_sdhost_set_io_mode");
  if (mode == SDHC_IO_MODE_IT_DMA) {
    hcfg = ioreg32_read(SDHOST_HCFG);
    hcfg |= SDHOST_CFG_BUSY_IRPT_EN;
    ioreg32_write(SDHOST_HCFG, hcfg);
    bcm2835_ic_enable_irq(BCM2835_IRQNR_SDHOST);
  }
  return SUCCESS;
}

static void bcm_sdhost_notify_dma(struct sdhc *s)
{
  os_event_notify(&bcm_sdhost_event);
}

struct sdhc_ops bcm_sdhost_ops = {
  .init = bcm_sdhost_init,
  .init_gpio = bcm_sdhost_init_gpio,
  .reset_regs = bcm_sdhost_reset_registers,
  .dump_fsm_state = bcm_sdhost_dump_fsm_state,
  .dump_regs = bcm_sdhost_dump_regs,
  .set_bus_width4 = bcm_sdhost_set_bus_width4,
  .set_high_speed_clock = bcm_sdhost_set_high_speed_clock,
  .set_io_mode = bcm_sdhost_set_io_mode,
  .cmd = bcm_sdhost_cmd,
  .notify_dma = bcm_sdhost_notify_dma
};
