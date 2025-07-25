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
#include "memory_map.h"

#define SD_HOST_TIMEOUT_USEC_DEFAULT 1000000
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

#define FIFO_READ_THRESHOLD   2
#define FIFO_WRITE_THRESHOLD  2
#define SDDATA_FIFO_PIO_BURST 8

#define delay(x) for (volatile int i = 0; i < x * 1000; i++) { asm volatile("nop"); }
#define CMD_READ_SINGLE_BLOCK 17

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

struct sd_host {
  struct sdhc_io io;
  bool initialized;
  bool blocking_mode;
  bool is_acmd_context;
  uint32_t rca;
  uint32_t device_id[4];
  sd_card_capacity_t card_capacity;
  bool UHS_II_support;
  bool SDUC_support;
  bool bus_width_4;
};

static struct sd_host sdhost = {
  .initialized = false,
  .blocking_mode = true,
  .bus_width_4 = false
};

int bcm_sdhost_log_level = LOG_LEVEL_DEBUG2;

bool use_dma = false;

static void bcm_sdhost_dump_regs(bool full)
{
  printf("cmd:0x%08x\r\n", ioreg32_read(SDHOST_CMD));
  printf("hsts:0x%08x\r\n", ioreg32_read(SDHOST_HSTS));
  if (!full)
    return;

  printf("edm:0x%08x\r\n", ioreg32_read(SDHOST_EDM));
  printf("tout:0x%08x\r\n", ioreg32_read(SDHOST_TOUT));
  printf("cdiv:0x%08x\r\n", ioreg32_read(SDHOST_CDIV));
  printf("cfg:0x%08x\r\n", ioreg32_read(SDHOST_HCFG));
  printf("cnt:0x%08x\r\n", ioreg32_read(SDHOST_CNT));
  printf("resp0:0x%08x\r\n", ioreg32_read(SDHOST_RESP0));
  printf("resp1:0x%08x\r\n", ioreg32_read(SDHOST_RESP1));
  printf("resp2:0x%08x\r\n", ioreg32_read(SDHOST_RESP2));
  printf("resp3:0x%08x\r\n", ioreg32_read(SDHOST_RESP3));
}

static void sdhost_on_dma_done(void)
{

}

int sdhc_io_init(struct sdhc_io *io, void (*on_dma_done)(void))
{
  io->dma_channel = bcm2835_dma_request_channel();
  io->dma_control_block_idx = bcm2835_dma_reserve_cb();
  printf("sdhc_io_init: dma_channel:%d cb_idx:%d\r\n", io->dma_channel, io->dma_control_block_idx);

  if (io->dma_channel == -1 || io->dma_control_block_idx == -1)
    return ERR_GENERIC;

  bcm2835_dma_set_irq_callback(io->dma_channel, on_dma_done);

  bcm2835_dma_irq_enable(io->dma_channel);
  bcm2835_dma_enable(io->dma_channel);
  bcm2835_dma_reset(io->dma_channel);

  return SUCCESS;
}


static void bcm_sdhost_setup_dma_transfer(int control_block,
  void *mem_addr, size_t block_size, size_t num_blocks, bool is_write)
{
  struct bcm2835_dma_request_param r = { 0 };
  uint32_t data_reg = PERIPH_ADDR_TO_DMA(SDHOST_DATA);

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
    r.dreq_type  = BCM2835_DMA_DREQ_TYPE_SRC;
    r.dst_type   = BCM2835_DMA_ENDPOINT_TYPE_INCREMENT;
    r.dst        = RAM_PHY_TO_BUS_UNCACHED(mem_addr);
    r.src_type   = BCM2835_DMA_ENDPOINT_TYPE_NOINC;
    r.src        = data_reg;
  }
  bcm2835_dma_program_cb(&r, control_block);
}

static int bcm_sdhost_cmd(struct sd_cmd *c, uint64_t timeout_usec)
{
#if 0
  uint32_t cmdreg = bcm2835_emmc.io.cmdreg;

  bool is_data = BCM2835_EMMC_CMDTM_GET_CMD_ISDATA(cmdreg);
  bool is_write;

  if (is_data) {
    is_write = BCM2835_EMMC_CMDTM_GET_TM_DAT_DIR(cmdreg) ==
      BCM2835_EMMC_TRANS_TYPE_DATA_HOST_TO_CARD;
    bcm_sdhost_setup_dma_transfer(bcm2835_emmc.io.dma_channel,
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
#endif
  return 0;
}

static int bcm_sdhost_dma_io(struct sd_cmd *c, bool is_write)
{
  uint32_t hsts;

  if (is_write) {
    printf("DMA write not supported yet\r\n");
    return ERR_NOTSUPP;
  }

  while(1) {
    hsts = ioreg32_read(SDHOST_HSTS);
    printf("data HSTS: %08x EDM:%08x\r\n", hsts, ioreg32_read(SDHOST_EDM));
    if (hsts & SDHSTS_DATA_FLAG)
      break;
  }

  dcache_invalidate(c->databuf, c->num_blocks * c->block_size);
  for (int i = 0; i < c->num_blocks * c->block_size; ++i)
    printf("%02x", ((const uint8_t *)c->databuf)[i]);
  printf("\r\n");
  return SUCCESS;
}

static inline void bcm_sdhost_wait_data_ready_bit(bool log_hsts)
{
  uint32_t hsts;

  while(1) {
    hsts = ioreg32_read(SDHOST_HSTS);
    if (log_hsts)
      printf("HSTS: %08x\r\n", hsts);

    if (hsts & SDHSTS_DATA_FLAG)
      return;
  }
}

static inline int bcm_sdhost_read_data_4bit_bus(uint32_t *out, bool log_edm)
{
  uint32_t result = 0;
  uint32_t edm;
  int num_words;
  /* Wait until EDM shows 4 words */
  for (int i = 0; i < 4; ++i) {
    edm = ioreg32_read(SDHOST_EDM);
    num_words = (edm >> 4) & 0x1f;
    if (num_words == 0)
      return ERR_IO;

    if (log_edm)
      printf("EDM:%08x, num_words:%d\r\n", edm, num_words);
    result |= (ioreg32_read(SDHOST_DATA) & 0xff) << (i * 8);
  }
  *out = result;
  return SUCCESS;
}

static inline int bcm_sdhost_write_data_4bit_bus(uint32_t data, bool log_edm)
{
  int num_bytes = 0;
  uint32_t result = 0;
  uint32_t edm;
  int num_words;
  /* Wait until EDM shows 4 words */
  for (int i = 0; i < 4; ++i) {
    edm = ioreg32_read(SDHOST_EDM);
    num_words = (edm >> 4) & 0x1f;
    if (num_words == 0)
      return ERR_IO;

    if (log_edm)
      printf("EDM:%08x, num_words:%d\r\n", edm, num_words);
    result = (result << 8) | ioreg32_read(SDHOST_DATA) & 0xff;
    num_bytes++;
  }
  return SUCCESS;
}

static int bcm_sdhost_data_write_sw(const uint32_t *ptr, const uint32_t *end)
{
  int ret;

  /* Wait for data ready flag */
  bcm_sdhost_wait_data_ready_bit(/* log_hsts */ true);

  while (ptr != end) {
    if (sdhost.bus_width_4) {
      ret = bcm_sdhost_write_data_4bit_bus(*ptr++, /* log_edm */ true);
      if (ret) {
        printf("write on bus width4 error: %d\r\n", ret);
        return ret;
      }
    }
    else
      ioreg32_write(SDHOST_DATA, *ptr++);

    bcm_sdhost_wait_data_ready_bit(/* log_hsts */ true);
  }

  return SUCCESS;
}

static int bcm_sdhost_data_read_sw(uint32_t *ptr, uint32_t *end)
{
  int ret;
  uint32_t data;

  while (ptr != end) {
    /* Wait for data ready flag */
    bcm_sdhost_wait_data_ready_bit(/* log_hsts */ true);

    if (sdhost.bus_width_4) {
      ret = bcm_sdhost_read_data_4bit_bus(&data, /* log_edm */ true);
      if (ret) {
        printf("reading on bus width4 error: %d\r\n", ret);
        return ret;
      }
    }
    else
      data = ioreg32_read(SDHOST_DATA);
    printf("data:%08x\r\n", data);
    *ptr++ = data;
  }

  return SUCCESS;
}

static int bcm_sdhost_cmd_data_phase(struct sd_cmd *c, bool is_write,
  bool use_dma)
{
  int ret;
  uint32_t *ptr;
  uint32_t *end;

  if (use_dma)
    return bcm_sdhost_dma_io(c, is_write);

  /* Do data IO by manually copying bytes from/to data register */
  ptr = (uint32_t *)c->databuf;
  end = ptr + (c->block_size * c->num_blocks) / 4;

  if (is_write)
    return bcm_sdhost_data_write_sw(ptr, end);

  return bcm_sdhost_data_read_sw(ptr, end);
}

static void bcm_sdhost_cmd_prep_data(struct sd_cmd *c, bool is_write)
{
  if (!use_dma)
    return;

  memset(c->databuf, 0, c->block_size * c->num_blocks);
  sdhost.io.c = c;
  printf("prep_data:is_write:%d\r\n", is_write);

  /* setup read DMA */
  bcm_sdhost_setup_dma_transfer(
    sdhost.io.dma_control_block_idx,
    sdhost.io.c->databuf,
    sdhost.io.c->block_size,
    sdhost.io.c->num_blocks, is_write);

  bcm2835_dma_set_cb(sdhost.io.dma_channel,
    sdhost.io.dma_control_block_idx);

  printf("dma_activate:channel:%d\r\n", sdhost.io.dma_channel);
  bcm2835_dma_activate(sdhost.io.dma_channel);
}

static int bcm_sdhost_cmd_blocking(struct sd_cmd *c, uint64_t timeout_usec)
{
  int ret = SUCCESS;
  uint32_t r;
  uint32_t status;
  bool is_write;
  bool has_data;

  uint32_t cmd_reg = sdhost.is_acmd_context
    ? sd_acommands[c->cmd_idx]
    : sd_commands[c->cmd_idx];

  cmd_reg |= BIT(SDHOST_CMD_BIT_NEW);
  has_data = cmd_reg & SDHOST_CMD_HAS_DATA_MASK;

  if (has_data) {
    is_write = cmd_reg & SDHOST_CMD_WRITE_CMD;
    bcm_sdhost_cmd_prep_data(c, is_write);
  }

  printf("CMD(new):0x%08x, ARG:0x%08x, CMD(current):0x%08x\r\n", cmd_reg,
    ioreg32_read(SDHOST_ARG), ioreg32_read(SDHOST_CMD));
  ioreg32_write(SDHOST_CMD, cmd_reg);

  /* Wait prev command completion */
  while(1) {
    r = ioreg32_read(SDHOST_CMD);
    if (BIT_IS_CLEAR(r, SDHOST_CMD_BIT_NEW))
      break;
  }

  /* Clear possible error state from previous command */
  if (BIT_IS_SET(r, SDHOST_CMD_BIT_FAIL)) {
    status = ioreg32_read(SDHOST_HSTS);
    ret = ERR_GENERIC;
    goto out;
  }

  if (has_data)
    ret = bcm_sdhost_cmd_data_phase(c, is_write, use_dma);

  c->resp0 = ioreg32_read(SDHOST_RESP0);
  c->resp1 = ioreg32_read(SDHOST_RESP1);
  c->resp2 = ioreg32_read(SDHOST_RESP2);
  c->resp3 = ioreg32_read(SDHOST_RESP3);

out:
  printf("CMD%d (ACMD:%d) done, ret:%d, HSTS:%08x,resp:%08x|%08x|%08x|%08x\r\n", c->cmd_idx,
    sdhost.is_acmd_context, ret, ioreg32_read(SDHOST_HSTS), c->resp0, c->resp1, c->resp2, c->resp3);

  if (ret != SUCCESS)
    bcm_sdhost_dump_regs(false);
  return ret;
}

static int bcm_sdhost_acmd(struct sd_cmd *c, uint64_t timeout_usec,
  bool blocking)
{
  int ret;
  int acmd_idx;
  int status;
  struct sd_cmd acmd;
  struct sd_cmd cmd;

  sd_cmd_init(&acmd, SDHC_CMD55, c->rca << 16);

  acmd.num_blocks = c->num_blocks;
  acmd.block_size = c->block_size;

  ret = sdhc_cmd(&acmd, timeout_usec, blocking);

  if (ret != SUCCESS)
    return ret;

  sdhost.is_acmd_context = true;
  memcpy(&cmd, c, sizeof(cmd));
  cmd.cmd_idx &= 0x7fffffff;
  ret = sdhc_cmd(&cmd, timeout_usec, blocking);
  c->resp0 = cmd.resp0;
  c->resp1 = cmd.resp1;
  c->resp2 = cmd.resp2;
  c->resp3 = cmd.resp3;
  c->status = cmd.status;
  sdhost.is_acmd_context = false;
  return ret;
}

int sdhc_cmd(struct sd_cmd *c, uint64_t timeout_usec,
  bool blocking)
{
  uint32_t r;

  const bool is_acmd = c->cmd_idx & 0x80000000;

  printf("CMD%d (is_acmd=%d) started %d/%d\r\n", c->cmd_idx & 0x7fffffff, is_acmd,
    c->block_size, c->num_blocks);

  if (is_acmd)
    return bcm_sdhost_acmd(c, timeout_usec, blocking);

  /* Wait for prev CMD to finish */
  while (1) {
    r = ioreg32_read(SDHOST_CMD);
    if (BIT_IS_CLEAR(r, SDHOST_CMD_BIT_NEW))
      break;

    printf("CMD still busy\r\n");
  }

  /* Clear prev CMD errors */
  r = ioreg32_read(SDHOST_HSTS);
  if (r & SDHOST_HSTS_ERROR_MASK) {
    printf("HSTS had errors: %08x\r\n", r);
    ioreg32_write(SDHOST_HSTS, r);
  }

  ioreg32_write(SDHOST_HBCT, c->block_size);
  ioreg32_write(SDHOST_HBLC, c->num_blocks);
  ioreg32_write(SDHOST_ARG, c->arg);

  if (blocking)
    return bcm_sdhost_cmd_blocking(c, timeout_usec);

  return bcm_sdhost_cmd(c, timeout_usec);
}

static void bcm_sdhost_reset_registers(void)
{
  uint32_t r;

  const uint32_t edm_clear_mask =
    (SDHOST_EDM_THRESHOLD_MASK << SDHOST_EDM_READ_THRESHOLD_SHIFT) |
    (SDHOST_EDM_THRESHOLD_MASK << SDHOST_EDM_WRITE_THRESHOLD_SHIFT);

  const uint32_t edm_set_mask = 
    (FIFO_READ_THRESHOLD << SDHOST_EDM_READ_THRESHOLD_SHIFT) |
    (FIFO_WRITE_THRESHOLD << SDHOST_EDM_WRITE_THRESHOLD_SHIFT);

  /* Power down */
  ioreg32_write(SDHOST_VDD, 0);

  bcm_sdhost_dump_regs(true);

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
  ioreg32_write(SDHOST_EDM, r);

  delay_us(20 * 1000);

  /* Power up */
  ioreg32_write(SDHOST_VDD, 1);

  delay_us(20 * 1000);
  r = ioreg32_read(SDHOST_HCFG);
  r &= ~SDHOST_CFG_WIDE_EXT_BUS;
  ioreg32_write(SDHOST_HCFG, r);

  // r |= SDHOST_CFG_WIDE_INT_BUS;
  // r |= SDHOST_CFG_SLOW_CARD;
  // ioreg32_write(SDHOST_HCFG, r);
  ioreg32_write(SDHOST_HCFG, SDHOST_CFG_WIDE_INT_BUS | SDHOST_CFG_SLOW_CARD);
  ioreg32_write(SDHOST_CDIV, SDHOST_MAX_CDIV);

  bcm_sdhost_dump_regs(true);
  delay_us(20 * 1000);
}

static inline void bcm_sdhost_dump_fsm_state(void)
{
  uint32_t r = ioreg32_read(SDHOST_EDM);
  int state = r & SDHOST_EDM_FSM_MASK;

  printf("sdhost FSM state: %d (%s)\r\n", state,
    bcm_sdhost_edm_state_to_string(r));
}

/*
 * "Physical Layer Simplified Specification Version 9.00"
 * SD host at initialization is in identification mode to know which cards
 * are available. In this mode it queries cards and knows their addresses
 * During that SD cards transition states in a defined order.
 * We want to set the card to STANDBY state and move SD host to data
 * transfer state.
 * For that we:
 * 1. Reset SD card back to IDLE state CMD0.
 * 2. Issue commands to traverse states:
 *    CDM0->IDLE->CMD8,CMD41->READY->CMD2->IDENT->CMD3->STANDBY
 */
int sdhc_identification(uint32_t *rca, bool blocking)
{
  int err;
  sd_card_state_t card_state;
  uint32_t sdcard_state;
  uint32_t device_id[4];

  err = sdhc_cmd0(SD_HOST_TIMEOUT_USEC_DEFAULT, blocking);
  BCM_SDHOST_CHECK_ERR("Failed at CMD0 (GO_TO_IDLE)");

  /* card_state = IDLE */

  /* CMD8 -SEND_IF_COND - this checks if SD card supports our voltage */
  err = sdhc_cmd8(SD_HOST_TIMEOUT_USEC_DEFAULT, blocking);
  BCM_SDHOST_CHECK_ERR("CMD8 (SEND_IF_COND) failed");
  /* card_state = IDLE */

  printf("will run ACMD41\r\n");
  uint32_t resp, arg;
  err = sdhc_acmd41(0, 0, &resp, SD_HOST_TIMEOUT_USEC_DEFAULT, blocking);
  BCM_SDHOST_CHECK_ERR("ACMD41 (GET_OCR) failed");

  printf("ACMD41: %08x\r\n", resp);
  arg = (resp & 0x00ff8000) | (1<<30);

  sdhost.card_capacity = SD_CARD_CAPACITY_SDSC;
  sdhost.UHS_II_support = false;
  sdhost.SDUC_support = false;
  while (1) {
    err = sdhc_acmd41(0x40ff8000, 0, &resp, SD_HOST_TIMEOUT_USEC_DEFAULT,
      blocking);
    BCM_SDHOST_CHECK_ERR("ACMD41 (GET_OCR) repeated failed");
    printf("REPEATED ACMD41, resp:%08x\r\n", resp);
    printf("ACMD41 repeat: arg=0x40ff8000, resp: %08x\r\n", resp);
    if (resp & (1<<31))
      break;
  }

  if (resp & (1<<30))
    sdhost.card_capacity = SD_CARD_CAPACITY_SDHC;
  if (resp & (1<<29))
    sdhost.UHS_II_support = true;
  if (resp & (1<<29))
    sdhost.SDUC_support = true;

  printf("Card capacity:%d, UHS-II support: %d, SDUC_support:%d\r\n",
    sdhost.card_capacity,
    sdhost.UHS_II_support,
    sdhost.SDUC_support);
  /* card_state = READY */

  /* Get device id */
  err = sdhc_cmd2(device_id, SD_HOST_TIMEOUT_USEC_DEFAULT, blocking);
  BCM_SDHOST_CHECK_ERR("failed at CMD2 (SEND_ALL_CID) step");
  /* card_state = IDENT */

  BCM_SDHOST_LOG_INFO("device_id: %08x.%08x.%08x.%08x",
    device_id[0], device_id[1], device_id[2], device_id[3]);

  err = sdhc_cmd3(rca, SD_HOST_TIMEOUT_USEC_DEFAULT, blocking);
  BCM_SDHOST_CHECK_ERR("failed at CMD3 (SEND_RELATIVE_ADDR) step");
  printf("RCA: %08x\r\n", *rca);
  /* card_state = STANDBY */

  err = sdhc_cmd13(*rca, &sdcard_state, SD_HOST_TIMEOUT_USEC_DEFAULT,
    blocking);
  BCM_SDHOST_CHECK_ERR("failed at CMD13 (SEND_STATUS) step");

  card_state = (sdcard_state >> 9) & 0xf;
  if (card_state != SD_CARD_STATE_STANDBY) {
    BCM_SDHOST_LOG_ERR("Card not in STANDBY state after init");
    return ERR_GENERIC;
  }

  bcm_sdhost_dump_fsm_state();
  return SUCCESS;
}

static void bcm_sdhost_set_bus_width4(void)
{
  uint32_t r;
  ioreg32_read(SDHOST_HCFG);
  r |= SDHOST_CFG_WIDE_EXT_BUS;
  ioreg32_write(SDHOST_HCFG, r);
  sdhost.bus_width_4 = true;
  printf("sdhost: set_bus_width4 done");
}

static void bcm_sdhost_get_max_clock(void)
{
  bool clk_enabled;
  int parent_clock_id;
  int clock_div;
  const char *parent_clock_name;

  if (!bcm_cm_get_clock_info(BCM2835_CLOCK_VPU, &clk_enabled,
    &clock_div, &parent_clock_id, &parent_clock_name)) {
    printf("Failed to get clock source\r\n");
    return;
  }

  printf("VCPU clock: ena:%d, div:%08x, src:%d(%s)\r\n",
    clk_enabled, clock_div, parent_clock_id, parent_clock_name);
}

static int bcm_sdhost_set_high_speed(bool blocking)
{
  int err;
  struct sd_cmd6_response_raw cmd6_resp = { 0 };

  printf("sdhost: set_high_speed start\r\n");

  err = sdhc_cmd6(CMD6_MODE_CHECK,
    CMD6_ARG_ACCESS_MODE_SDR25,
    CMD6_ARG_CMD_SYSTEM_DEFAULT,
    CMD6_ARG_DRIVER_STRENGTH_DEFAULT,
    CMD6_ARG_POWER_LIMIT_DEFAULT,
    cmd6_resp.data,
    SD_HOST_TIMEOUT_USEC_DEFAULT, blocking);
  BCM_SDHOST_CHECK_ERR("CMD6 query");

  BCM_SDHOST_LOG_INFO("CMD6 result: max_current:%d, fns(%04x,%04x,%04x,%04x)",
    sd_cmd6_mode_0_resp_get_max_current(&cmd6_resp),
    sd_cmd6_mode_0_resp_get_supp_fns_1(&cmd6_resp),
    sd_cmd6_mode_0_resp_get_supp_fns_2(&cmd6_resp),
    sd_cmd6_mode_0_resp_get_supp_fns_3(&cmd6_resp),
    sd_cmd6_mode_0_resp_get_supp_fns_4(&cmd6_resp));

  if (!sd_cmd6_mode_0_resp_is_sdr25_supported(&cmd6_resp)) {
    BCM_SDHOST_LOG_INFO("Switching to SDR25 not supported, skipping...");
    return SUCCESS;
  }

  BCM_SDHOST_LOG_INFO("Switching to SDR25 ...\r\n");

  err = sdhc_cmd6(CMD6_MODE_SWITCH,
    CMD6_ARG_ACCESS_MODE_SDR25,
    CMD6_ARG_CMD_SYSTEM_DEFAULT,
    CMD6_ARG_DRIVER_STRENGTH_DEFAULT,
    CMD6_ARG_POWER_LIMIT_DEFAULT,
    cmd6_resp.data,
    SD_HOST_TIMEOUT_USEC_DEFAULT, blocking);
  BCM_SDHOST_CHECK_ERR("CMD6 set");

  printf("sdhost: set_high_speed done\r\n");
}

static int csd_read_bits(const uint8_t *csd, int pos, int width)
{
  return BITS_EXTRACT8(csd[pos / 8], (pos % 8), width);
}

static void sdhc_parse_csd(const uint8_t *csd)
{
  int csd_ver;
  int taac;
  int nsac;
  int tran_speed;
  int ccc;
  int read_bl_len;
  int read_bl_partial;
  int wr_block_misalignment;
  int rd_block_misalignment;
  int dsr;
  int c_size;
  int c_size_mult;
  int erase_one_block_ena;
  int erase_sector_size;
  int write_speed_factor;
  int max_wr_block_len;

  printf("CSD:");
  for (int i = 0; i < 4; ++i)
    printf("%08x", csd[i]);
  printf("\r\n");

  csd_ver = csd_read_bits(csd, 126, 2);
  taac = csd_read_bits(csd, 112, 8);
  nsac = csd_read_bits(csd, 104, 8);
  tran_speed = csd_read_bits(csd, 96, 8);
  ccc = csd_read_bits(csd, 84, 12);
  read_bl_len = csd_read_bits(csd, 80, 4);
  read_bl_partial = csd_read_bits(csd, 79, 1);
  wr_block_misalignment = csd_read_bits(csd, 78, 1);
  rd_block_misalignment = csd_read_bits(csd, 77, 1);
  dsr = csd_read_bits(csd, 76, 1);
  c_size = csd_read_bits(csd, 62, 12);
  c_size_mult = csd_read_bits(csd, 47, 3);
  erase_one_block_ena = csd_read_bits(csd, 46, 1);
  erase_sector_size = csd_read_bits(csd, 39, 1);
  write_speed_factor  = csd_read_bits(csd, 26, 3);
  max_wr_block_len   = csd_read_bits(csd, 22, 4);

  printf("CSD ver: %d\r\n", csd_ver);
  printf("CSD TAAC(read access time): %d / NSAC:%d clk\r\n", taac, nsac);
  printf("CSD TRAN_SPEED:%d, CCC:0x%03x\r\n", tran_speed, ccc);
  printf("CSD READ_BL_LEN:%d, READ_BL_PARTIAL:%d\r\n", read_bl_len,
    read_bl_partial);
  printf("CSD wr block misalignment:%d,rd block misalignment:%d\r\n",
    wr_block_misalignment, rd_block_misalignment);
  printf("CSD DSR:%d, C_SIZE:%d, C_SIZE_MULT:%d\r\n", dsr, c_size,
    c_size_mult);
  printf("CSD erase_one_block_ena: %d, erase sector size:%d\r\n",
    erase_one_block_ena, erase_sector_size);
  printf("CSD write speed factor: %d\r\n", write_speed_factor);
  printf("CSD max write block len: %d\r\n", max_wr_block_len);
}

uint64_t reverse_bytes(uint64_t input) {
    uint64_t output;
    asm volatile (
        "rev %0, %1\n\t"  // Reverse byte order of input into output
        : "=r" (output)   // Output operand (output)
        : "r" (input)     // Input operand
    );
    return output;
}

static int bcm_sdhost_to_data_transfer_mode(uint32_t rca, bool blocking)
{
  uint64_t scr;
  uint32_t csd[4];
  int err;

  /*
   * Guide card through data transfer mode states into 'tran' state
   * CMD9 SEND_CSD to get and parse CSD registers
   */
  err = sdhc_cmd9(rca, csd, SD_HOST_TIMEOUT_USEC_DEFAULT,
    blocking);
  BCM_SDHOST_CHECK_ERR("Failed at CMD9 (SEND_CSD)");

  sdhc_parse_csd((uint8_t *)csd);
  /* CMD7 SELECT_CARD to select card, making it to 'tran' statew */
  err = sdhc_cmd7(rca, SD_HOST_TIMEOUT_USEC_DEFAULT, blocking);
  BCM_SDHOST_CHECK_ERR("Failed at CMD7 (SELECT_CARD)");

  /* ACMD51 SEND_SCR */
  use_dma = false;
  err = sdhc_acmd51(rca, &scr, sizeof(scr), SD_HOST_TIMEOUT_USEC_DEFAULT,
    blocking);
  BCM_SDHOST_CHECK_ERR("Failed at ACMD51 (SEND_SCR)");
  ioreg32_write(SDHOST_CDIV, 5);

  uint32_t hcfg = ioreg32_read(SDHOST_HCFG);
  hcfg &= ~SDHOST_CFG_SLOW_CARD;
  ioreg32_write(SDHOST_HCFG, hcfg);
  sdhost.bus_width_4 = true;

  scr = reverse_bytes(scr);

  bool buswidth4_supported = sd_scr_bus_width4_supported(scr);
  int sd_spec_ver_maj;
  int sd_spec_ver_min;

  sd_scr_get_sd_spec_version(scr, &sd_spec_ver_maj, &sd_spec_ver_min);

  printf("SCR %016llx: sd spec version: %d.%d\r\n"
    "bus_width1: %s, buswidth4:%s\r\n",
    scr,
    sd_spec_ver_maj,
    sd_spec_ver_min,
    sd_scr_bus_width1_supported(scr) ? "supported" : "not_supp",
    buswidth4_supported ? "supported" : "not_supp"
  );

  if (buswidth4_supported) {
    err = sdhc_acmd6(rca, SDHC_ACMD6_ARG_BUS_WIDTH_4,
      SD_HOST_TIMEOUT_USEC_DEFAULT, blocking);
    BCM_SDHOST_CHECK_ERR("Failed at ACMD6 (SET_BUS_WIDTH)");
    delay_us(1 * 1000 * 1000);
    bcm_sdhost_set_bus_width4();
    printf("TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT\r\n");
    delay_us(1 * 1000 * 1000);
    printf("TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT\r\n");
  }

  uint32_t sdcard_state;
  uint32_t card_state;
  err = sdhc_cmd13(rca, &sdcard_state, SD_HOST_TIMEOUT_USEC_DEFAULT,
    blocking);
  BCM_SDHOST_CHECK_ERR("failed at CMD13 (SEND_STATUS) step");

  card_state = (sdcard_state >> 9) & 0xf;
  printf("sdcard_state:%08x, card_state:%d\r\n", sdcard_state, card_state);

  /* ACMD51 SEND_SCR */
  use_dma = false;
  err = sdhc_acmd51(rca, &scr, sizeof(scr), SD_HOST_TIMEOUT_USEC_DEFAULT,
    blocking);
  BCM_SDHOST_CHECK_ERR("Failed at ACMD51 (SEND_SCR)");
  printf("scr after 4bit bus: %016llx\r\n", reverse_bytes(scr));

  bcm_sdhost_set_high_speed(blocking);
  bcm_sdhost_dump_fsm_state();
  return 0;
}

static int bcm_sdhost_read(uint8_t *buf, uint32_t from_sector, uint32_t num_sectors,
  bool blocking)
{
  return sdhc_cmd17(from_sector, buf, SD_HOST_TIMEOUT_USEC_DEFAULT, blocking);
}

static int bcm_sdhost_reset(bool blocking, uint32_t *rca, uint32_t *device_id)
{
  int err;
  uint8_t readbuf[512];

  bcm_sdhost_get_max_clock();
  bcm_sdhost_reset_registers();
  err = sdhc_identification(rca, blocking);
  if (err != SUCCESS)
    return err;

  err = bcm_sdhost_to_data_transfer_mode(*rca, blocking);
  if (err != SUCCESS)
    return err;

  bcm_sdhost_read(readbuf, 0, 1, blocking);
  printf("bcm_sdhost_reset done");
  while(1);
  return SUCCESS;
}

static void bcm_sdhost_init_gpio(void)
{
  int i;
  for (i = 48; i < 48 + 6; i++) {
    gpio_set_pin_function(i, GPIO_FUNCTION_ALT_0);
    gpio_set_pin_pullupdown_mode(i, GPIO_PULLUPDOWN_MODE_UP);
  }
}

int bcm_sdhost_init(void)
{
  int err;

  sdhost.bus_width_4 = false;
  err = sdhc_io_init(&sdhost.io, sdhost_on_dma_done);
  if (err) {
    printf("sdhc_io_init failed, err:%d\r\n", err);
    return err;
  }
  bcm_sdhost_log_level = LOG_LEVEL_DEBUG2;
  // char buf[512];

#if 0
  if (sdhost.initialized)
    return SUCCESS;

  bcm2835_emmc_log_level = LOG_LEVEL_DEBUG2;
  bcm2835_emmc.is_blocking_mode = true;
  bcm2835_emmc.num_inhibit_waits = 0;
  err = bcm2835_emmc_io_init(&bcm2835_emmc.io, bcm2835_emmc_dma_irq_callback);
  if (err != SUCCESS) {
    BCM2835_EMMC_ERR("bcm2835_emmc_init failed at io: %d", err);
    return err;
  }
#endif

  bcm_sdhost_init_gpio();
  sdhost.blocking_mode = true;
  err = bcm_sdhost_reset(sdhost.blocking_mode, &sdhost.rca,
    sdhost.device_id);

  if (err != SUCCESS) {
    printf("sd host reset failed: %d", err);
    return err;
  }

  sdhost.initialized = true;

#if 0
  err = _emmc_data_io(BCM2835_EMMC_IO_READ, buf, 0, 1);
  if (err) {
    printf("Failed to read block %d\n", err);
  }
  else {
    for (int i = 0; i < 512; ++i) {
      printf("block:%02x\n", buf[i]);
    }
  }
#endif
  return SUCCESS;
}
