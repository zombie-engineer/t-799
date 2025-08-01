#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <sdhc_io.h>

typedef enum {
  SDHC_IO_MODE_BLOCKING_PIO,
  SDHC_IO_MODE_BLOCKING_DMA,
  SDHC_IO_MODE_IT_DMA,
} sdhc_io_mode_t;

typedef enum {
  SD_CARD_STATE_IDLE    = 0,
  SD_CARD_STATE_READY   = 1,
  SD_CARD_STATE_IDENT   = 2,
  SD_CARD_STATE_STANDBY = 3,
  SD_CARD_STATE_TRAN    = 4,
  SD_CARD_STATE_DATA    = 5,
  SD_CARD_STATE_RECV    = 6,
  SD_CARD_STATE_PROG    = 7,
  SD_CARD_STATE_DISCARD = 8,
  SD_CARD_STATE_UNKNOWN = 0xff
} sd_card_state_t;

static inline const char *sd_card_state_to_str(sd_card_state_t c)
{
  switch (c) {
    case SD_CARD_STATE_IDLE    : return "IDLE";
    case SD_CARD_STATE_READY   : return "READY";
    case SD_CARD_STATE_IDENT   : return "IDENT";
    case SD_CARD_STATE_STANDBY : return "STANDBY";
    case SD_CARD_STATE_TRAN    : return "TRAN";
    case SD_CARD_STATE_DATA    : return "DATA";
    case SD_CARD_STATE_RECV    : return "RECV";
    case SD_CARD_STATE_PROG    : return "PROG";
    case SD_CARD_STATE_DISCARD : return "DISCARD";
    case SD_CARD_STATE_UNKNOWN :
    default: break;
  }
  return "UNKONWN";
}

typedef enum {
  /* zero-initialized value is not valid */
  SD_CARD_CAPACITY_UNKNOWN = 0,
  /* Standard Capacity SD Memory card: SIZE <= 2GB */
  SD_CARD_CAPACITY_SDSC,
  /* High Capacity SD Memory card: 2GB < SIZE <= 32GB */
  SD_CARD_CAPACITY_SDHC,
  /* Extended Capacity SD Memory card: 32GB < SIZE <= 2TB */
  SD_CARD_CAPACITY_SDXC,
  /* Ultra Capacity SD Memory card: 2TB < SIZE <= 128TB */
  SD_CARD_CAPACITY_SDUC,
} sd_card_capacity_t;

static inline const char *sd_card_capacity_to_str(sd_card_capacity_t c)
{
  switch (c) {
    case SD_CARD_CAPACITY_SDSC: return "SDSC";
    case SD_CARD_CAPACITY_SDHC: return "SDHC";
    case SD_CARD_CAPACITY_SDXC: return "SDXC";
    case SD_CARD_CAPACITY_SDUC: return "SDUC";
    case SD_CARD_CAPACITY_UNKNOWN:
    default: return "SD_UNKNOWN";
  }
}

static inline int sd_scr_get_scr_version(uint64_t scr)
{
  return (scr >> 60) & 0xf;
}

static inline int sd_scr_get_sd_spec(uint64_t scr)
{
  return (scr >> 56) & 0xf;
}

static inline bool sd_scr_get_data_stat_after_erase(uint64_t scr)
{
  return (scr >> 55) & 1;
}

static inline int sd_scr_get_sd_security(uint64_t scr)
{
  return (scr >> 52) & 7;
}

static inline int sd_scr_get_bus_widths(uint64_t scr)
{
  return (scr >> 48) & 0xf;
}

static inline bool sd_scr_get_sd_spec3(uint64_t scr)
{
  return (scr >> 47) & 1;
}

static inline bool sd_scr_get_ex_security(uint64_t scr)
{
  return (scr >> 43) & 0xf;
}

static inline bool sd_scr_get_sd_spec4(uint64_t scr)
{
  return (scr >> 42) & 1;
}

static inline int sd_scr_get_sd_specx(uint64_t scr)
{
  return (scr >> 38) & 0xf;
}

static inline int sd_scr_get_cmd_support(uint64_t scr)
{
  return (scr >> 32) & 0xf;
}

static inline void sd_scr_get_sd_spec_version(uint64_t scr, int *maj, int *min)
{
  int major = 0;
  int minor = 0;

  int sd_spec = sd_scr_get_sd_spec(scr);
  int sd_spec3 = sd_scr_get_sd_spec3(scr);
  int sd_spec4 = sd_scr_get_sd_spec4(scr);
  int sd_specx = sd_scr_get_sd_specx(scr);

  if (sd_spec == 0) {
    major = 1;
    goto out;
  }
  
  if (sd_spec == 1) {
    major = 1;
    minor = 10;
    goto out;
  }

  if (sd_spec != 2)
    goto out;

  /* SD_SPEC == 2 */
  if (!sd_spec3) {
    if (!sd_spec4 && !sd_specx)
      major = 2;

    goto out;
  }

  if (!sd_spec4 && !sd_specx) {
    major = 3;
    goto out;
  }

  if (sd_spec4 && !sd_specx) {
    major = 4;
    goto out;
  }

  if (sd_specx == 1) {
    major = 5;
    goto out;
  }

  if (sd_specx == 2) {
    major = 6;
    goto out;
  }

  if (sd_specx == 3) {
    major = 7;
    goto out;
  }

  if (sd_specx == 4) {
    major = 8;
    goto out;
  }

  if (sd_specx == 5) {
    major = 9;
    goto out;
  }

out:
  *maj = major;
  *min = minor;
}

struct sd_cmd6_response_raw {
  uint8_t data[64];
};

static inline int sd_cmd6_mode_0_resp_get_max_current(
  const struct sd_cmd6_response_raw *r)
{
  return r->data[1] | (r->data[0] << 8);
}

static inline int sd_cmd6_mode_0_resp_get_supp_fns_6(
  const struct sd_cmd6_response_raw *r)
{
  return r->data[3] | (r->data[2] << 8);
}

static inline int sd_cmd6_mode_0_resp_get_supp_fns_5(
  const struct sd_cmd6_response_raw *r)
{
  return r->data[5] | (r->data[4] << 8);
}

static inline int sd_cmd6_mode_0_resp_get_supp_fns_4(
  const struct sd_cmd6_response_raw *r)
{
  return r->data[7] | (r->data[6] << 8);
}

static inline int sd_cmd6_mode_0_resp_get_supp_fns_3(
  const struct sd_cmd6_response_raw *r)
{
  return r->data[9] | (r->data[8] << 8);
}

static inline int sd_cmd6_mode_0_resp_get_supp_fns_2(
  const struct sd_cmd6_response_raw *r)
{
  return r->data[11] | (r->data[10] << 8);
}

static inline int sd_cmd6_mode_0_resp_get_supp_fns_1(
  const struct sd_cmd6_response_raw *r)
{
  return r->data[13] | (r->data[12] << 8);
}

static inline bool sd_cmd6_mode_0_resp_is_sdr25_supported(
  const struct sd_cmd6_response_raw *r)
{
  return (sd_cmd6_mode_0_resp_get_supp_fns_1(r) >> 1) & 1;
}

static inline bool sd_scr_bus_width1_supported(uint64_t scr)
{
  return sd_scr_get_bus_widths(scr) & 1;
}

static inline bool sd_scr_bus_width4_supported(uint64_t scr)
{
  return (sd_scr_get_bus_widths(scr) >> 2) & 1;
}

struct sdhc;
struct sd_cmd;

struct sdhc_ops {
  int (*init)(void);
  void (*init_gpio)(void);
  void (*reset_regs)(void);
  void (*dump_regs)(bool);
  void (*dump_fsm_state)(void);
  void (*set_bus_width4)(void);
  void (*set_high_speed_clock)(void);
  int (*set_io_mode)(struct sdhc *s, sdhc_io_mode_t mode);
  int (*cmd)(struct sdhc *s, struct sd_cmd *c, uint64_t timeout_usec);
};

typedef enum {
  SDHC_SDCARD_MODE_UNKNOWN = 0,
  /* Mode SD is default mode after power up */
  SDHC_SDCARD_MODE_SD,
  /* Mode SPI is set when CMD0 is run with CS line asserted */
  SDHC_SDCARD_MODE_SPI,
} sdhc_sdcard_mode_t;

struct sdhc {
  sdhc_sdcard_mode_t sdcard_mode;
  struct sdhc_io io;
  bool initialized;
  bool blocking_mode;
  bool is_acmd_context;
  uint32_t rca;
  uint64_t scr;
  uint32_t device_id[4];
  uint32_t csd[4];
  sd_card_capacity_t card_capacity;
  bool UHS_II_support;
  bool bus_width_4_supported;
  bool bus_width_4;
  const struct sdhc_ops *ops;
  bool cmd8_response_received;
  sdhc_io_mode_t io_mode;
  uint32_t timeout_us;
};

int sdhc_init(struct sdhc *sdhc, struct sdhc_ops *ops);

int sdhc_set_io_mode(struct sdhc *sdhc, sdhc_io_mode_t mode);

int sdhc_read(struct sdhc *s, uint8_t *buf, uint32_t from_sector,
  uint32_t num_sectors);

int sdhc_write(struct sdhc *s, uint8_t *buf, uint32_t from_sector,
  uint32_t num_sectors);
