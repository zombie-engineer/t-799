#pragma once
#include <stdint.h>

typedef enum {
  SD_CARD_STATE_IDLE    = 0,
  SD_CARD_STATE_READY   = 1,
  SD_CARD_STATE_IDENT   = 2,
  SD_CARD_STATE_STANDBY = 3,
  SD_CARD_STATE_TRAN    = 4,
  SD_CARD_STATE_DATA    = 5,
  SD_CARD_STATE_RECV    = 5,
  SD_CARD_STATE_PROG    = 6,
  SD_CARD_STATE_DISCARD = 7,
  SD_CARD_STATE_UNKNOWN = 0xff
} sd_card_state_t;

typedef enum {
  SD_CARD_CAPACITY_UNKNOWN = 0,
  SD_CARD_CAPACITY_SDSC = 1,
  SD_CARD_CAPACITY_SDHC = 2
} sd_card_capacity_t;

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
