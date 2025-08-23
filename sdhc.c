#include <errcode.h>
#include <sdhc.h>
#include <sdhc_cmd.h>
#include <bcm2835_dma.h>
#include <bitops.h>
#include <os_api.h>
#include <log.h>
#include <list_fifo.h>

typedef enum {
  SDHC_OP_READ = 0,
  SDHC_OP_WRITE = 1
} sdhc_op_t;

static int sdhc_log_level;
static struct sdhc *sdhc_current;
extern struct sdhc_cmd_stat bcm_sdhost_cmd_stats;

#define __SDHC_LOG(__level, __fmt, ...) \
  LOG(sdhc_log_level, __level, "sdhc", __fmt, ##__VA_ARGS__)

#define SDHC_LOG_INFO(__fmt, ...) \
  __SDHC_LOG(INFO, __fmt, ## __VA_ARGS__)

#define SDHC_LOG_DBG(__fmt, ...) \
  __SDHC_LOG(DEBUG, __fmt, ## __VA_ARGS__)

#define SDHC_LOG_DBG2(__fmt, ...) \
  __SDHC_LOG(DEBUG2, __fmt, ## __VA_ARGS__)

#define SDHC_LOG_WARN(__fmt, ...) \
  __SDHC_LOG(WARN, __fmt, ## __VA_ARGS__)

#define SDHC_LOG_CRIT(__fmt, ...) \
  __SDHC_LOG(CRIT, __fmt, ## __VA_ARGS__)

#define SDHC_LOG_ERR(__fmt, ...) \
  __SDHC_LOG(ERR, __fmt, ## __VA_ARGS__)

#define SDHC_CHECK_ERR(__fmt, ...)\
  do {\
    if (err != SUCCESS) {\
      SDHC_LOG_ERR("%s err %d, " __fmt, __func__,  err, ## __VA_ARGS__);\
      return err;\
    }\
  } while(0)

#define SDHC_TIMEOUT_DEFAULT_USEC 1000000

static inline const char *sdhc_op_to_str(sdhc_op_t op)
{
  switch (op) {
    case SDHC_OP_READ: return "READ";
    case SDHC_OP_WRITE: return "WRITE";
    default: break;
  }
  return "UNKNOWN";
}

static void sdhc_dma_irq(void)
{
  sdhc_current->ops->notify_dma_isr(sdhc_current);
}

/*
 * - We to put SD card to SD mode (CMD0 with CS not asserted)
 * - We need to make sure SD card runs in UHS-I 
 *   UHS-I stands for Ultra High Speed Phase-I, it has 
 *     DS - Default Speed up to 25MHz 3.3V signaling
 *     HS - High Speed up to 50MHz 3.3V signaling
 * - On reset after CMD0 SD card operates at 400KHz, we have a clock source
 *   of 250MHz, 250 000 000  / 400 000 = 625, in theory we should run with this
 *   clock, in theory we should run with this.
 */

/* 
 * sdhc_process_acmd41 implements the following paragraphs, related to ACMD41
 * Physical Layer Simplified Specification Version 9.00:
 * 4.2.2 Operating Condition Validation
 * "By setting the OCR to zero in the argument of ACMD41, the host can query
 * each card and determine the common voltage range before sending
 * out-of-range cards into the Inactive State (query mode). This query should
 * be used if the host is able to select a common voltage range or if a
 * notification to the application of non-usable cards in the stack is desired.
 * The card does not start initialization and ignores HCS in the argument
 * (refer to Section 4.2.3) if ACMD41 is issued as a query. Afterwards, the
 * host may choose a voltage for operation and reissue ACMD41 with this
 * condition, sending incompatible cards into the Inactive State.
 * During the initialization procedure, the host is not allowed to change the
 * operating voltage range. Refer to power up sequence as described in
 * Section 6.4"
 * 
 * 4.2.3 Card Initialization and Identification Process
 * 4.2.3.1 Initialization Command (ACMD41)
 */
static int sdhc_process_acmd41(struct sdhc *s)
{
  int err;
  uint32_t acmd41_resp;
  uint32_t acmd41_arg;

#define SDHC_ACMD41_ARG_INQUERY 0

#define SDHC_ACMD41_ARG_HCS 30
#define SDHC_ACMD41_ARG_XPC 28

#define SDHC_ACMD41_RESP_BUSY   31
#define SDHC_ACMD41_RESP_CCS    30
#define SDHC_ACMD41_RESP_UHS_II 29
#define SDHC_ACMD41_RESP_S18_A  24
/*
 * In inquery mode ACMD41 (arg=0) SD card returns supported voltage ranges
 * in response in OCR bits [23:15].
 * OCR reg:
 *  __________________________________________________________________________
 * |    |   |     | |    | |    |   |   |   |   |   |   |   |   |   |         |
 * |BUSY|CCS|UHSII|-|CO2T|-|S18A|V35|V34|V33|V32|V31|V30|V29|V28|V27|Reserved |
 * |____|___|_____|_|____|_|____|___|___|___|___|___|___|___|___|___|_________|
 * | 31 |30 | 29  | | 27 | | 24 |23 |22 |21 |20 |19 |18 |17 |16 |15 |  14-00  |
 * |____|___|_____|_|____|_|____|___|___|___|___|___|___|___|___|___|_________|
 *
 *, where V35, V34, etc are ranges: V35=v3.5-3.6, V34=v3.4-v3.5 and so on
 * In ACMD41 bits [23:8] of OCR are encoded in argument:
 *  _____________________________________________
 * |    |   |  |   |        |    |     |        |
 * |BUSY|HCS|FB|XPC|Reserved|S18R| OCR |Reserved|
 * |____|___|__|___|________|____|_____|________|
 * | 31 |30 |29|28 |  27-25 | 24 |23-08|  07-00 |
 * |____|___|__|___|________|____|_____|________|
 *
 * Voltage window mask thus is 9bits of data shifted by 15 bits:
 * Raspberry PI only supports 3.3v signal so, we need to limit the mask even
 * more by only bits 21 and 20
 */
#define SDHC_ACMD41_ARG_VOLTAGE_MASK 0x00ff8000 & (BIT(21) | BIT(20)
#define SDHC_ACMD41_ARG_VOLTAGE_MASK_RPI3 (BIT(21) | BIT(20))

  /*
   * If the voltage window field (bit 23-0) in the argument is set to zero, it
   * is called "inquiry CMD41" that does not start initialization and is use
   * for getting OCR. The inquiry ACMD41 shall ignore the other field
   * (bit 31-24) in the argument.
   */
  err = sdhc_acmd41(s, SDHC_ACMD41_ARG_INQUERY, &acmd41_resp,
    SDHC_TIMEOUT_DEFAULT_USEC);

  SDHC_CHECK_ERR("ACMD41 (GET_OCR) failed");
  SDHC_LOG_DBG("ACMD41 response:%08x", acmd41_resp);

  /* Form ACMD41 arg with valid voltage range
   */
  acmd41_arg = acmd41_resp & SDHC_ACMD41_ARG_VOLTAGE_MASK_RPI3;
  if (s->cmd8_response_received)
    acmd41_arg |= BIT(SDHC_ACMD41_ARG_HCS);

  while (1) {
    err = sdhc_acmd41(s, acmd41_arg, &acmd41_resp, SDHC_TIMEOUT_DEFAULT_USEC);
    SDHC_CHECK_ERR("ACMD41 (GET_OCR) repeated failed");
    SDHC_LOG_DBG("ACMD41: arg:%08x, resp:%08x", acmd41_arg, acmd41_resp);

    /* BUSY bit == 0 : card is BUSY / still initializing
     * BUSY bit == 1 : card is READY / initialized
     */
    if (acmd41_resp & BIT(SDHC_ACMD41_RESP_BUSY))
      break;
  }

  /*
   * If the card responds to CMD8, the response of ACMD41 includes the CCS
   * field information. CCS is valid when the card returns ready (the busy bit
   * is set to 1). CCS=0 means that the card is SDSC. CCS=1 means that the
   * card is SDHC or SDXC
   */
  if (BIT_IS_SET(acmd41_resp, SDHC_ACMD41_RESP_CCS))
    s->card_capacity = SD_CARD_CAPACITY_SDHC;
  if (BIT_IS_SET(acmd41_resp, SDHC_ACMD41_RESP_UHS_II))
    s->UHS_II_support = true;

  SDHC_LOG_INFO("Card capacity: %s%s",
    sd_card_capacity_to_str(s->card_capacity),
    s->UHS_II_support ? ", UHS-II supported" : "");

  return SUCCESS;
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
static int sdhc_to_identification_mode(struct sdhc *s)
{
  int err;
  sd_card_state_t card_state;
  uint32_t sdcard_state;

  /*
   * TODO: set controller clock to 400KHz, at power on this is the cards
   * operation speed until end of identitication state
   */
  err = sdhc_cmd0(s, SDHC_TIMEOUT_DEFAULT_USEC);
  SDHC_CHECK_ERR("Failed at CMD0 (GO_TO_IDLE)");

  /* card_state = IDLE */

  /* CMD8 -SEND_IF_COND - this checks if SD card supports our voltage */
  err = sdhc_cmd8(s, SDHC_TIMEOUT_DEFAULT_USEC);
  SDHC_CHECK_ERR("CMD8 (SEND_IF_COND) failed");
  /* card_state = IDLE */
  s->cmd8_response_received = true;
  err = sdhc_process_acmd41(s);
  SDHC_CHECK_ERR("failed to run ACMD41");

  /* card_state = READY */

  /* Get device id */
  err = sdhc_cmd2(s, s->device_id, SDHC_TIMEOUT_DEFAULT_USEC);
  SDHC_CHECK_ERR("failed at CMD2 (SEND_ALL_CID) step");
  /* card_state = IDENT */

  SDHC_LOG_INFO("device_id: %08x|%08x|%08x|%08x",
    s->device_id[0], s->device_id[1], s->device_id[2], s->device_id[3]);

  err = sdhc_cmd3(s, &s->rca, SDHC_TIMEOUT_DEFAULT_USEC);
  SDHC_CHECK_ERR("failed at CMD3 (SEND_RELATIVE_ADDR) step");
  SDHC_LOG_INFO("SD card RCA:%08x", s->rca);

  /* card_state = STANDBY */
  err = sdhc_cmd13(s,&sdcard_state, SDHC_TIMEOUT_DEFAULT_USEC);
  SDHC_CHECK_ERR("failed at CMD13 (SEND_STATUS) step");

  card_state = (sdcard_state >> 9) & 0xf;
  if (card_state != SD_CARD_STATE_STANDBY) {
    SDHC_LOG_ERR("Card not in STANDBY state after init");
    return ERR_GENERIC;
  }

  if (s->ops->dump_fsm_state)
    s->ops->dump_fsm_state();
  return SUCCESS;
}

static int sdhc_read_scr_and_parse(struct sdhc *s)
{
  int err;
  int sd_spec_ver_maj;
  int sd_spec_ver_min;

  /* ACMD51 SEND_SCR */
  err = sdhc_acmd51(s, &s->scr, sizeof(s->scr), SDHC_TIMEOUT_DEFAULT_USEC);
  SDHC_CHECK_ERR("Failed at ACMD51 (SEND_SCR)");

  s->bus_width_4_supported = sd_scr_bus_width4_supported(s->scr);

  sd_scr_get_sd_spec_version(s->scr, &sd_spec_ver_maj, &sd_spec_ver_min);

  SDHC_LOG_INFO("SCR %016llx: Spec vesionr:%d.%d, "
    "bus_widths1:%s,bus_width4:%s",
    s->scr,
    sd_spec_ver_maj,
    sd_spec_ver_min,
    sd_scr_bus_width1_supported(s->scr) ? "yes" : "no",
    s->bus_width_4_supported ? "yes" : "no"
  );

  return SUCCESS;
}

/*
 * Runs CMD13 to card state.
 * Check 4.8 Card State Transition Table in
 * Physical Layer Simplified Specification Version 9.00
 * If CMD13 completes successfully, returns sd_card_state_t ret > 0
 * If CMD13 fails, returns error value < 0
 */
static int sdhc_get_card_state(struct sdhc *s)
{
  int err;

  /* card_state = STANDBY */
  uint32_t card_status;
  err = sdhc_cmd13(s, &card_status, SDHC_TIMEOUT_DEFAULT_USEC);
  SDHC_CHECK_ERR("failed at CMD13 (SEND_STATUS) step");
  return (int)((card_status >> 9) & 0xf);
}

static inline int sdhc_dump_card_state(struct sdhc *s, const char *tag,
  int log_level)
{
  int card_state;

  if (sdhc_log_level < log_level)
    return SUCCESS;

  card_state = sdhc_get_card_state(s);
  if (card_state < 0)
    return card_state;

  SDHC_LOG_INFO("[%s] SD card state:%d %s", tag, card_state,
    sd_card_state_to_str(card_state));

  return SUCCESS;
}

static int sdhc_set_high_speed(struct sdhc *s)
{
  int err;
  struct sd_cmd6_response_raw cmd6_resp = { 0 };

  SDHC_LOG_DBG("Switching to HIGH SPEED mode");

  os_wait_ms(100);
  err = sdhc_cmd6(s, CMD6_MODE_CHECK,
    CMD6_ARG_ACCESS_MODE_SDR25,
    CMD6_ARG_CMD_SYSTEM_DEFAULT,
    CMD6_ARG_DRIVER_STRENGTH_DEFAULT,
    CMD6_ARG_POWER_LIMIT_DEFAULT,
    cmd6_resp.data,
    SDHC_TIMEOUT_DEFAULT_USEC);
  SDHC_CHECK_ERR("CMD6 query");

  SDHC_LOG_INFO("CMD6 result: max_current:%d, fns(%04x,%04x,%04x,%04x)",
    sd_cmd6_mode_0_resp_get_max_current(&cmd6_resp),
    sd_cmd6_mode_0_resp_get_supp_fns_1(&cmd6_resp),
    sd_cmd6_mode_0_resp_get_supp_fns_2(&cmd6_resp),
    sd_cmd6_mode_0_resp_get_supp_fns_3(&cmd6_resp),
    sd_cmd6_mode_0_resp_get_supp_fns_4(&cmd6_resp));

  if (!sd_cmd6_mode_0_resp_is_sdr25_supported(&cmd6_resp)) {
    SDHC_LOG_INFO("Switching to SDR25 not supported, skipping...");
    return SUCCESS;
  }

  SDHC_LOG_INFO("Switching to SDR25 ...");

  err = sdhc_cmd6(s, CMD6_MODE_SWITCH,
    CMD6_ARG_ACCESS_MODE_SDR25,
    CMD6_ARG_CMD_SYSTEM_DEFAULT,
    CMD6_ARG_DRIVER_STRENGTH_DEFAULT,
    CMD6_ARG_POWER_LIMIT_DEFAULT,
    cmd6_resp.data,
    SDHC_TIMEOUT_DEFAULT_USEC);
  SDHC_CHECK_ERR("CMD6 set");

  SDHC_LOG_DBG("Switched to HIGH SPEED mode");
  return SUCCESS;
}

static int csd_read_bits(const uint8_t *csd, int pos, int width)
{
  return BITS_EXTRACT8(csd[pos / 8], (pos % 8), width);
}

static void sdhc_parse_csd(struct sdhc *s, const uint8_t *csd)
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
  char csdbuf[16 * 2 + 1];

  int n = 0;
  for (int i = 0; i < 16; ++i)
    n += snprintf(csdbuf + n, sizeof(csdbuf) - n, "%02x", csd[i]);

  SDHC_LOG_INFO("CSD: %s", csdbuf);

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

  SDHC_LOG_INFO("CSD ver: %d", csd_ver);
  SDHC_LOG_INFO("CSD TAAC(read access time): %d / NSAC:%d clk", taac,
    nsac);
  SDHC_LOG_INFO("CSD TRAN_SPEED:%d, CCC:0x%03x", tran_speed, ccc);
  SDHC_LOG_INFO("CSD READ_BL_LEN:%d, READ_BL_PARTIAL:%d", read_bl_len,
    read_bl_partial);
  SDHC_LOG_INFO("CSD wr block misalignment:%d,rd block misalignment:%d",
    wr_block_misalignment, rd_block_misalignment);
  SDHC_LOG_INFO("CSD DSR:%d, C_SIZE:%d, C_SIZE_MULT:%d", dsr, c_size,
    c_size_mult);
  SDHC_LOG_INFO("CSD erase_one_block_ena: %d, erase sector size:%d",
    erase_one_block_ena, erase_sector_size);
  SDHC_LOG_INFO("CSD write speed factor: %d", write_speed_factor);
  SDHC_LOG_INFO("CSD max write block len: %d", max_wr_block_len);

  s->block_size = 1 << read_bl_len;
}

static int sdhc_to_data_transfer_mode(struct sdhc *s)
{
  uint8_t sd_status[64];
  uint64_t test_scr;
  int err;

  /*
   * Guide card through data transfer mode states into 'tran' state
   * CMD9 SEND_CSD to get and parse CSD registers
   */
  err = sdhc_cmd9(s, s->csd, SDHC_TIMEOUT_DEFAULT_USEC);
  SDHC_CHECK_ERR("Failed at CMD9 (SEND_CSD)");

  sdhc_parse_csd(s, (uint8_t *)s->csd);

  /* CMD7 SELECT_CARD to select card, making it to 'tran' statew */
  err = sdhc_cmd7(s, SDHC_TIMEOUT_DEFAULT_USEC);
  SDHC_CHECK_ERR("Failed at CMD7 (SELECT_CARD)");

  sdhc_read_scr_and_parse(s);
  SDHC_CHECK_ERR("Failed to read / parse SCR");

  err = sdhc_acmd13(s, sd_status, SDHC_TIMEOUT_DEFAULT_USEC);
  SDHC_CHECK_ERR("Failed ACMD13");

  err = sdhc_set_high_speed(s);
  SDHC_CHECK_ERR("Failed to set UHS-I \"High Speed\" 50MHz 3.3v mode");
  s->ops->set_high_speed_clock();

  if (s->bus_width_4_supported) {
    err = sdhc_acmd6(s, SDHC_ACMD6_ARG_BUS_WIDTH_4,
      SDHC_TIMEOUT_DEFAULT_USEC);
    SDHC_CHECK_ERR("Failed at ACMD6 (SET_BUS_WIDTH)");
    s->ops->set_bus_width4();
    s->bus_width_4 = true;
  }

  err = sdhc_dump_card_state(s, "high-speed and bus_width configured",
    LOG_LEVEL_INFO);

  SDHC_CHECK_ERR("Failed to read card state");

  err = sdhc_acmd51(s, &test_scr, sizeof(test_scr),
    SDHC_TIMEOUT_DEFAULT_USEC);
  SDHC_CHECK_ERR("Failed at ACMD51 (SEND_SCR)");

  if (test_scr != s->scr) {
    SDHC_LOG_DBG("SCR mismatch after switch to 4bit bus: %016llx",
      test_scr);
    return ERR_IO;
  }

  SDHC_LOG_INFO("SD card now runs in UHS-I 3.3V \"High Speed\" mode");
  SDHC_LOG_INFO("Bus with 4 DAT3-0 lines, 50MHz -> 25Mbytes/sec");

  err = sdhc_cmd16(s, 512, SDHC_TIMEOUT_DEFAULT_USEC);

  if (s->ops->dump_fsm_state)
    s->ops->dump_fsm_state();

  return SUCCESS;
}

static int sdhc_io_init(struct sdhc_io *io, void (*on_dma_done)(void))
{
  io->dma_channel = bcm2835_dma_request_channel();
  io->dma_control_block_idx = bcm2835_dma_reserve_cb();
  printf("sdhc_io_init: DMA chan:%d, cb:%d\r\n", io->dma_channel, io->dma_control_block_idx);

  if (io->dma_channel == -1 || io->dma_control_block_idx == -1)
    return ERR_GENERIC;

  bcm2835_dma_set_irq_callback(io->dma_channel, on_dma_done);

  bcm2835_dma_irq_enable(io->dma_channel);
  bcm2835_dma_enable(io->dma_channel);
  bcm2835_dma_reset(io->dma_channel);

  return SUCCESS;
}

static OPTIMIZED int sdhc_op(struct sdhc *s, sdhc_op_t op, uint8_t *buf,
  size_t start_block_idx, size_t num_blocks)
{
  int err = ERR_INVAL;
//  sd_card_state_t card_state;

#if 0
wait:
  err = sdhc_get_card_state(s);
  if (err < 0)
    return err;

  card_state = (sd_card_state_t)err;
  SDHC_LOG_INFO("card state: %d(%s)", card_state,
    sd_card_state_to_str(card_state));

  if (card_state == SD_CARD_STATE_PROG)
    goto wait;

  err = sdhc_dump_card_state(s, "before op", LOG_LEVEL_DEBUG2);
  if (err)
    return err;
#endif
  s->ops->wait_prev_done(s);
  sdhc_current = s;
  /*
   * CMD18 READ_MULTIPLE_BLOCKS or CMD25 WRITE_MULTIPLE_BLOCKS must follow
   * CMD23 SET_BLOCK_COUNT in strict sequence, other CMDs not allowed inside
   */
  if (num_blocks > 1) {
    bcm_sdhost_cmd_stats.multiblock_start_time = arm_timer_get_count();
    err = sdhc_cmd23(s, num_blocks, SDHC_TIMEOUT_DEFAULT_USEC);
    bcm_sdhost_cmd_stats.multiblock_end_time = arm_timer_get_count();
    SDHC_CHECK_ERR("SET_BLOCK_COUNT failed");
  }

  if (op == SDHC_OP_READ) {
    /* READ_SINGLE_BLOCK or READ_MULTIPLE_BLOCKS */
    if (num_blocks == 1)
      err = sdhc_cmd17(s, start_block_idx, buf, SDHC_TIMEOUT_DEFAULT_USEC);
    else
      err = sdhc_cmd18(s, start_block_idx, num_blocks, buf,
        SDHC_TIMEOUT_DEFAULT_USEC);
  } else if (op == SDHC_OP_WRITE) {
    /* WRITE_SINGLE_BLOCK or WRITE_MULTIPLE_BLOCKS */
    if (num_blocks == 1)
      err = sdhc_cmd24(s, start_block_idx, buf, SDHC_TIMEOUT_DEFAULT_USEC);
    else
      err = sdhc_cmd25(s, start_block_idx, num_blocks, buf,
        SDHC_TIMEOUT_DEFAULT_USEC);
  }

  if (num_blocks > 1) {
    // err = sdhc_cmd12(s, SDHC_TIMEOUT_DEFAULT_USEC);
    // SDHC_CHECK_ERR("SET_BLOCK_COUNT failed");
  }

#if 0
  if (err == SUCCESS) {
    err = sdhc_dump_card_state(s, "after op", LOG_LEVEL_DEBUG2);
    if (err)
      return err;
  }
#endif

  if (err)
    SDHC_LOG_ERR("sd op %s failed", sdhc_op_to_str(op));

  return err;
}

int sdhc_read(struct sdhc *s, uint8_t *buf, uint64_t start_block_idx,
  uint32_t num_blocks)
{
  return sdhc_op(s, SDHC_OP_READ, buf, start_block_idx, num_blocks);
}

int sdhc_write(struct sdhc *s, const uint8_t *buf, uint64_t start_block_idx,
  uint32_t num_blocks)
{
  return sdhc_op(s, SDHC_OP_WRITE, (uint8_t *)buf, start_block_idx,
    num_blocks);
}

int sdhc_blockdev_read(struct block_device *blockdev, uint8_t *buf,
  uint64_t start_block_idx, uint32_t num_blocks)
{
  struct sdhc *s = blockdev->priv;
  return sdhc_op(s, SDHC_OP_READ, buf, start_block_idx, num_blocks);
}

int OPTIMIZED sdhc_blockdev_write(struct block_device *blockdev, const uint8_t *buf,
  uint64_t start_block_idx, uint32_t num_blocks)
{
  struct sdhc *s = blockdev->priv;
  return sdhc_op(s, SDHC_OP_WRITE, (uint8_t *)buf, start_block_idx,
    num_blocks);
}

int sdhc_blockdev_erase(struct block_device *blockdev,
  uint64_t start_block_idx, uint32_t num_blocks)
{
  return ERR_NOTSUPP;
}

int sdhc_write_stream_open(struct block_device *blockdev,
  uint64_t start_block_idx)
{
  struct sdhc *s = blockdev->priv;
  s->write_stream_next_block_idx = start_block_idx;
  s->write_stream_opened = true;
  return SUCCESS;
}

static void sdhc_write_stream_release_after_wr(struct sdhc *s,
  uint32_t num_blocks_written)
{
  struct write_stream_buf *b;
  struct write_stream_buf *tmp;

  list_for_each_entry_safe(b, tmp, &s->write_stream_pending, list) {
    uint32_t remaining_size = b->size - b->io_offset;
    bcm2835_dma_release_cb(b->dma_cb);
    s->write_stream_pending_size -= b->io_size;
    if (s->write_stream_num_pending_bufs == 1 && remaining_size) {
      if (s->write_stream_pending_size) {
        os_log("Value of write_stream_pending_size not as expected %d vs %d\r\n",
          s->write_stream_pending_size, b->size - b->io_offset);
        while(1)
          asm volatile("wfe");
      }
      if (s->write_stream_num_pending_bufs != 1) {
        os_log("Value of write_stream_num_pending_bufs not as expected 1 vs %d\r\n",
          s->write_stream_num_pending_bufs);
        while(1)
          asm volatile("wfe");
      }

      b->io_size = remaining_size;
      s->write_stream_pending_size = remaining_size;
      break;
    }

    s->write_stream_num_pending_bufs--;

    list_del(&b->list);
    list_fifo_push_tail_locked(&s->write_stream_release_list, &b->list);
  }
}

static int sdhc_write_stream_one(struct sdhc *s, struct write_stream_buf *b)
{
  int err;
  uint32_t num_wr_blocks;
  uint32_t new_pending_size;
  uint32_t out_of_bound_size;
  uint32_t buf_io_capacity;

  s->dma_stream_mode = true;

  if (s->block_size != 512) {
    os_log("FAILED block size not 512\r\n");
    while(1)
      asm volatile ("wfe");
  }

  buf_io_capacity = b->size - b->io_offset;
  new_pending_size = s->write_stream_pending_size + buf_io_capacity;
  s->write_stream_num_pending_bufs++;
  list_fifo_push_tail(&s->write_stream_pending, &b->list);

  if (new_pending_size < s->block_size) {
    b->io_size = buf_io_capacity;
    s->write_stream_pending_size = new_pending_size;
    err = SUCCESS;
    goto out;
  }

  num_wr_blocks = new_pending_size / s->block_size;
  s->write_stream_pending_size = num_wr_blocks * s->block_size;
  out_of_bound_size = new_pending_size - s->write_stream_pending_size;
  b->io_size = buf_io_capacity - out_of_bound_size;

  s->ops->wait_prev_done(s);
  sdhc_current = s;
  err = sdhc_cmd23(s, num_wr_blocks, SDHC_TIMEOUT_DEFAULT_USEC);
  SDHC_CHECK_ERR("Failed to SET_BLOCK_LEN for write stream to %d",
    num_wr_blocks);

  err = sdhc_cmd25(s, s->write_stream_next_block_idx, num_wr_blocks, NULL,
    SDHC_TIMEOUT_DEFAULT_USEC);
  SDHC_CHECK_ERR("Failed to WRITE MULTIPLE BLOCKS to write stream");
  s->write_stream_next_block_idx += num_wr_blocks;
  sdhc_write_stream_release_after_wr(s, num_wr_blocks);

out:
  return err;
}

int sdhc_write_stream_push(struct block_device *blockdev,
  struct list_head *buflist)
{
  int err;
  struct sdhc *s = blockdev->priv;
  struct write_stream_buf *b;
  struct write_stream_buf *tmp;
  struct list_head *node;
  struct list_head *tmp_node;

  if (!s->write_stream_opened) {
    SDHC_LOG_ERR("Attempted to write to non-existing stream");
    return ERR_INVAL;
  }

  list_for_each_entry_safe(b, tmp, buflist, list) {
    list_del_init(&b->list);
    err = sdhc_write_stream_one(s, b);
    SDHC_CHECK_ERR("Failed to push buffer to stream");
  }

  if (!list_empty(buflist)) {
    SDHC_LOG_ERR("write stream logic error, buflist not empty");
    while (1)
      asm volatile ("wfe");
  }

  list_for_each_safe(node, tmp_node, &s->write_stream_release_list) {
    list_del_init(node);
    list_fifo_push_tail_locked(buflist, node);
  }
  return SUCCESS;
}

int sdhc_init(struct block_device *blockdev, struct sdhc *s,
  struct sdhc_ops *ops)
{
  int err;
  INIT_LIST_HEAD(&s->write_stream_pending);
  INIT_LIST_HEAD(&s->write_stream_release_list);
  s->write_stream_opened = false;
  s->write_stream_num_pending_bufs = 0;
  s->write_stream_pending_size = 0;
  s->write_stream_next_block_idx = 0;
  s->dma_stream_mode = false;
  s->invalidate_before_write = false;

  s->bus_width_4 = false;
  s->bus_width_4_supported = false;
  s->blocking_mode = true;
  s->is_acmd_context = false;
  s->rca = 0;
  s->scr = 0;
  s->sdcard_mode = SDHC_SDCARD_MODE_SD;
  s->cmd8_response_received = false;
  s->card_capacity = SD_CARD_CAPACITY_SDSC;
  s->UHS_II_support = false;
  sdhc_log_level = LOG_LEVEL_INFO;

  s->io_mode = SDHC_IO_MODE_BLOCKING_PIO;
  s->ops = ops;

  err = sdhc_io_init(&s->io, sdhc_dma_irq);
  SDHC_CHECK_ERR("Failed to init sdhc io");

  if (s->ops->init) {
    err = s->ops->init();
    if (err)
      return err;
  }

  if (s->ops->init_gpio)
    s->ops->init_gpio();

  if (s->ops->reset_regs)
    s->ops->reset_regs();

  err = sdhc_to_identification_mode(s);
  if (err)
    return err;

  err = sdhc_to_data_transfer_mode(s);
  if (err)
    return err;

  s->initialized = true;
  blockdev->priv = s;
  blockdev->sector_size = s->block_size;
  blockdev->ops.read = sdhc_blockdev_read;
  blockdev->ops.write = sdhc_blockdev_write;
  blockdev->ops.block_erase = sdhc_blockdev_erase;
  blockdev->ops.write_stream_open = sdhc_write_stream_open;
  blockdev->ops.write_stream_push = sdhc_write_stream_push;
  SDHC_LOG_INFO("SD host controller initialized, block_size:%d",
    blockdev->sector_size);

  return SUCCESS;
}

int sdhc_set_io_mode(struct sdhc *sdhc, sdhc_io_mode_t mode,
  bool invalidate_before_write)
{
  int err;
  err = sdhc->ops->set_io_mode(sdhc, mode, invalidate_before_write);
  if (err != SUCCESS) {
    SDHC_LOG_ERR("Failed to set io_mode %d", mode);
    return err;
  }


  sdhc->io_mode = mode;
  sdhc->invalidate_before_write = invalidate_before_write;

  SDHC_LOG_INFO("io mode set to %d(%s), invalidate_wr:%d", mode,
    sdhc_io_mode_to_str(mode), invalidate_before_write);

  return SUCCESS;
}

void sdhc_set_log_level(int l)
{
  SDHC_LOG_INFO("log level set from %d to %d\r\n", sdhc_log_level, l);
  sdhc_log_level = l;
}
