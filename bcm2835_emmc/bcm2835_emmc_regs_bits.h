#pragma once
#include <bitops.h>

#define BCM2835_EMMC_BLKSIZECNT_GET_BLKSIZE(v)           BITS_EXTRACT32(v, 0 , 10)
#define BCM2835_EMMC_BLKSIZECNT_GET_BLKCNT(v)            BITS_EXTRACT32(v, 16, 16)
#define BCM2835_EMMC_BLKSIZECNT_CLR_SET_BLKSIZE(v, set)          B_CLEAR_AND_SET32(v, set, 0 , 10)
#define BCM2835_EMMC_BLKSIZECNT_CLR_SET_BLKCNT(v, set)           B_CLEAR_AND_SET32(v, set, 16, 16)
#define BCM2835_EMMC_BLKSIZECNT_CLR_BLKSIZE(v)           B_CLEAR32(v, 0 , 10)
#define BCM2835_EMMC_BLKSIZECNT_CLR_BLKCNT(v)            B_CLEAR32(v, 16, 16)
#define BCM2835_EMMC_BLKSIZECNT_MASK_BLKSIZE             BF_MAKE_MASK32(0, 10)
#define BCM2835_EMMC_BLKSIZECNT_MASK_BLKCNT              BF_MAKE_MASK32(16, 16)
#define BCM2835_EMMC_BLKSIZECNT_SHIFT_BLKSIZE            0
#define BCM2835_EMMC_BLKSIZECNT_SHIFT_BLKCNT             16


static inline int bcm2835_emmc_blksizecnt_to_string(char *buf, int bufsz, uint32_t v)
{
  return snprintf(buf, bufsz, "%08x,BLKSIZE:%x,BLKCNT:%x",
    v,
    (int)BCM2835_EMMC_BLKSIZECNT_GET_BLKSIZE(v),
    (int)BCM2835_EMMC_BLKSIZECNT_GET_BLKCNT(v));
}
#define BCM2835_EMMC_ARG1_GET_ARGUMENT(v)                BITS_EXTRACT32(v, 0 , 32)
#define BCM2835_EMMC_ARG1_CLR_SET_ARGUMENT(v, set)               B_CLEAR_AND_SET32(v, set, 0 , 32)
#define BCM2835_EMMC_ARG1_CLR_ARGUMENT(v)                B_CLEAR32(v, 0 , 32)
#define BCM2835_EMMC_ARG1_MASK_ARGUMENT                  BF_MAKE_MASK32(0, 32)
#define BCM2835_EMMC_ARG1_SHIFT_ARGUMENT                 0


static inline int bcm2835_emmc_arg1_to_string(char *buf, int bufsz, uint32_t v)
{
  return snprintf(buf, bufsz, "%08x,ARGUMENT:%x",
    v,
    (int)BCM2835_EMMC_ARG1_GET_ARGUMENT(v));
}
#define BCM2835_EMMC_CMDTM_GET_TM_BLKCNT_EN(v)           BITS_EXTRACT32(v, 1 , 1 )
#define BCM2835_EMMC_CMDTM_GET_TM_AUTOCMD_EN(v)          BITS_EXTRACT32(v, 2 , 2 )
#define BCM2835_EMMC_CMDTM_GET_TM_DAT_DIR(v)             BITS_EXTRACT32(v, 4 , 1 )
#define BCM2835_EMMC_CMDTM_GET_TM_MULTI_BLOCK(v)         BITS_EXTRACT32(v, 5 , 1 )
#define BCM2835_EMMC_CMDTM_GET_CMD_RSPNS_TYPE(v)         BITS_EXTRACT32(v, 16, 2 )
#define BCM2835_EMMC_CMDTM_GET_CMD_CRCCHK_EN(v)          BITS_EXTRACT32(v, 19, 1 )
#define BCM2835_EMMC_CMDTM_GET_CMD_IXCHK_EN(v)           BITS_EXTRACT32(v, 20, 1 )
#define BCM2835_EMMC_CMDTM_GET_CMD_ISDATA(v)             BITS_EXTRACT32(v, 21, 1 )
#define BCM2835_EMMC_CMDTM_GET_CMD_TYPE(v)               BITS_EXTRACT32(v, 22, 2 )
#define BCM2835_EMMC_CMDTM_GET_CMD_INDEX(v)              BITS_EXTRACT32(v, 24, 6 )
#define BCM2835_EMMC_CMDTM_CLR_SET_TM_BLKCNT_EN(v, set)          B_CLEAR_AND_SET32(v, set, 1 , 1 )
#define BCM2835_EMMC_CMDTM_CLR_SET_TM_AUTOCMD_EN(v, set)         B_CLEAR_AND_SET32(v, set, 2 , 2 )
#define BCM2835_EMMC_CMDTM_CLR_SET_TM_DAT_DIR(v, set)            B_CLEAR_AND_SET32(v, set, 4 , 1 )
#define BCM2835_EMMC_CMDTM_CLR_SET_TM_MULTI_BLOCK(v, set)        B_CLEAR_AND_SET32(v, set, 5 , 1 )
#define BCM2835_EMMC_CMDTM_CLR_SET_CMD_RSPNS_TYPE(v, set)        B_CLEAR_AND_SET32(v, set, 16, 2 )
#define BCM2835_EMMC_CMDTM_CLR_SET_CMD_CRCCHK_EN(v, set)         B_CLEAR_AND_SET32(v, set, 19, 1 )
#define BCM2835_EMMC_CMDTM_CLR_SET_CMD_IXCHK_EN(v, set)          B_CLEAR_AND_SET32(v, set, 20, 1 )
#define BCM2835_EMMC_CMDTM_CLR_SET_CMD_ISDATA(v, set)            B_CLEAR_AND_SET32(v, set, 21, 1 )
#define BCM2835_EMMC_CMDTM_CLR_SET_CMD_TYPE(v, set)              B_CLEAR_AND_SET32(v, set, 22, 2 )
#define BCM2835_EMMC_CMDTM_CLR_SET_CMD_INDEX(v, set)             B_CLEAR_AND_SET32(v, set, 24, 6 )
#define BCM2835_EMMC_CMDTM_CLR_TM_BLKCNT_EN(v)           B_CLEAR32(v, 1 , 1 )
#define BCM2835_EMMC_CMDTM_CLR_TM_AUTOCMD_EN(v)          B_CLEAR32(v, 2 , 2 )
#define BCM2835_EMMC_CMDTM_CLR_TM_DAT_DIR(v)             B_CLEAR32(v, 4 , 1 )
#define BCM2835_EMMC_CMDTM_CLR_TM_MULTI_BLOCK(v)         B_CLEAR32(v, 5 , 1 )
#define BCM2835_EMMC_CMDTM_CLR_CMD_RSPNS_TYPE(v)         B_CLEAR32(v, 16, 2 )
#define BCM2835_EMMC_CMDTM_CLR_CMD_CRCCHK_EN(v)          B_CLEAR32(v, 19, 1 )
#define BCM2835_EMMC_CMDTM_CLR_CMD_IXCHK_EN(v)           B_CLEAR32(v, 20, 1 )
#define BCM2835_EMMC_CMDTM_CLR_CMD_ISDATA(v)             B_CLEAR32(v, 21, 1 )
#define BCM2835_EMMC_CMDTM_CLR_CMD_TYPE(v)               B_CLEAR32(v, 22, 2 )
#define BCM2835_EMMC_CMDTM_CLR_CMD_INDEX(v)              B_CLEAR32(v, 24, 6 )
#define BCM2835_EMMC_CMDTM_MASK_TM_BLKCNT_EN             BF_MAKE_MASK32(1, 1)
#define BCM2835_EMMC_CMDTM_MASK_TM_AUTOCMD_EN            BF_MAKE_MASK32(2, 2)
#define BCM2835_EMMC_CMDTM_MASK_TM_DAT_DIR               BF_MAKE_MASK32(4, 1)
#define BCM2835_EMMC_CMDTM_MASK_TM_MULTI_BLOCK           BF_MAKE_MASK32(5, 1)
#define BCM2835_EMMC_CMDTM_MASK_CMD_RSPNS_TYPE           BF_MAKE_MASK32(16, 2)
#define BCM2835_EMMC_CMDTM_MASK_CMD_CRCCHK_EN            BF_MAKE_MASK32(19, 1)
#define BCM2835_EMMC_CMDTM_MASK_CMD_IXCHK_EN             BF_MAKE_MASK32(20, 1)
#define BCM2835_EMMC_CMDTM_MASK_CMD_ISDATA               BF_MAKE_MASK32(21, 1)
#define BCM2835_EMMC_CMDTM_MASK_CMD_TYPE                 BF_MAKE_MASK32(22, 2)
#define BCM2835_EMMC_CMDTM_MASK_CMD_INDEX                BF_MAKE_MASK32(24, 6)
#define BCM2835_EMMC_CMDTM_SHIFT_TM_BLKCNT_EN            1
#define BCM2835_EMMC_CMDTM_SHIFT_TM_AUTOCMD_EN           2
#define BCM2835_EMMC_CMDTM_SHIFT_TM_DAT_DIR              4
#define BCM2835_EMMC_CMDTM_SHIFT_TM_MULTI_BLOCK          5
#define BCM2835_EMMC_CMDTM_SHIFT_CMD_RSPNS_TYPE          16
#define BCM2835_EMMC_CMDTM_SHIFT_CMD_CRCCHK_EN           19
#define BCM2835_EMMC_CMDTM_SHIFT_CMD_IXCHK_EN            20
#define BCM2835_EMMC_CMDTM_SHIFT_CMD_ISDATA              21
#define BCM2835_EMMC_CMDTM_SHIFT_CMD_TYPE                22
#define BCM2835_EMMC_CMDTM_SHIFT_CMD_INDEX               24


static inline int bcm2835_emmc_cmdtm_to_string(char *buf, int bufsz, uint32_t v)
{
  return snprintf(buf, bufsz, "%08x,TM_BLKCNT_EN:%x,TM_AUTOCMD_EN:%x,"
    "TM_DAT_DIR:%x,TM_MULTI_BLOCK:%x,CMD_RSPNS_TYPE:%x,CMD_CRCCHK_EN:%x,"
    "CMD_IXCHK_EN:%x,CMD_ISDATA:%x,CMD_TYPE:%x,CMD_INDEX:%x",
    v,
    (int)BCM2835_EMMC_CMDTM_GET_TM_BLKCNT_EN(v),
    (int)BCM2835_EMMC_CMDTM_GET_TM_AUTOCMD_EN(v),
    (int)BCM2835_EMMC_CMDTM_GET_TM_DAT_DIR(v),
    (int)BCM2835_EMMC_CMDTM_GET_TM_MULTI_BLOCK(v),
    (int)BCM2835_EMMC_CMDTM_GET_CMD_RSPNS_TYPE(v),
    (int)BCM2835_EMMC_CMDTM_GET_CMD_CRCCHK_EN(v),
    (int)BCM2835_EMMC_CMDTM_GET_CMD_IXCHK_EN(v),
    (int)BCM2835_EMMC_CMDTM_GET_CMD_ISDATA(v),
    (int)BCM2835_EMMC_CMDTM_GET_CMD_TYPE(v),
    (int)BCM2835_EMMC_CMDTM_GET_CMD_INDEX(v));
}
#define BCM2835_EMMC_RESP0_GET_RESPONSE(v)          BITS_EXTRACT32(v, 0 , 32)
#define BCM2835_EMMC_RESP0_CLR_SET_RESPONSE(v, set) B_CLEAR_AND_SET32(v, set, 0 , 32)
#define BCM2835_EMMC_RESP0_CLR_RESPONSE(v)          B_CLEAR32(v, 0 , 32)
#define BCM2835_EMMC_RESP0_MASK_RESPONSE            BF_MAKE_MASK32(0, 32)
#define BCM2835_EMMC_RESP0_SHIFT_RESPONSE           0


static inline int bcm2835_emmc_resp0_to_string(char *buf, int bufsz, uint32_t v)
{
  return snprintf(buf, bufsz, "%08x,RESPONSE:%x",
    v,
    (int)BCM2835_EMMC_RESP0_GET_RESPONSE(v));
}
#define BCM2835_EMMC_RESP1_GET_RESPONSE(v)          BITS_EXTRACT32(v, 0 , 32)
#define BCM2835_EMMC_RESP1_CLR_SET_RESPONSE(v, set) B_CLEAR_AND_SET32(v, set, 0 , 32)
#define BCM2835_EMMC_RESP1_CLR_RESPONSE(v)          B_CLEAR32(v, 0 , 32)
#define BCM2835_EMMC_RESP1_MASK_RESPONSE            BF_MAKE_MASK32(0, 32)
#define BCM2835_EMMC_RESP1_SHIFT_RESPONSE           0


static inline int bcm2835_emmc_resp1_to_string(char *buf, int bufsz, uint32_t v)
{
  return snprintf(buf, bufsz, "%08x,RESPONSE:%x",
    v,
    (int)BCM2835_EMMC_RESP1_GET_RESPONSE(v));
}
#define BCM2835_EMMC_RESP2_GET_RESPONSE(v)               BITS_EXTRACT32(v, 0 , 32)
#define BCM2835_EMMC_RESP2_CLR_SET_RESPONSE(v, set)              B_CLEAR_AND_SET32(v, set, 0 , 32)
#define BCM2835_EMMC_RESP2_CLR_RESPONSE(v)               B_CLEAR32(v, 0 , 32)
#define BCM2835_EMMC_RESP2_MASK_RESPONSE                 BF_MAKE_MASK32(0, 32)
#define BCM2835_EMMC_RESP2_SHIFT_RESPONSE                0


static inline int bcm2835_emmc_resp2_to_string(char *buf, int bufsz, uint32_t v)
{
  return snprintf(buf, bufsz, "%08x,RESPONSE:%x",
    v,
    (int)BCM2835_EMMC_RESP2_GET_RESPONSE(v));
}
#define BCM2835_EMMC_RESP3_GET_RESPONSE(v)               BITS_EXTRACT32(v, 0 , 32)
#define BCM2835_EMMC_RESP3_CLR_SET_RESPONSE(v, set)              B_CLEAR_AND_SET32(v, set, 0 , 32)
#define BCM2835_EMMC_RESP3_CLR_RESPONSE(v)               B_CLEAR32(v, 0 , 32)
#define BCM2835_EMMC_RESP3_MASK_RESPONSE                 BF_MAKE_MASK32(0, 32)
#define BCM2835_EMMC_RESP3_SHIFT_RESPONSE                0


static inline int bcm2835_emmc_resp3_to_string(char *buf, int bufsz, uint32_t v)
{
  return snprintf(buf, bufsz, "%08x,RESPONSE:%x",
    v,
    (int)BCM2835_EMMC_RESP3_GET_RESPONSE(v));
}
#define BCM2835_EMMC_DATA_GET_DATA(v)                    BITS_EXTRACT32(v, 0 , 32)
#define BCM2835_EMMC_DATA_CLR_SET_DATA(v, set)                   B_CLEAR_AND_SET32(v, set, 0 , 32)
#define BCM2835_EMMC_DATA_CLR_DATA(v)                    B_CLEAR32(v, 0 , 32)
#define BCM2835_EMMC_DATA_MASK_DATA                      BF_MAKE_MASK32(0, 32)
#define BCM2835_EMMC_DATA_SHIFT_DATA                     0


static inline int bcm2835_emmc_data_to_string(char *buf, int bufsz, uint32_t v)
{
  return snprintf(buf, bufsz, "%08x,DATA:%x",
    v,
    (int)BCM2835_EMMC_DATA_GET_DATA(v));
}
#define BCM2835_EMMC_STATUS_GET_CMD_INHIBIT(v)           BITS_EXTRACT32(v, 0 , 1 )
#define BCM2835_EMMC_STATUS_GET_DAT_INHIBIT(v)           BITS_EXTRACT32(v, 1 , 1 )
#define BCM2835_EMMC_STATUS_GET_DAT_ACTIVE(v)            BITS_EXTRACT32(v, 2 , 1 )
#define BCM2835_EMMC_STATUS_GET_WRITE_TRANSFER(v)        BITS_EXTRACT32(v, 8 , 1 )
#define BCM2835_EMMC_STATUS_GET_READ_TRANSFER(v)         BITS_EXTRACT32(v, 9 , 1 )
#define BCM2835_EMMC_STATUS_GET_DAT_LEVEL0(v)            BITS_EXTRACT32(v, 20, 4 )
#define BCM2835_EMMC_STATUS_GET_CMD_LEVEL(v)             BITS_EXTRACT32(v, 24, 1 )
#define BCM2835_EMMC_STATUS_GET_DAT_LEVEL1(v)            BITS_EXTRACT32(v, 25, 4 )
#define BCM2835_EMMC_STATUS_CLR_SET_CMD_INHIBIT(v, set)          B_CLEAR_AND_SET32(v, set, 0 , 1 )
#define BCM2835_EMMC_STATUS_CLR_SET_DAT_INHIBIT(v, set)          B_CLEAR_AND_SET32(v, set, 1 , 1 )
#define BCM2835_EMMC_STATUS_CLR_SET_DAT_ACTIVE(v, set)           B_CLEAR_AND_SET32(v, set, 2 , 1 )
#define BCM2835_EMMC_STATUS_CLR_SET_WRITE_TRANSFER(v, set)       B_CLEAR_AND_SET32(v, set, 8 , 1 )
#define BCM2835_EMMC_STATUS_CLR_SET_READ_TRANSFER(v, set)        B_CLEAR_AND_SET32(v, set, 9 , 1 )
#define BCM2835_EMMC_STATUS_CLR_SET_DAT_LEVEL0(v, set)           B_CLEAR_AND_SET32(v, set, 20, 4 )
#define BCM2835_EMMC_STATUS_CLR_SET_CMD_LEVEL(v, set)            B_CLEAR_AND_SET32(v, set, 24, 1 )
#define BCM2835_EMMC_STATUS_CLR_SET_DAT_LEVEL1(v, set)           B_CLEAR_AND_SET32(v, set, 25, 4 )
#define BCM2835_EMMC_STATUS_CLR_CMD_INHIBIT(v)           B_CLEAR32(v, 0 , 1 )
#define BCM2835_EMMC_STATUS_CLR_DAT_INHIBIT(v)           B_CLEAR32(v, 1 , 1 )
#define BCM2835_EMMC_STATUS_CLR_DAT_ACTIVE(v)            B_CLEAR32(v, 2 , 1 )
#define BCM2835_EMMC_STATUS_CLR_WRITE_TRANSFER(v)        B_CLEAR32(v, 8 , 1 )
#define BCM2835_EMMC_STATUS_CLR_READ_TRANSFER(v)         B_CLEAR32(v, 9 , 1 )
#define BCM2835_EMMC_STATUS_CLR_DAT_LEVEL0(v)            B_CLEAR32(v, 20, 4 )
#define BCM2835_EMMC_STATUS_CLR_CMD_LEVEL(v)             B_CLEAR32(v, 24, 1 )
#define BCM2835_EMMC_STATUS_CLR_DAT_LEVEL1(v)            B_CLEAR32(v, 25, 4 )
#define BCM2835_EMMC_STATUS_MASK_CMD_INHIBIT             BF_MAKE_MASK32(0, 1)
#define BCM2835_EMMC_STATUS_MASK_DAT_INHIBIT             BF_MAKE_MASK32(1, 1)
#define BCM2835_EMMC_STATUS_MASK_DAT_ACTIVE              BF_MAKE_MASK32(2, 1)
#define BCM2835_EMMC_STATUS_MASK_WRITE_TRANSFER          BF_MAKE_MASK32(8, 1)
#define BCM2835_EMMC_STATUS_MASK_READ_TRANSFER           BF_MAKE_MASK32(9, 1)
#define BCM2835_EMMC_STATUS_MASK_DAT_LEVEL0              BF_MAKE_MASK32(20, 4)
#define BCM2835_EMMC_STATUS_MASK_CMD_LEVEL               BF_MAKE_MASK32(24, 1)
#define BCM2835_EMMC_STATUS_MASK_DAT_LEVEL1              BF_MAKE_MASK32(25, 4)
#define BCM2835_EMMC_STATUS_SHIFT_CMD_INHIBIT            0
#define BCM2835_EMMC_STATUS_SHIFT_DAT_INHIBIT            1
#define BCM2835_EMMC_STATUS_SHIFT_DAT_ACTIVE             2
#define BCM2835_EMMC_STATUS_SHIFT_WRITE_TRANSFER         8
#define BCM2835_EMMC_STATUS_SHIFT_READ_TRANSFER          9
#define BCM2835_EMMC_STATUS_SHIFT_DAT_LEVEL0             20
#define BCM2835_EMMC_STATUS_SHIFT_CMD_LEVEL              24
#define BCM2835_EMMC_STATUS_SHIFT_DAT_LEVEL1             25


static inline int bcm2835_emmc_status_to_string(char *buf, int bufsz, uint32_t v)
{
  return snprintf(buf, bufsz, "%08x,CMD_INHIBIT:%x,DAT_INHIBIT:%x,DAT_ACTIVE:%x,WRITE_TRANSFER:%x,READ_TRANSFER:%x,DAT_LEVEL0:%x,CMD_LEVEL:%x,DAT_LEVEL1:%x",
    v,
    (int)BCM2835_EMMC_STATUS_GET_CMD_INHIBIT(v),
    (int)BCM2835_EMMC_STATUS_GET_DAT_INHIBIT(v),
    (int)BCM2835_EMMC_STATUS_GET_DAT_ACTIVE(v),
    (int)BCM2835_EMMC_STATUS_GET_WRITE_TRANSFER(v),
    (int)BCM2835_EMMC_STATUS_GET_READ_TRANSFER(v),
    (int)BCM2835_EMMC_STATUS_GET_DAT_LEVEL0(v),
    (int)BCM2835_EMMC_STATUS_GET_CMD_LEVEL(v),
    (int)BCM2835_EMMC_STATUS_GET_DAT_LEVEL1(v));
}
#define BCM2835_EMMC_CONTROL0_GET_HCTL_DWIDTH(v)         BITS_EXTRACT32(v, 1 , 1 )
#define BCM2835_EMMC_CONTROL0_GET_HCTL_HS_EN(v)          BITS_EXTRACT32(v, 2 , 1 )
#define BCM2835_EMMC_CONTROL0_GET_HCTL_8BIT(v)           BITS_EXTRACT32(v, 5 , 1 )
#define BCM2835_EMMC_CONTROL0_GET_GAP_STOP(v)            BITS_EXTRACT32(v, 16, 1 )
#define BCM2835_EMMC_CONTROL0_GET_GAP_RESTART(v)         BITS_EXTRACT32(v, 17, 1 )
#define BCM2835_EMMC_CONTROL0_GET_READWAIT_EN(v)         BITS_EXTRACT32(v, 18, 1 )
#define BCM2835_EMMC_CONTROL0_GET_GAP_IEN(v)             BITS_EXTRACT32(v, 19, 1 )
#define BCM2835_EMMC_CONTROL0_GET_SPI_MODE(v)            BITS_EXTRACT32(v, 20, 1 )
#define BCM2835_EMMC_CONTROL0_GET_BOOT_EN(v)             BITS_EXTRACT32(v, 21, 1 )
#define BCM2835_EMMC_CONTROL0_GET_ALT_BOOT_EN(v)         BITS_EXTRACT32(v, 22, 1 )
#define BCM2835_EMMC_CONTROL0_CLR_SET_HCTL_DWIDTH(v, set)        B_CLEAR_AND_SET32(v, set, 1 , 1 )
#define BCM2835_EMMC_CONTROL0_CLR_SET_HCTL_HS_EN(v, set)         B_CLEAR_AND_SET32(v, set, 2 , 1 )
#define BCM2835_EMMC_CONTROL0_CLR_SET_HCTL_8BIT(v, set)          B_CLEAR_AND_SET32(v, set, 5 , 1 )
#define BCM2835_EMMC_CONTROL0_CLR_SET_GAP_STOP(v, set)           B_CLEAR_AND_SET32(v, set, 16, 1 )
#define BCM2835_EMMC_CONTROL0_CLR_SET_GAP_RESTART(v, set)        B_CLEAR_AND_SET32(v, set, 17, 1 )
#define BCM2835_EMMC_CONTROL0_CLR_SET_READWAIT_EN(v, set)        B_CLEAR_AND_SET32(v, set, 18, 1 )
#define BCM2835_EMMC_CONTROL0_CLR_SET_GAP_IEN(v, set)            B_CLEAR_AND_SET32(v, set, 19, 1 )
#define BCM2835_EMMC_CONTROL0_CLR_SET_SPI_MODE(v, set)           B_CLEAR_AND_SET32(v, set, 20, 1 )
#define BCM2835_EMMC_CONTROL0_CLR_SET_BOOT_EN(v, set)            B_CLEAR_AND_SET32(v, set, 21, 1 )
#define BCM2835_EMMC_CONTROL0_CLR_SET_ALT_BOOT_EN(v, set)        B_CLEAR_AND_SET32(v, set, 22, 1 )
#define BCM2835_EMMC_CONTROL0_CLR_HCTL_DWIDTH(v)         B_CLEAR32(v, 1 , 1 )
#define BCM2835_EMMC_CONTROL0_CLR_HCTL_HS_EN(v)          B_CLEAR32(v, 2 , 1 )
#define BCM2835_EMMC_CONTROL0_CLR_HCTL_8BIT(v)           B_CLEAR32(v, 5 , 1 )
#define BCM2835_EMMC_CONTROL0_CLR_GAP_STOP(v)            B_CLEAR32(v, 16, 1 )
#define BCM2835_EMMC_CONTROL0_CLR_GAP_RESTART(v)         B_CLEAR32(v, 17, 1 )
#define BCM2835_EMMC_CONTROL0_CLR_READWAIT_EN(v)         B_CLEAR32(v, 18, 1 )
#define BCM2835_EMMC_CONTROL0_CLR_GAP_IEN(v)             B_CLEAR32(v, 19, 1 )
#define BCM2835_EMMC_CONTROL0_CLR_SPI_MODE(v)            B_CLEAR32(v, 20, 1 )
#define BCM2835_EMMC_CONTROL0_CLR_BOOT_EN(v)             B_CLEAR32(v, 21, 1 )
#define BCM2835_EMMC_CONTROL0_CLR_ALT_BOOT_EN(v)         B_CLEAR32(v, 22, 1 )
#define BCM2835_EMMC_CONTROL0_MASK_HCTL_DWIDTH           BF_MAKE_MASK32(1, 1)
#define BCM2835_EMMC_CONTROL0_MASK_HCTL_HS_EN            BF_MAKE_MASK32(2, 1)
#define BCM2835_EMMC_CONTROL0_MASK_HCTL_8BIT             BF_MAKE_MASK32(5, 1)
#define BCM2835_EMMC_CONTROL0_MASK_GAP_STOP              BF_MAKE_MASK32(16, 1)
#define BCM2835_EMMC_CONTROL0_MASK_GAP_RESTART           BF_MAKE_MASK32(17, 1)
#define BCM2835_EMMC_CONTROL0_MASK_READWAIT_EN           BF_MAKE_MASK32(18, 1)
#define BCM2835_EMMC_CONTROL0_MASK_GAP_IEN               BF_MAKE_MASK32(19, 1)
#define BCM2835_EMMC_CONTROL0_MASK_SPI_MODE              BF_MAKE_MASK32(20, 1)
#define BCM2835_EMMC_CONTROL0_MASK_BOOT_EN               BF_MAKE_MASK32(21, 1)
#define BCM2835_EMMC_CONTROL0_MASK_ALT_BOOT_EN           BF_MAKE_MASK32(22, 1)
#define BCM2835_EMMC_CONTROL0_SHIFT_HCTL_DWIDTH          1
#define BCM2835_EMMC_CONTROL0_SHIFT_HCTL_HS_EN           2
#define BCM2835_EMMC_CONTROL0_SHIFT_HCTL_8BIT            5
#define BCM2835_EMMC_CONTROL0_SHIFT_GAP_STOP             16
#define BCM2835_EMMC_CONTROL0_SHIFT_GAP_RESTART          17
#define BCM2835_EMMC_CONTROL0_SHIFT_READWAIT_EN          18
#define BCM2835_EMMC_CONTROL0_SHIFT_GAP_IEN              19
#define BCM2835_EMMC_CONTROL0_SHIFT_SPI_MODE             20
#define BCM2835_EMMC_CONTROL0_SHIFT_BOOT_EN              21
#define BCM2835_EMMC_CONTROL0_SHIFT_ALT_BOOT_EN          22

static inline int bcm2835_emmc_control0_bitmask_to_string(char *buf, int bufsz, uint32_t v)
{
  int n = 0;
  int first = 1;
  if (BCM2835_EMMC_CONTROL0_GET_HCTL_DWIDTH(v)) {
    n += snprintf(buf + n, bufsz - n, "%sHCTL_DWIDTH", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_CONTROL0_GET_HCTL_HS_EN(v)) {
    n += snprintf(buf + n, bufsz - n, "%sHCTL_HS_EN", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_CONTROL0_GET_HCTL_8BIT(v)) {
    n += snprintf(buf + n, bufsz - n, "%sHCTL_8BIT", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_CONTROL0_GET_GAP_STOP(v)) {
    n += snprintf(buf + n, bufsz - n, "%sGAP_STOP", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_CONTROL0_GET_GAP_RESTART(v)) {
    n += snprintf(buf + n, bufsz - n, "%sGAP_RESTART", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_CONTROL0_GET_READWAIT_EN(v)) {
    n += snprintf(buf + n, bufsz - n, "%sREADWAIT_EN", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_CONTROL0_GET_GAP_IEN(v)) {
    n += snprintf(buf + n, bufsz - n, "%sGAP_IEN", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_CONTROL0_GET_SPI_MODE(v)) {
    n += snprintf(buf + n, bufsz - n, "%sSPI_MODE", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_CONTROL0_GET_BOOT_EN(v)) {
    n += snprintf(buf + n, bufsz - n, "%sBOOT_EN", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_CONTROL0_GET_ALT_BOOT_EN(v)) {
    n += snprintf(buf + n, bufsz - n, "%sALT_BOOT_EN", first ? "" : ",");
    first = 0;
  }
  return n;
}


static inline int bcm2835_emmc_control0_to_string(char *buf, int bufsz, uint32_t v)
{
  return snprintf(buf, bufsz, "%08x,HCTL_DWIDTH:%x,HCTL_HS_EN:%x,HCTL_8BIT:%x,GAP_STOP:%x,GAP_RESTART:%x,READWAIT_EN:%x,GAP_IEN:%x,SPI_MODE:%x,BOOT_EN:%x,ALT_BOOT_EN:%x",
    v,
    (int)BCM2835_EMMC_CONTROL0_GET_HCTL_DWIDTH(v),
    (int)BCM2835_EMMC_CONTROL0_GET_HCTL_HS_EN(v),
    (int)BCM2835_EMMC_CONTROL0_GET_HCTL_8BIT(v),
    (int)BCM2835_EMMC_CONTROL0_GET_GAP_STOP(v),
    (int)BCM2835_EMMC_CONTROL0_GET_GAP_RESTART(v),
    (int)BCM2835_EMMC_CONTROL0_GET_READWAIT_EN(v),
    (int)BCM2835_EMMC_CONTROL0_GET_GAP_IEN(v),
    (int)BCM2835_EMMC_CONTROL0_GET_SPI_MODE(v),
    (int)BCM2835_EMMC_CONTROL0_GET_BOOT_EN(v),
    (int)BCM2835_EMMC_CONTROL0_GET_ALT_BOOT_EN(v));
}
#define BCM2835_EMMC_CONTROL1_GET_CLK_INTLEN(v)          BITS_EXTRACT32(v, 0 , 1 )
#define BCM2835_EMMC_CONTROL1_GET_CLK_STABLE(v)          BITS_EXTRACT32(v, 1 , 1 )
#define BCM2835_EMMC_CONTROL1_GET_CLK_EN(v)              BITS_EXTRACT32(v, 2 , 1 )
#define BCM2835_EMMC_CONTROL1_GET_CLK_GENSEL(v)          BITS_EXTRACT32(v, 5 , 1 )
#define BCM2835_EMMC_CONTROL1_GET_CLK_FREQ_MS2(v)        BITS_EXTRACT32(v, 6 , 2 )
#define BCM2835_EMMC_CONTROL1_GET_CLK_FREQ8(v)           BITS_EXTRACT32(v, 8 , 8 )
#define BCM2835_EMMC_CONTROL1_GET_DATA_TOUNIT(v)         BITS_EXTRACT32(v, 16, 4 )
#define BCM2835_EMMC_CONTROL1_GET_SRST_HC(v)             BITS_EXTRACT32(v, 24, 1 )
#define BCM2835_EMMC_CONTROL1_GET_SRST_CMD(v)            BITS_EXTRACT32(v, 25, 1 )
#define BCM2835_EMMC_CONTROL1_GET_SRST_DATA(v)           BITS_EXTRACT32(v, 26, 1 )
#define BCM2835_EMMC_CONTROL1_CLR_SET_CLK_INTLEN(v, set)         B_CLEAR_AND_SET32(v, set, 0 , 1 )
#define BCM2835_EMMC_CONTROL1_CLR_SET_CLK_STABLE(v, set)         B_CLEAR_AND_SET32(v, set, 1 , 1 )
#define BCM2835_EMMC_CONTROL1_CLR_SET_CLK_EN(v, set)             B_CLEAR_AND_SET32(v, set, 2 , 1 )
#define BCM2835_EMMC_CONTROL1_CLR_SET_CLK_GENSEL(v, set)         B_CLEAR_AND_SET32(v, set, 5 , 1 )
#define BCM2835_EMMC_CONTROL1_CLR_SET_CLK_FREQ_MS2(v, set)       B_CLEAR_AND_SET32(v, set, 6 , 2 )
#define BCM2835_EMMC_CONTROL1_CLR_SET_CLK_FREQ8(v, set)          B_CLEAR_AND_SET32(v, set, 8 , 8 )
#define BCM2835_EMMC_CONTROL1_CLR_SET_DATA_TOUNIT(v, set)        B_CLEAR_AND_SET32(v, set, 16, 4 )
#define BCM2835_EMMC_CONTROL1_CLR_SET_SRST_HC(v, set)            B_CLEAR_AND_SET32(v, set, 24, 1 )
#define BCM2835_EMMC_CONTROL1_CLR_SET_SRST_CMD(v, set)           B_CLEAR_AND_SET32(v, set, 25, 1 )
#define BCM2835_EMMC_CONTROL1_CLR_SET_SRST_DATA(v, set)          B_CLEAR_AND_SET32(v, set, 26, 1 )
#define BCM2835_EMMC_CONTROL1_CLR_CLK_INTLEN(v)          B_CLEAR32(v, 0 , 1 )
#define BCM2835_EMMC_CONTROL1_CLR_CLK_STABLE(v)          B_CLEAR32(v, 1 , 1 )
#define BCM2835_EMMC_CONTROL1_CLR_CLK_EN(v)              B_CLEAR32(v, 2 , 1 )
#define BCM2835_EMMC_CONTROL1_CLR_CLK_GENSEL(v)          B_CLEAR32(v, 5 , 1 )
#define BCM2835_EMMC_CONTROL1_CLR_CLK_FREQ_MS2(v)        B_CLEAR32(v, 6 , 2 )
#define BCM2835_EMMC_CONTROL1_CLR_CLK_FREQ8(v)           B_CLEAR32(v, 8 , 8 )
#define BCM2835_EMMC_CONTROL1_CLR_DATA_TOUNIT(v)         B_CLEAR32(v, 16, 4 )
#define BCM2835_EMMC_CONTROL1_CLR_SRST_HC(v)             B_CLEAR32(v, 24, 1 )
#define BCM2835_EMMC_CONTROL1_CLR_SRST_CMD(v)            B_CLEAR32(v, 25, 1 )
#define BCM2835_EMMC_CONTROL1_CLR_SRST_DATA(v)           B_CLEAR32(v, 26, 1 )
#define BCM2835_EMMC_CONTROL1_MASK_CLK_INTLEN            BF_MAKE_MASK32(0, 1)
#define BCM2835_EMMC_CONTROL1_MASK_CLK_STABLE            BF_MAKE_MASK32(1, 1)
#define BCM2835_EMMC_CONTROL1_MASK_CLK_EN                BF_MAKE_MASK32(2, 1)
#define BCM2835_EMMC_CONTROL1_MASK_CLK_GENSEL            BF_MAKE_MASK32(5, 1)
#define BCM2835_EMMC_CONTROL1_MASK_CLK_FREQ_MS2          BF_MAKE_MASK32(6, 2)
#define BCM2835_EMMC_CONTROL1_MASK_CLK_FREQ8             BF_MAKE_MASK32(8, 8)
#define BCM2835_EMMC_CONTROL1_MASK_DATA_TOUNIT           BF_MAKE_MASK32(16, 4)
#define BCM2835_EMMC_CONTROL1_MASK_SRST_HC               BF_MAKE_MASK32(24, 1)
#define BCM2835_EMMC_CONTROL1_MASK_SRST_CMD              BF_MAKE_MASK32(25, 1)
#define BCM2835_EMMC_CONTROL1_MASK_SRST_DATA             BF_MAKE_MASK32(26, 1)
#define BCM2835_EMMC_CONTROL1_SHIFT_CLK_INTLEN           0
#define BCM2835_EMMC_CONTROL1_SHIFT_CLK_STABLE           1
#define BCM2835_EMMC_CONTROL1_SHIFT_CLK_EN               2
#define BCM2835_EMMC_CONTROL1_SHIFT_CLK_GENSEL           5
#define BCM2835_EMMC_CONTROL1_SHIFT_CLK_FREQ_MS2         6
#define BCM2835_EMMC_CONTROL1_SHIFT_CLK_FREQ8            8
#define BCM2835_EMMC_CONTROL1_SHIFT_DATA_TOUNIT          16
#define BCM2835_EMMC_CONTROL1_SHIFT_SRST_HC              24
#define BCM2835_EMMC_CONTROL1_SHIFT_SRST_CMD             25
#define BCM2835_EMMC_CONTROL1_SHIFT_SRST_DATA            26


static inline int bcm2835_emmc_control1_to_string(char *buf, int bufsz, uint32_t v)
{
  return snprintf(buf, bufsz, "%08x,CLK_INTLEN:%x,CLK_STABLE:%x,CLK_EN:%x,CLK_GENSEL:%x,CLK_FREQ_MS2:%x,CLK_FREQ8:%x,DATA_TOUNIT:%x,SRST_HC:%x,SRST_CMD:%x,SRST_DATA:%x",
    v,
    (int)BCM2835_EMMC_CONTROL1_GET_CLK_INTLEN(v),
    (int)BCM2835_EMMC_CONTROL1_GET_CLK_STABLE(v),
    (int)BCM2835_EMMC_CONTROL1_GET_CLK_EN(v),
    (int)BCM2835_EMMC_CONTROL1_GET_CLK_GENSEL(v),
    (int)BCM2835_EMMC_CONTROL1_GET_CLK_FREQ_MS2(v),
    (int)BCM2835_EMMC_CONTROL1_GET_CLK_FREQ8(v),
    (int)BCM2835_EMMC_CONTROL1_GET_DATA_TOUNIT(v),
    (int)BCM2835_EMMC_CONTROL1_GET_SRST_HC(v),
    (int)BCM2835_EMMC_CONTROL1_GET_SRST_CMD(v),
    (int)BCM2835_EMMC_CONTROL1_GET_SRST_DATA(v));
}
#define BCM2835_EMMC_INTERRUPT_GET_CMD_DONE(v)           BITS_EXTRACT32(v, 0 , 1 )
#define BCM2835_EMMC_INTERRUPT_GET_DATA_DONE(v)          BITS_EXTRACT32(v, 1 , 1 )
#define BCM2835_EMMC_INTERRUPT_GET_BLOCK_GAP(v)          BITS_EXTRACT32(v, 2 , 1 )
#define BCM2835_EMMC_INTERRUPT_GET_WRITE_RDY(v)          BITS_EXTRACT32(v, 4 , 1 )
#define BCM2835_EMMC_INTERRUPT_GET_READ_RDY(v)           BITS_EXTRACT32(v, 5 , 1 )
#define BCM2835_EMMC_INTERRUPT_GET_CARD(v)               BITS_EXTRACT32(v, 8 , 1 )
#define BCM2835_EMMC_INTERRUPT_GET_RETUNE(v)             BITS_EXTRACT32(v, 12, 1 )
#define BCM2835_EMMC_INTERRUPT_GET_BOOTACK(v)            BITS_EXTRACT32(v, 13, 1 )
#define BCM2835_EMMC_INTERRUPT_GET_ENDBOOT(v)            BITS_EXTRACT32(v, 14, 1 )
#define BCM2835_EMMC_INTERRUPT_GET_ERR(v)                BITS_EXTRACT32(v, 15, 1 )
#define BCM2835_EMMC_INTERRUPT_GET_CTO_ERR(v)            BITS_EXTRACT32(v, 16, 1 )
#define BCM2835_EMMC_INTERRUPT_GET_CCRC_ERR(v)           BITS_EXTRACT32(v, 17, 1 )
#define BCM2835_EMMC_INTERRUPT_GET_CEND_ERR(v)           BITS_EXTRACT32(v, 18, 1 )
#define BCM2835_EMMC_INTERRUPT_GET_CBAD_ERR(v)           BITS_EXTRACT32(v, 19, 1 )
#define BCM2835_EMMC_INTERRUPT_GET_DTO_ERR(v)            BITS_EXTRACT32(v, 20, 1 )
#define BCM2835_EMMC_INTERRUPT_GET_DCRC_ERR(v)           BITS_EXTRACT32(v, 21, 1 )
#define BCM2835_EMMC_INTERRUPT_GET_DEND_ERR(v)           BITS_EXTRACT32(v, 22, 1 )
#define BCM2835_EMMC_INTERRUPT_GET_ACMD_ERR(v)           BITS_EXTRACT32(v, 24, 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_SET_CMD_DONE(v, set)          B_CLEAR_AND_SET32(v, set, 0 , 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_SET_DATA_DONE(v, set)         B_CLEAR_AND_SET32(v, set, 1 , 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_SET_BLOCK_GAP(v, set)         B_CLEAR_AND_SET32(v, set, 2 , 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_SET_WRITE_RDY(v, set)         B_CLEAR_AND_SET32(v, set, 4 , 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_SET_READ_RDY(v, set)          B_CLEAR_AND_SET32(v, set, 5 , 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_SET_CARD(v, set)              B_CLEAR_AND_SET32(v, set, 8 , 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_SET_RETUNE(v, set)            B_CLEAR_AND_SET32(v, set, 12, 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_SET_BOOTACK(v, set)           B_CLEAR_AND_SET32(v, set, 13, 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_SET_ENDBOOT(v, set)           B_CLEAR_AND_SET32(v, set, 14, 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_SET_ERR(v, set)               B_CLEAR_AND_SET32(v, set, 15, 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_SET_CTO_ERR(v, set)           B_CLEAR_AND_SET32(v, set, 16, 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_SET_CCRC_ERR(v, set)          B_CLEAR_AND_SET32(v, set, 17, 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_SET_CEND_ERR(v, set)          B_CLEAR_AND_SET32(v, set, 18, 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_SET_CBAD_ERR(v, set)          B_CLEAR_AND_SET32(v, set, 19, 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_SET_DTO_ERR(v, set)           B_CLEAR_AND_SET32(v, set, 20, 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_SET_DCRC_ERR(v, set)          B_CLEAR_AND_SET32(v, set, 21, 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_SET_DEND_ERR(v, set)          B_CLEAR_AND_SET32(v, set, 22, 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_SET_ACMD_ERR(v, set)          B_CLEAR_AND_SET32(v, set, 24, 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_CMD_DONE(v)           B_CLEAR32(v, 0 , 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_DATA_DONE(v)          B_CLEAR32(v, 1 , 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_BLOCK_GAP(v)          B_CLEAR32(v, 2 , 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_DMA_DONE(v)           B_CLEAR32(v, 3 , 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_WRITE_RDY(v)          B_CLEAR32(v, 4 , 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_READ_RDY(v)           B_CLEAR32(v, 5 , 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_CARD(v)               B_CLEAR32(v, 8 , 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_RETUNE(v)             B_CLEAR32(v, 12, 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_BOOTACK(v)            B_CLEAR32(v, 13, 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_ENDBOOT(v)            B_CLEAR32(v, 14, 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_ERR(v)                B_CLEAR32(v, 15, 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_CTO_ERR(v)            B_CLEAR32(v, 16, 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_CCRC_ERR(v)           B_CLEAR32(v, 17, 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_CEND_ERR(v)           B_CLEAR32(v, 18, 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_CBAD_ERR(v)           B_CLEAR32(v, 19, 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_DTO_ERR(v)            B_CLEAR32(v, 20, 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_DCRC_ERR(v)           B_CLEAR32(v, 21, 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_DEND_ERR(v)           B_CLEAR32(v, 22, 1 )
#define BCM2835_EMMC_INTERRUPT_CLR_ACMD_ERR(v)           B_CLEAR32(v, 24, 1 )
#define BCM2835_EMMC_INTERRUPT_MASK_CMD_DONE             BF_MAKE_MASK32(0, 1)
#define BCM2835_EMMC_INTERRUPT_MASK_DATA_DONE            BF_MAKE_MASK32(1, 1)
#define BCM2835_EMMC_INTERRUPT_MASK_BLOCK_GAP            BF_MAKE_MASK32(2, 1)
#define BCM2835_EMMC_INTERRUPT_MASK_WRITE_RDY            BF_MAKE_MASK32(4, 1)
#define BCM2835_EMMC_INTERRUPT_MASK_READ_RDY             BF_MAKE_MASK32(5, 1)
#define BCM2835_EMMC_INTERRUPT_MASK_CARD                 BF_MAKE_MASK32(8, 1)
#define BCM2835_EMMC_INTERRUPT_MASK_RETUNE               BF_MAKE_MASK32(12, 1)
#define BCM2835_EMMC_INTERRUPT_MASK_BOOTACK              BF_MAKE_MASK32(13, 1)
#define BCM2835_EMMC_INTERRUPT_MASK_ENDBOOT              BF_MAKE_MASK32(14, 1)
#define BCM2835_EMMC_INTERRUPT_MASK_ERR                  BF_MAKE_MASK32(15, 1)
#define BCM2835_EMMC_INTERRUPT_MASK_CTO_ERR              BF_MAKE_MASK32(16, 1)
#define BCM2835_EMMC_INTERRUPT_MASK_CCRC_ERR             BF_MAKE_MASK32(17, 1)
#define BCM2835_EMMC_INTERRUPT_MASK_CEND_ERR             BF_MAKE_MASK32(18, 1)
#define BCM2835_EMMC_INTERRUPT_MASK_CBAD_ERR             BF_MAKE_MASK32(19, 1)
#define BCM2835_EMMC_INTERRUPT_MASK_DTO_ERR              BF_MAKE_MASK32(20, 1)
#define BCM2835_EMMC_INTERRUPT_MASK_DCRC_ERR             BF_MAKE_MASK32(21, 1)
#define BCM2835_EMMC_INTERRUPT_MASK_DEND_ERR             BF_MAKE_MASK32(22, 1)
#define BCM2835_EMMC_INTERRUPT_MASK_ACMD_ERR             BF_MAKE_MASK32(24, 1)
#define BCM2835_EMMC_INTERRUPT_SHIFT_CMD_DONE            0
#define BCM2835_EMMC_INTERRUPT_SHIFT_DATA_DONE           1
#define BCM2835_EMMC_INTERRUPT_SHIFT_BLOCK_GAP           2
#define BCM2835_EMMC_INTERRUPT_SHIFT_WRITE_RDY           4
#define BCM2835_EMMC_INTERRUPT_SHIFT_READ_RDY            5
#define BCM2835_EMMC_INTERRUPT_SHIFT_CARD                8
#define BCM2835_EMMC_INTERRUPT_SHIFT_RETUNE              12
#define BCM2835_EMMC_INTERRUPT_SHIFT_BOOTACK             13
#define BCM2835_EMMC_INTERRUPT_SHIFT_ENDBOOT             14
#define BCM2835_EMMC_INTERRUPT_SHIFT_ERR                 15
#define BCM2835_EMMC_INTERRUPT_SHIFT_CTO_ERR             16
#define BCM2835_EMMC_INTERRUPT_SHIFT_CCRC_ERR            17
#define BCM2835_EMMC_INTERRUPT_SHIFT_CEND_ERR            18
#define BCM2835_EMMC_INTERRUPT_SHIFT_CBAD_ERR            19
#define BCM2835_EMMC_INTERRUPT_SHIFT_DTO_ERR             20
#define BCM2835_EMMC_INTERRUPT_SHIFT_DCRC_ERR            21
#define BCM2835_EMMC_INTERRUPT_SHIFT_DEND_ERR            22
#define BCM2835_EMMC_INTERRUPT_SHIFT_ACMD_ERR            24

static inline int bcm2835_emmc_interrupt_bitmask_to_string(char *buf, int bufsz, uint32_t v)
{
  int n = 0;
  int first = 1;
  if (BCM2835_EMMC_INTERRUPT_GET_CMD_DONE(v)) {
    n += snprintf(buf + n, bufsz - n, "%sCMD_DONE", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_INTERRUPT_GET_DATA_DONE(v)) {
    n += snprintf(buf + n, bufsz - n, "%sDATA_DONE", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_INTERRUPT_GET_BLOCK_GAP(v)) {
    n += snprintf(buf + n, bufsz - n, "%sBLOCK_GAP", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_INTERRUPT_GET_WRITE_RDY(v)) {
    n += snprintf(buf + n, bufsz - n, "%sWRITE_RDY", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_INTERRUPT_GET_READ_RDY(v)) {
    n += snprintf(buf + n, bufsz - n, "%sREAD_RDY", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_INTERRUPT_GET_CARD(v)) {
    n += snprintf(buf + n, bufsz - n, "%sCARD", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_INTERRUPT_GET_RETUNE(v)) {
    n += snprintf(buf + n, bufsz - n, "%sRETUNE", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_INTERRUPT_GET_BOOTACK(v)) {
    n += snprintf(buf + n, bufsz - n, "%sBOOTACK", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_INTERRUPT_GET_ENDBOOT(v)) {
    n += snprintf(buf + n, bufsz - n, "%sENDBOOT", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_INTERRUPT_GET_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sERR", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_INTERRUPT_GET_CTO_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sCTO_ERR", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_INTERRUPT_GET_CCRC_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sCCRC_ERR", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_INTERRUPT_GET_CEND_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sCEND_ERR", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_INTERRUPT_GET_CBAD_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sCBAD_ERR", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_INTERRUPT_GET_DTO_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sDTO_ERR", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_INTERRUPT_GET_DCRC_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sDCRC_ERR", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_INTERRUPT_GET_DEND_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sDEND_ERR", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_INTERRUPT_GET_ACMD_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sACMD_ERR", first ? "" : ",");
    first = 0;
  }
  return n;
}


static inline int bcm2835_emmc_interrupt_to_string(char *buf, int bufsz, uint32_t v)
{
  return snprintf(buf, bufsz, "%08x,CMD_DONE:%x,DATA_DONE:%x,BLOCK_GAP:%x,WRITE_RDY:%x,READ_RDY:%x,CARD:%x,RETUNE:%x,BOOTACK:%x,ENDBOOT:%x,ERR:%x,CTO_ERR:%x,CCRC_ERR:%x,CEND_ERR:%x,CBAD_ERR:%x,DTO_ERR:%x,DCRC_ERR:%x,DEND_ERR:%x,ACMD_ERR:%x",
    v,
    (int)BCM2835_EMMC_INTERRUPT_GET_CMD_DONE(v),
    (int)BCM2835_EMMC_INTERRUPT_GET_DATA_DONE(v),
    (int)BCM2835_EMMC_INTERRUPT_GET_BLOCK_GAP(v),
    (int)BCM2835_EMMC_INTERRUPT_GET_WRITE_RDY(v),
    (int)BCM2835_EMMC_INTERRUPT_GET_READ_RDY(v),
    (int)BCM2835_EMMC_INTERRUPT_GET_CARD(v),
    (int)BCM2835_EMMC_INTERRUPT_GET_RETUNE(v),
    (int)BCM2835_EMMC_INTERRUPT_GET_BOOTACK(v),
    (int)BCM2835_EMMC_INTERRUPT_GET_ENDBOOT(v),
    (int)BCM2835_EMMC_INTERRUPT_GET_ERR(v),
    (int)BCM2835_EMMC_INTERRUPT_GET_CTO_ERR(v),
    (int)BCM2835_EMMC_INTERRUPT_GET_CCRC_ERR(v),
    (int)BCM2835_EMMC_INTERRUPT_GET_CEND_ERR(v),
    (int)BCM2835_EMMC_INTERRUPT_GET_CBAD_ERR(v),
    (int)BCM2835_EMMC_INTERRUPT_GET_DTO_ERR(v),
    (int)BCM2835_EMMC_INTERRUPT_GET_DCRC_ERR(v),
    (int)BCM2835_EMMC_INTERRUPT_GET_DEND_ERR(v),
    (int)BCM2835_EMMC_INTERRUPT_GET_ACMD_ERR(v));
}
#define BCM2835_EMMC_IRPT_MASK_GET_CMD_DONE(v)           BITS_EXTRACT32(v, 0 , 1 )
#define BCM2835_EMMC_IRPT_MASK_GET_DATA_DONE(v)          BITS_EXTRACT32(v, 1 , 1 )
#define BCM2835_EMMC_IRPT_MASK_GET_BLOCK_GAP(v)          BITS_EXTRACT32(v, 2 , 1 )
#define BCM2835_EMMC_IRPT_MASK_GET_WRITE_RDY(v)          BITS_EXTRACT32(v, 4 , 1 )
#define BCM2835_EMMC_IRPT_MASK_GET_READ_RDY(v)           BITS_EXTRACT32(v, 5 , 1 )
#define BCM2835_EMMC_IRPT_MASK_GET_CARD(v)               BITS_EXTRACT32(v, 8 , 1 )
#define BCM2835_EMMC_IRPT_MASK_GET_RETUNE(v)             BITS_EXTRACT32(v, 12, 1 )
#define BCM2835_EMMC_IRPT_MASK_GET_BOOTACK(v)            BITS_EXTRACT32(v, 13, 1 )
#define BCM2835_EMMC_IRPT_MASK_GET_ENDBOOT(v)            BITS_EXTRACT32(v, 14, 1 )
#define BCM2835_EMMC_IRPT_MASK_GET_ERR(v)                BITS_EXTRACT32(v, 15, 1 )
#define BCM2835_EMMC_IRPT_MASK_GET_CTO_ERR(v)            BITS_EXTRACT32(v, 16, 1 )
#define BCM2835_EMMC_IRPT_MASK_GET_CCRC_ERR(v)           BITS_EXTRACT32(v, 17, 1 )
#define BCM2835_EMMC_IRPT_MASK_GET_CEND_ERR(v)           BITS_EXTRACT32(v, 18, 1 )
#define BCM2835_EMMC_IRPT_MASK_GET_CBAD_ERR(v)           BITS_EXTRACT32(v, 19, 1 )
#define BCM2835_EMMC_IRPT_MASK_GET_DTO_ERR(v)            BITS_EXTRACT32(v, 20, 1 )
#define BCM2835_EMMC_IRPT_MASK_GET_DCRC_ERR(v)           BITS_EXTRACT32(v, 21, 1 )
#define BCM2835_EMMC_IRPT_MASK_GET_DEND_ERR(v)           BITS_EXTRACT32(v, 22, 1 )
#define BCM2835_EMMC_IRPT_MASK_GET_ACMD_ERR(v)           BITS_EXTRACT32(v, 24, 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_SET_CMD_DONE(v, set)          B_CLEAR_AND_SET32(v, set, 0 , 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_SET_DATA_DONE(v, set)         B_CLEAR_AND_SET32(v, set, 1 , 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_SET_BLOCK_GAP(v, set)         B_CLEAR_AND_SET32(v, set, 2 , 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_SET_WRITE_RDY(v, set)         B_CLEAR_AND_SET32(v, set, 4 , 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_SET_READ_RDY(v, set)          B_CLEAR_AND_SET32(v, set, 5 , 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_SET_CARD(v, set)              B_CLEAR_AND_SET32(v, set, 8 , 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_SET_RETUNE(v, set)            B_CLEAR_AND_SET32(v, set, 12, 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_SET_BOOTACK(v, set)           B_CLEAR_AND_SET32(v, set, 13, 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_SET_ENDBOOT(v, set)           B_CLEAR_AND_SET32(v, set, 14, 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_SET_ERR(v, set)               B_CLEAR_AND_SET32(v, set, 15, 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_SET_CTO_ERR(v, set)           B_CLEAR_AND_SET32(v, set, 16, 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_SET_CCRC_ERR(v, set)          B_CLEAR_AND_SET32(v, set, 17, 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_SET_CEND_ERR(v, set)          B_CLEAR_AND_SET32(v, set, 18, 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_SET_CBAD_ERR(v, set)          B_CLEAR_AND_SET32(v, set, 19, 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_SET_DTO_ERR(v, set)           B_CLEAR_AND_SET32(v, set, 20, 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_SET_DCRC_ERR(v, set)          B_CLEAR_AND_SET32(v, set, 21, 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_SET_DEND_ERR(v, set)          B_CLEAR_AND_SET32(v, set, 22, 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_SET_ACMD_ERR(v, set)          B_CLEAR_AND_SET32(v, set, 24, 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_CMD_DONE(v)           B_CLEAR32(v, 0 , 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_DATA_DONE(v)          B_CLEAR32(v, 1 , 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_BLOCK_GAP(v)          B_CLEAR32(v, 2 , 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_WRITE_RDY(v)          B_CLEAR32(v, 4 , 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_READ_RDY(v)           B_CLEAR32(v, 5 , 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_CARD(v)               B_CLEAR32(v, 8 , 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_RETUNE(v)             B_CLEAR32(v, 12, 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_BOOTACK(v)            B_CLEAR32(v, 13, 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_ENDBOOT(v)            B_CLEAR32(v, 14, 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_ERR(v)                B_CLEAR32(v, 15, 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_CTO_ERR(v)            B_CLEAR32(v, 16, 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_CCRC_ERR(v)           B_CLEAR32(v, 17, 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_CEND_ERR(v)           B_CLEAR32(v, 18, 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_CBAD_ERR(v)           B_CLEAR32(v, 19, 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_DTO_ERR(v)            B_CLEAR32(v, 20, 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_DCRC_ERR(v)           B_CLEAR32(v, 21, 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_DEND_ERR(v)           B_CLEAR32(v, 22, 1 )
#define BCM2835_EMMC_IRPT_MASK_CLR_ACMD_ERR(v)           B_CLEAR32(v, 24, 1 )
#define BCM2835_EMMC_IRPT_MASK_MASK_CMD_DONE             BF_MAKE_MASK32(0, 1)
#define BCM2835_EMMC_IRPT_MASK_MASK_DATA_DONE            BF_MAKE_MASK32(1, 1)
#define BCM2835_EMMC_IRPT_MASK_MASK_BLOCK_GAP            BF_MAKE_MASK32(2, 1)
#define BCM2835_EMMC_IRPT_MASK_MASK_WRITE_RDY            BF_MAKE_MASK32(4, 1)
#define BCM2835_EMMC_IRPT_MASK_MASK_READ_RDY             BF_MAKE_MASK32(5, 1)
#define BCM2835_EMMC_IRPT_MASK_MASK_CARD                 BF_MAKE_MASK32(8, 1)
#define BCM2835_EMMC_IRPT_MASK_MASK_RETUNE               BF_MAKE_MASK32(12, 1)
#define BCM2835_EMMC_IRPT_MASK_MASK_BOOTACK              BF_MAKE_MASK32(13, 1)
#define BCM2835_EMMC_IRPT_MASK_MASK_ENDBOOT              BF_MAKE_MASK32(14, 1)
#define BCM2835_EMMC_IRPT_MASK_MASK_ERR                  BF_MAKE_MASK32(15, 1)
#define BCM2835_EMMC_IRPT_MASK_MASK_CTO_ERR              BF_MAKE_MASK32(16, 1)
#define BCM2835_EMMC_IRPT_MASK_MASK_CCRC_ERR             BF_MAKE_MASK32(17, 1)
#define BCM2835_EMMC_IRPT_MASK_MASK_CEND_ERR             BF_MAKE_MASK32(18, 1)
#define BCM2835_EMMC_IRPT_MASK_MASK_CBAD_ERR             BF_MAKE_MASK32(19, 1)
#define BCM2835_EMMC_IRPT_MASK_MASK_DTO_ERR              BF_MAKE_MASK32(20, 1)
#define BCM2835_EMMC_IRPT_MASK_MASK_DCRC_ERR             BF_MAKE_MASK32(21, 1)
#define BCM2835_EMMC_IRPT_MASK_MASK_DEND_ERR             BF_MAKE_MASK32(22, 1)
#define BCM2835_EMMC_IRPT_MASK_MASK_ACMD_ERR             BF_MAKE_MASK32(24, 1)
#define BCM2835_EMMC_IRPT_MASK_SHIFT_CMD_DONE            0
#define BCM2835_EMMC_IRPT_MASK_SHIFT_DATA_DONE           1
#define BCM2835_EMMC_IRPT_MASK_SHIFT_BLOCK_GAP           2
#define BCM2835_EMMC_IRPT_MASK_SHIFT_WRITE_RDY           4
#define BCM2835_EMMC_IRPT_MASK_SHIFT_READ_RDY            5
#define BCM2835_EMMC_IRPT_MASK_SHIFT_CARD                8
#define BCM2835_EMMC_IRPT_MASK_SHIFT_RETUNE              12
#define BCM2835_EMMC_IRPT_MASK_SHIFT_BOOTACK             13
#define BCM2835_EMMC_IRPT_MASK_SHIFT_ENDBOOT             14
#define BCM2835_EMMC_IRPT_MASK_SHIFT_ERR                 15
#define BCM2835_EMMC_IRPT_MASK_SHIFT_CTO_ERR             16
#define BCM2835_EMMC_IRPT_MASK_SHIFT_CCRC_ERR            17
#define BCM2835_EMMC_IRPT_MASK_SHIFT_CEND_ERR            18
#define BCM2835_EMMC_IRPT_MASK_SHIFT_CBAD_ERR            19
#define BCM2835_EMMC_IRPT_MASK_SHIFT_DTO_ERR             20
#define BCM2835_EMMC_IRPT_MASK_SHIFT_DCRC_ERR            21
#define BCM2835_EMMC_IRPT_MASK_SHIFT_DEND_ERR            22
#define BCM2835_EMMC_IRPT_MASK_SHIFT_ACMD_ERR            24

static inline int bcm2835_emmc_irpt_mask_bitmask_to_string(char *buf, int bufsz, uint32_t v)
{
  int n = 0;
  int first = 1;
  if (BCM2835_EMMC_IRPT_MASK_GET_CMD_DONE(v)) {
    n += snprintf(buf + n, bufsz - n, "%sCMD_DONE", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_MASK_GET_DATA_DONE(v)) {
    n += snprintf(buf + n, bufsz - n, "%sDATA_DONE", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_MASK_GET_BLOCK_GAP(v)) {
    n += snprintf(buf + n, bufsz - n, "%sBLOCK_GAP", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_MASK_GET_WRITE_RDY(v)) {
    n += snprintf(buf + n, bufsz - n, "%sWRITE_RDY", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_MASK_GET_READ_RDY(v)) {
    n += snprintf(buf + n, bufsz - n, "%sREAD_RDY", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_MASK_GET_CARD(v)) {
    n += snprintf(buf + n, bufsz - n, "%sCARD", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_MASK_GET_RETUNE(v)) {
    n += snprintf(buf + n, bufsz - n, "%sRETUNE", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_MASK_GET_BOOTACK(v)) {
    n += snprintf(buf + n, bufsz - n, "%sBOOTACK", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_MASK_GET_ENDBOOT(v)) {
    n += snprintf(buf + n, bufsz - n, "%sENDBOOT", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_MASK_GET_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sERR", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_MASK_GET_CTO_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sCTO_ERR", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_MASK_GET_CCRC_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sCCRC_ERR", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_MASK_GET_CEND_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sCEND_ERR", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_MASK_GET_CBAD_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sCBAD_ERR", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_MASK_GET_DTO_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sDTO_ERR", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_MASK_GET_DCRC_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sDCRC_ERR", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_MASK_GET_DEND_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sDEND_ERR", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_MASK_GET_ACMD_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sACMD_ERR", first ? "" : ",");
    first = 0;
  }
  return n;
}


static inline int bcm2835_emmc_irpt_mask_to_string(char *buf, int bufsz, uint32_t v)
{
  return snprintf(buf, bufsz, "%08x,CMD_DONE:%x,DATA_DONE:%x,BLOCK_GAP:%x,WRITE_RDY:%x,READ_RDY:%x,CARD:%x,RETUNE:%x,BOOTACK:%x,ENDBOOT:%x,ERR:%x,CTO_ERR:%x,CCRC_ERR:%x,CEND_ERR:%x,CBAD_ERR:%x,DTO_ERR:%x,DCRC_ERR:%x,DEND_ERR:%x,ACMD_ERR:%x",
    v,
    (int)BCM2835_EMMC_IRPT_MASK_GET_CMD_DONE(v),
    (int)BCM2835_EMMC_IRPT_MASK_GET_DATA_DONE(v),
    (int)BCM2835_EMMC_IRPT_MASK_GET_BLOCK_GAP(v),
    (int)BCM2835_EMMC_IRPT_MASK_GET_WRITE_RDY(v),
    (int)BCM2835_EMMC_IRPT_MASK_GET_READ_RDY(v),
    (int)BCM2835_EMMC_IRPT_MASK_GET_CARD(v),
    (int)BCM2835_EMMC_IRPT_MASK_GET_RETUNE(v),
    (int)BCM2835_EMMC_IRPT_MASK_GET_BOOTACK(v),
    (int)BCM2835_EMMC_IRPT_MASK_GET_ENDBOOT(v),
    (int)BCM2835_EMMC_IRPT_MASK_GET_ERR(v),
    (int)BCM2835_EMMC_IRPT_MASK_GET_CTO_ERR(v),
    (int)BCM2835_EMMC_IRPT_MASK_GET_CCRC_ERR(v),
    (int)BCM2835_EMMC_IRPT_MASK_GET_CEND_ERR(v),
    (int)BCM2835_EMMC_IRPT_MASK_GET_CBAD_ERR(v),
    (int)BCM2835_EMMC_IRPT_MASK_GET_DTO_ERR(v),
    (int)BCM2835_EMMC_IRPT_MASK_GET_DCRC_ERR(v),
    (int)BCM2835_EMMC_IRPT_MASK_GET_DEND_ERR(v),
    (int)BCM2835_EMMC_IRPT_MASK_GET_ACMD_ERR(v));
}
#define BCM2835_EMMC_IRPT_EN_GET_CMD_DONE(v)             BITS_EXTRACT32(v, 0 , 1 )
#define BCM2835_EMMC_IRPT_EN_GET_DATA_DONE(v)            BITS_EXTRACT32(v, 1 , 1 )
#define BCM2835_EMMC_IRPT_EN_GET_BLOCK_GAP(v)            BITS_EXTRACT32(v, 2 , 1 )
#define BCM2835_EMMC_IRPT_EN_GET_WRITE_RDY(v)            BITS_EXTRACT32(v, 4 , 1 )
#define BCM2835_EMMC_IRPT_EN_GET_READ_RDY(v)             BITS_EXTRACT32(v, 5 , 1 )
#define BCM2835_EMMC_IRPT_EN_GET_CARD(v)                 BITS_EXTRACT32(v, 8 , 1 )
#define BCM2835_EMMC_IRPT_EN_GET_RETUNE(v)               BITS_EXTRACT32(v, 12, 1 )
#define BCM2835_EMMC_IRPT_EN_GET_BOOTACK(v)              BITS_EXTRACT32(v, 13, 1 )
#define BCM2835_EMMC_IRPT_EN_GET_ENDBOOT(v)              BITS_EXTRACT32(v, 14, 1 )
#define BCM2835_EMMC_IRPT_EN_GET_ERR(v)                  BITS_EXTRACT32(v, 15, 1 )
#define BCM2835_EMMC_IRPT_EN_GET_CTO_ERR(v)              BITS_EXTRACT32(v, 16, 1 )
#define BCM2835_EMMC_IRPT_EN_GET_CCRC_ERR(v)             BITS_EXTRACT32(v, 17, 1 )
#define BCM2835_EMMC_IRPT_EN_GET_CEND_ERR(v)             BITS_EXTRACT32(v, 18, 1 )
#define BCM2835_EMMC_IRPT_EN_GET_CBAD_ERR(v)             BITS_EXTRACT32(v, 19, 1 )
#define BCM2835_EMMC_IRPT_EN_GET_DTO_ERR(v)              BITS_EXTRACT32(v, 20, 1 )
#define BCM2835_EMMC_IRPT_EN_GET_DCRC_ERR(v)             BITS_EXTRACT32(v, 21, 1 )
#define BCM2835_EMMC_IRPT_EN_GET_DEND_ERR(v)             BITS_EXTRACT32(v, 22, 1 )
#define BCM2835_EMMC_IRPT_EN_GET_ACMD_ERR(v)             BITS_EXTRACT32(v, 24, 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_SET_CMD_DONE(v, set)            B_CLEAR_AND_SET32(v, set, 0 , 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_SET_DATA_DONE(v, set)           B_CLEAR_AND_SET32(v, set, 1 , 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_SET_BLOCK_GAP(v, set)           B_CLEAR_AND_SET32(v, set, 2 , 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_SET_WRITE_RDY(v, set)           B_CLEAR_AND_SET32(v, set, 4 , 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_SET_READ_RDY(v, set)            B_CLEAR_AND_SET32(v, set, 5 , 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_SET_CARD(v, set)                B_CLEAR_AND_SET32(v, set, 8 , 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_SET_RETUNE(v, set)              B_CLEAR_AND_SET32(v, set, 12, 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_SET_BOOTACK(v, set)             B_CLEAR_AND_SET32(v, set, 13, 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_SET_ENDBOOT(v, set)             B_CLEAR_AND_SET32(v, set, 14, 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_SET_ERR(v, set)                 B_CLEAR_AND_SET32(v, set, 15, 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_SET_CTO_ERR(v, set)             B_CLEAR_AND_SET32(v, set, 16, 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_SET_CCRC_ERR(v, set)            B_CLEAR_AND_SET32(v, set, 17, 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_SET_CEND_ERR(v, set)            B_CLEAR_AND_SET32(v, set, 18, 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_SET_CBAD_ERR(v, set)            B_CLEAR_AND_SET32(v, set, 19, 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_SET_DTO_ERR(v, set)             B_CLEAR_AND_SET32(v, set, 20, 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_SET_DCRC_ERR(v, set)            B_CLEAR_AND_SET32(v, set, 21, 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_SET_DEND_ERR(v, set)            B_CLEAR_AND_SET32(v, set, 22, 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_SET_ACMD_ERR(v, set)            B_CLEAR_AND_SET32(v, set, 24, 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_CMD_DONE(v)             B_CLEAR32(v, 0 , 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_DATA_DONE(v)            B_CLEAR32(v, 1 , 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_BLOCK_GAP(v)            B_CLEAR32(v, 2 , 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_DMA_DONE(v)             B_CLEAR32(v, 3 , 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_WRITE_RDY(v)            B_CLEAR32(v, 4 , 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_READ_RDY(v)             B_CLEAR32(v, 5 , 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_CARD(v)                 B_CLEAR32(v, 8 , 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_RETUNE(v)               B_CLEAR32(v, 12, 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_BOOTACK(v)              B_CLEAR32(v, 13, 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_ENDBOOT(v)              B_CLEAR32(v, 14, 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_ERR(v)                  B_CLEAR32(v, 15, 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_CTO_ERR(v)              B_CLEAR32(v, 16, 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_CCRC_ERR(v)             B_CLEAR32(v, 17, 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_CEND_ERR(v)             B_CLEAR32(v, 18, 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_CBAD_ERR(v)             B_CLEAR32(v, 19, 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_DTO_ERR(v)              B_CLEAR32(v, 20, 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_DCRC_ERR(v)             B_CLEAR32(v, 21, 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_DEND_ERR(v)             B_CLEAR32(v, 22, 1 )
#define BCM2835_EMMC_IRPT_EN_CLR_ACMD_ERR(v)             B_CLEAR32(v, 24, 1 )
#define BCM2835_EMMC_IRPT_EN_MASK_CMD_DONE               BF_MAKE_MASK32(0, 1)
#define BCM2835_EMMC_IRPT_EN_MASK_DATA_DONE              BF_MAKE_MASK32(1, 1)
#define BCM2835_EMMC_IRPT_EN_MASK_BLOCK_GAP              BF_MAKE_MASK32(2, 1)
#define BCM2835_EMMC_IRPT_EN_MASK_WRITE_RDY              BF_MAKE_MASK32(4, 1)
#define BCM2835_EMMC_IRPT_EN_MASK_READ_RDY               BF_MAKE_MASK32(5, 1)
#define BCM2835_EMMC_IRPT_EN_MASK_CARD                   BF_MAKE_MASK32(8, 1)
#define BCM2835_EMMC_IRPT_EN_MASK_RETUNE                 BF_MAKE_MASK32(12, 1)
#define BCM2835_EMMC_IRPT_EN_MASK_BOOTACK                BF_MAKE_MASK32(13, 1)
#define BCM2835_EMMC_IRPT_EN_MASK_ENDBOOT                BF_MAKE_MASK32(14, 1)
#define BCM2835_EMMC_IRPT_EN_MASK_ERR                    BF_MAKE_MASK32(15, 1)
#define BCM2835_EMMC_IRPT_EN_MASK_CTO_ERR                BF_MAKE_MASK32(16, 1)
#define BCM2835_EMMC_IRPT_EN_MASK_CCRC_ERR               BF_MAKE_MASK32(17, 1)
#define BCM2835_EMMC_IRPT_EN_MASK_CEND_ERR               BF_MAKE_MASK32(18, 1)
#define BCM2835_EMMC_IRPT_EN_MASK_CBAD_ERR               BF_MAKE_MASK32(19, 1)
#define BCM2835_EMMC_IRPT_EN_MASK_DTO_ERR                BF_MAKE_MASK32(20, 1)
#define BCM2835_EMMC_IRPT_EN_MASK_DCRC_ERR               BF_MAKE_MASK32(21, 1)
#define BCM2835_EMMC_IRPT_EN_MASK_DEND_ERR               BF_MAKE_MASK32(22, 1)
#define BCM2835_EMMC_IRPT_EN_MASK_ACMD_ERR               BF_MAKE_MASK32(24, 1)
#define BCM2835_EMMC_IRPT_EN_SHIFT_CMD_DONE              0
#define BCM2835_EMMC_IRPT_EN_SHIFT_DATA_DONE             1
#define BCM2835_EMMC_IRPT_EN_SHIFT_BLOCK_GAP             2
#define BCM2835_EMMC_IRPT_EN_SHIFT_WRITE_RDY             4
#define BCM2835_EMMC_IRPT_EN_SHIFT_READ_RDY              5
#define BCM2835_EMMC_IRPT_EN_SHIFT_CARD                  8
#define BCM2835_EMMC_IRPT_EN_SHIFT_RETUNE                12
#define BCM2835_EMMC_IRPT_EN_SHIFT_BOOTACK               13
#define BCM2835_EMMC_IRPT_EN_SHIFT_ENDBOOT               14
#define BCM2835_EMMC_IRPT_EN_SHIFT_ERR                   15
#define BCM2835_EMMC_IRPT_EN_SHIFT_CTO_ERR               16
#define BCM2835_EMMC_IRPT_EN_SHIFT_CCRC_ERR              17
#define BCM2835_EMMC_IRPT_EN_SHIFT_CEND_ERR              18
#define BCM2835_EMMC_IRPT_EN_SHIFT_CBAD_ERR              19
#define BCM2835_EMMC_IRPT_EN_SHIFT_DTO_ERR               20
#define BCM2835_EMMC_IRPT_EN_SHIFT_DCRC_ERR              21
#define BCM2835_EMMC_IRPT_EN_SHIFT_DEND_ERR              22
#define BCM2835_EMMC_IRPT_EN_SHIFT_ACMD_ERR              24

static inline int bcm2835_emmc_irpt_en_bitmask_to_string(char *buf, int bufsz, uint32_t v)
{
  int n = 0;
  int first = 1;
  if (BCM2835_EMMC_IRPT_EN_GET_CMD_DONE(v)) {
    n += snprintf(buf + n, bufsz - n, "%sCMD_DONE", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_EN_GET_DATA_DONE(v)) {
    n += snprintf(buf + n, bufsz - n, "%sDATA_DONE", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_EN_GET_BLOCK_GAP(v)) {
    n += snprintf(buf + n, bufsz - n, "%sBLOCK_GAP", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_EN_GET_WRITE_RDY(v)) {
    n += snprintf(buf + n, bufsz - n, "%sWRITE_RDY", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_EN_GET_READ_RDY(v)) {
    n += snprintf(buf + n, bufsz - n, "%sREAD_RDY", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_EN_GET_CARD(v)) {
    n += snprintf(buf + n, bufsz - n, "%sCARD", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_EN_GET_RETUNE(v)) {
    n += snprintf(buf + n, bufsz - n, "%sRETUNE", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_EN_GET_BOOTACK(v)) {
    n += snprintf(buf + n, bufsz - n, "%sBOOTACK", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_EN_GET_ENDBOOT(v)) {
    n += snprintf(buf + n, bufsz - n, "%sENDBOOT", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_EN_GET_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sERR", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_EN_GET_CTO_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sCTO_ERR", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_EN_GET_CCRC_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sCCRC_ERR", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_EN_GET_CEND_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sCEND_ERR", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_EN_GET_CBAD_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sCBAD_ERR", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_EN_GET_DTO_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sDTO_ERR", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_EN_GET_DCRC_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sDCRC_ERR", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_EN_GET_DEND_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sDEND_ERR", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_IRPT_EN_GET_ACMD_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sACMD_ERR", first ? "" : ",");
    first = 0;
  }
  return n;
}


static inline int bcm2835_emmc_irpt_en_to_string(char *buf, int bufsz, uint32_t v)
{
  return snprintf(buf, bufsz, "%08x,CMD_DONE:%x,DATA_DONE:%x,BLOCK_GAP:%x,WRITE_RDY:%x,READ_RDY:%x,CARD:%x,RETUNE:%x,BOOTACK:%x,ENDBOOT:%x,ERR:%x,CTO_ERR:%x,CCRC_ERR:%x,CEND_ERR:%x,CBAD_ERR:%x,DTO_ERR:%x,DCRC_ERR:%x,DEND_ERR:%x,ACMD_ERR:%x",
    v,
    (int)BCM2835_EMMC_IRPT_EN_GET_CMD_DONE(v),
    (int)BCM2835_EMMC_IRPT_EN_GET_DATA_DONE(v),
    (int)BCM2835_EMMC_IRPT_EN_GET_BLOCK_GAP(v),
    (int)BCM2835_EMMC_IRPT_EN_GET_WRITE_RDY(v),
    (int)BCM2835_EMMC_IRPT_EN_GET_READ_RDY(v),
    (int)BCM2835_EMMC_IRPT_EN_GET_CARD(v),
    (int)BCM2835_EMMC_IRPT_EN_GET_RETUNE(v),
    (int)BCM2835_EMMC_IRPT_EN_GET_BOOTACK(v),
    (int)BCM2835_EMMC_IRPT_EN_GET_ENDBOOT(v),
    (int)BCM2835_EMMC_IRPT_EN_GET_ERR(v),
    (int)BCM2835_EMMC_IRPT_EN_GET_CTO_ERR(v),
    (int)BCM2835_EMMC_IRPT_EN_GET_CCRC_ERR(v),
    (int)BCM2835_EMMC_IRPT_EN_GET_CEND_ERR(v),
    (int)BCM2835_EMMC_IRPT_EN_GET_CBAD_ERR(v),
    (int)BCM2835_EMMC_IRPT_EN_GET_DTO_ERR(v),
    (int)BCM2835_EMMC_IRPT_EN_GET_DCRC_ERR(v),
    (int)BCM2835_EMMC_IRPT_EN_GET_DEND_ERR(v),
    (int)BCM2835_EMMC_IRPT_EN_GET_ACMD_ERR(v));
}
#define BCM2835_EMMC_FORCE_IRPT_GET_CMD_DONE(v)          BITS_EXTRACT32(v, 0 , 1 )
#define BCM2835_EMMC_FORCE_IRPT_GET_DATA_DONE(v)         BITS_EXTRACT32(v, 1 , 1 )
#define BCM2835_EMMC_FORCE_IRPT_GET_BLOCK_GAP(v)         BITS_EXTRACT32(v, 2 , 1 )
#define BCM2835_EMMC_FORCE_IRPT_GET_WRITE_RDY(v)         BITS_EXTRACT32(v, 4 , 1 )
#define BCM2835_EMMC_FORCE_IRPT_GET_READ_RDY(v)          BITS_EXTRACT32(v, 5 , 1 )
#define BCM2835_EMMC_FORCE_IRPT_GET_CARD(v)              BITS_EXTRACT32(v, 8 , 1 )
#define BCM2835_EMMC_FORCE_IRPT_GET_RETUNE(v)            BITS_EXTRACT32(v, 12, 1 )
#define BCM2835_EMMC_FORCE_IRPT_GET_BOOTACK(v)           BITS_EXTRACT32(v, 13, 1 )
#define BCM2835_EMMC_FORCE_IRPT_GET_ENDBOOT(v)           BITS_EXTRACT32(v, 14, 1 )
#define BCM2835_EMMC_FORCE_IRPT_GET_ERR(v)               BITS_EXTRACT32(v, 15, 1 )
#define BCM2835_EMMC_FORCE_IRPT_GET_CTO_ERR(v)           BITS_EXTRACT32(v, 16, 1 )
#define BCM2835_EMMC_FORCE_IRPT_GET_CCRC_ERR(v)          BITS_EXTRACT32(v, 17, 1 )
#define BCM2835_EMMC_FORCE_IRPT_GET_CEND_ERR(v)          BITS_EXTRACT32(v, 18, 1 )
#define BCM2835_EMMC_FORCE_IRPT_GET_CBAD_ERR(v)          BITS_EXTRACT32(v, 19, 1 )
#define BCM2835_EMMC_FORCE_IRPT_GET_DTO_ERR(v)           BITS_EXTRACT32(v, 20, 1 )
#define BCM2835_EMMC_FORCE_IRPT_GET_DCRC_ERR(v)          BITS_EXTRACT32(v, 21, 1 )
#define BCM2835_EMMC_FORCE_IRPT_GET_DEND_ERR(v)          BITS_EXTRACT32(v, 22, 1 )
#define BCM2835_EMMC_FORCE_IRPT_GET_ACMD_ERR(v)          BITS_EXTRACT32(v, 24, 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_SET_CMD_DONE(v, set)         B_CLEAR_AND_SET32(v, set, 0 , 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_SET_DATA_DONE(v, set)        B_CLEAR_AND_SET32(v, set, 1 , 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_SET_BLOCK_GAP(v, set)        B_CLEAR_AND_SET32(v, set, 2 , 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_SET_WRITE_RDY(v, set)        B_CLEAR_AND_SET32(v, set, 4 , 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_SET_READ_RDY(v, set)         B_CLEAR_AND_SET32(v, set, 5 , 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_SET_CARD(v, set)             B_CLEAR_AND_SET32(v, set, 8 , 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_SET_RETUNE(v, set)           B_CLEAR_AND_SET32(v, set, 12, 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_SET_BOOTACK(v, set)          B_CLEAR_AND_SET32(v, set, 13, 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_SET_ENDBOOT(v, set)          B_CLEAR_AND_SET32(v, set, 14, 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_SET_ERR(v, set)              B_CLEAR_AND_SET32(v, set, 15, 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_SET_CTO_ERR(v, set)          B_CLEAR_AND_SET32(v, set, 16, 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_SET_CCRC_ERR(v, set)         B_CLEAR_AND_SET32(v, set, 17, 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_SET_CEND_ERR(v, set)         B_CLEAR_AND_SET32(v, set, 18, 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_SET_CBAD_ERR(v, set)         B_CLEAR_AND_SET32(v, set, 19, 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_SET_DTO_ERR(v, set)          B_CLEAR_AND_SET32(v, set, 20, 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_SET_DCRC_ERR(v, set)         B_CLEAR_AND_SET32(v, set, 21, 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_SET_DEND_ERR(v, set)         B_CLEAR_AND_SET32(v, set, 22, 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_SET_ACMD_ERR(v, set)         B_CLEAR_AND_SET32(v, set, 24, 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_CMD_DONE(v)          B_CLEAR32(v, 0 , 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_DATA_DONE(v)         B_CLEAR32(v, 1 , 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_BLOCK_GAP(v)         B_CLEAR32(v, 2 , 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_WRITE_RDY(v)         B_CLEAR32(v, 4 , 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_READ_RDY(v)          B_CLEAR32(v, 5 , 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_CARD(v)              B_CLEAR32(v, 8 , 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_RETUNE(v)            B_CLEAR32(v, 12, 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_BOOTACK(v)           B_CLEAR32(v, 13, 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_ENDBOOT(v)           B_CLEAR32(v, 14, 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_ERR(v)               B_CLEAR32(v, 15, 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_CTO_ERR(v)           B_CLEAR32(v, 16, 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_CCRC_ERR(v)          B_CLEAR32(v, 17, 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_CEND_ERR(v)          B_CLEAR32(v, 18, 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_CBAD_ERR(v)          B_CLEAR32(v, 19, 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_DTO_ERR(v)           B_CLEAR32(v, 20, 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_DCRC_ERR(v)          B_CLEAR32(v, 21, 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_DEND_ERR(v)          B_CLEAR32(v, 22, 1 )
#define BCM2835_EMMC_FORCE_IRPT_CLR_ACMD_ERR(v)          B_CLEAR32(v, 24, 1 )
#define BCM2835_EMMC_FORCE_IRPT_MASK_CMD_DONE            BF_MAKE_MASK32(0, 1)
#define BCM2835_EMMC_FORCE_IRPT_MASK_DATA_DONE           BF_MAKE_MASK32(1, 1)
#define BCM2835_EMMC_FORCE_IRPT_MASK_BLOCK_GAP           BF_MAKE_MASK32(2, 1)
#define BCM2835_EMMC_FORCE_IRPT_MASK_WRITE_RDY           BF_MAKE_MASK32(4, 1)
#define BCM2835_EMMC_FORCE_IRPT_MASK_READ_RDY            BF_MAKE_MASK32(5, 1)
#define BCM2835_EMMC_FORCE_IRPT_MASK_CARD                BF_MAKE_MASK32(8, 1)
#define BCM2835_EMMC_FORCE_IRPT_MASK_RETUNE              BF_MAKE_MASK32(12, 1)
#define BCM2835_EMMC_FORCE_IRPT_MASK_BOOTACK             BF_MAKE_MASK32(13, 1)
#define BCM2835_EMMC_FORCE_IRPT_MASK_ENDBOOT             BF_MAKE_MASK32(14, 1)
#define BCM2835_EMMC_FORCE_IRPT_MASK_ERR                 BF_MAKE_MASK32(15, 1)
#define BCM2835_EMMC_FORCE_IRPT_MASK_CTO_ERR             BF_MAKE_MASK32(16, 1)
#define BCM2835_EMMC_FORCE_IRPT_MASK_CCRC_ERR            BF_MAKE_MASK32(17, 1)
#define BCM2835_EMMC_FORCE_IRPT_MASK_CEND_ERR            BF_MAKE_MASK32(18, 1)
#define BCM2835_EMMC_FORCE_IRPT_MASK_CBAD_ERR            BF_MAKE_MASK32(19, 1)
#define BCM2835_EMMC_FORCE_IRPT_MASK_DTO_ERR             BF_MAKE_MASK32(20, 1)
#define BCM2835_EMMC_FORCE_IRPT_MASK_DCRC_ERR            BF_MAKE_MASK32(21, 1)
#define BCM2835_EMMC_FORCE_IRPT_MASK_DEND_ERR            BF_MAKE_MASK32(22, 1)
#define BCM2835_EMMC_FORCE_IRPT_MASK_ACMD_ERR            BF_MAKE_MASK32(24, 1)
#define BCM2835_EMMC_FORCE_IRPT_SHIFT_CMD_DONE           0
#define BCM2835_EMMC_FORCE_IRPT_SHIFT_DATA_DONE          1
#define BCM2835_EMMC_FORCE_IRPT_SHIFT_BLOCK_GAP          2
#define BCM2835_EMMC_FORCE_IRPT_SHIFT_WRITE_RDY          4
#define BCM2835_EMMC_FORCE_IRPT_SHIFT_READ_RDY           5
#define BCM2835_EMMC_FORCE_IRPT_SHIFT_CARD               8
#define BCM2835_EMMC_FORCE_IRPT_SHIFT_RETUNE             12
#define BCM2835_EMMC_FORCE_IRPT_SHIFT_BOOTACK            13
#define BCM2835_EMMC_FORCE_IRPT_SHIFT_ENDBOOT            14
#define BCM2835_EMMC_FORCE_IRPT_SHIFT_ERR                15
#define BCM2835_EMMC_FORCE_IRPT_SHIFT_CTO_ERR            16
#define BCM2835_EMMC_FORCE_IRPT_SHIFT_CCRC_ERR           17
#define BCM2835_EMMC_FORCE_IRPT_SHIFT_CEND_ERR           18
#define BCM2835_EMMC_FORCE_IRPT_SHIFT_CBAD_ERR           19
#define BCM2835_EMMC_FORCE_IRPT_SHIFT_DTO_ERR            20
#define BCM2835_EMMC_FORCE_IRPT_SHIFT_DCRC_ERR           21
#define BCM2835_EMMC_FORCE_IRPT_SHIFT_DEND_ERR           22
#define BCM2835_EMMC_FORCE_IRPT_SHIFT_ACMD_ERR           24

static inline int bcm2835_emmc_force_irpt_bitmask_to_string(char *buf, int bufsz, uint32_t v)
{
  int n = 0;
  int first = 1;
  if (BCM2835_EMMC_FORCE_IRPT_GET_CMD_DONE(v)) {
    n += snprintf(buf + n, bufsz - n, "%sCMD_DONE", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_FORCE_IRPT_GET_DATA_DONE(v)) {
    n += snprintf(buf + n, bufsz - n, "%sDATA_DONE", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_FORCE_IRPT_GET_BLOCK_GAP(v)) {
    n += snprintf(buf + n, bufsz - n, "%sBLOCK_GAP", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_FORCE_IRPT_GET_WRITE_RDY(v)) {
    n += snprintf(buf + n, bufsz - n, "%sWRITE_RDY", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_FORCE_IRPT_GET_READ_RDY(v)) {
    n += snprintf(buf + n, bufsz - n, "%sREAD_RDY", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_FORCE_IRPT_GET_CARD(v)) {
    n += snprintf(buf + n, bufsz - n, "%sCARD", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_FORCE_IRPT_GET_RETUNE(v)) {
    n += snprintf(buf + n, bufsz - n, "%sRETUNE", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_FORCE_IRPT_GET_BOOTACK(v)) {
    n += snprintf(buf + n, bufsz - n, "%sBOOTACK", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_FORCE_IRPT_GET_ENDBOOT(v)) {
    n += snprintf(buf + n, bufsz - n, "%sENDBOOT", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_FORCE_IRPT_GET_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sERR", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_FORCE_IRPT_GET_CTO_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sCTO_ERR", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_FORCE_IRPT_GET_CCRC_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sCCRC_ERR", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_FORCE_IRPT_GET_CEND_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sCEND_ERR", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_FORCE_IRPT_GET_CBAD_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sCBAD_ERR", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_FORCE_IRPT_GET_DTO_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sDTO_ERR", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_FORCE_IRPT_GET_DCRC_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sDCRC_ERR", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_FORCE_IRPT_GET_DEND_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sDEND_ERR", first ? "" : ",");
    first = 0;
  }
  if (BCM2835_EMMC_FORCE_IRPT_GET_ACMD_ERR(v)) {
    n += snprintf(buf + n, bufsz - n, "%sACMD_ERR", first ? "" : ",");
    first = 0;
  }
  return n;
}


static inline int bcm2835_emmc_force_irpt_to_string(char *buf, int bufsz, uint32_t v)
{
  return snprintf(buf, bufsz, "%08x,CMD_DONE:%x,DATA_DONE:%x,BLOCK_GAP:%x,WRITE_RDY:%x,READ_RDY:%x,CARD:%x,RETUNE:%x,BOOTACK:%x,ENDBOOT:%x,ERR:%x,CTO_ERR:%x,CCRC_ERR:%x,CEND_ERR:%x,CBAD_ERR:%x,DTO_ERR:%x,DCRC_ERR:%x,DEND_ERR:%x,ACMD_ERR:%x",
    v,
    (int)BCM2835_EMMC_FORCE_IRPT_GET_CMD_DONE(v),
    (int)BCM2835_EMMC_FORCE_IRPT_GET_DATA_DONE(v),
    (int)BCM2835_EMMC_FORCE_IRPT_GET_BLOCK_GAP(v),
    (int)BCM2835_EMMC_FORCE_IRPT_GET_WRITE_RDY(v),
    (int)BCM2835_EMMC_FORCE_IRPT_GET_READ_RDY(v),
    (int)BCM2835_EMMC_FORCE_IRPT_GET_CARD(v),
    (int)BCM2835_EMMC_FORCE_IRPT_GET_RETUNE(v),
    (int)BCM2835_EMMC_FORCE_IRPT_GET_BOOTACK(v),
    (int)BCM2835_EMMC_FORCE_IRPT_GET_ENDBOOT(v),
    (int)BCM2835_EMMC_FORCE_IRPT_GET_ERR(v),
    (int)BCM2835_EMMC_FORCE_IRPT_GET_CTO_ERR(v),
    (int)BCM2835_EMMC_FORCE_IRPT_GET_CCRC_ERR(v),
    (int)BCM2835_EMMC_FORCE_IRPT_GET_CEND_ERR(v),
    (int)BCM2835_EMMC_FORCE_IRPT_GET_CBAD_ERR(v),
    (int)BCM2835_EMMC_FORCE_IRPT_GET_DTO_ERR(v),
    (int)BCM2835_EMMC_FORCE_IRPT_GET_DCRC_ERR(v),
    (int)BCM2835_EMMC_FORCE_IRPT_GET_DEND_ERR(v),
    (int)BCM2835_EMMC_FORCE_IRPT_GET_ACMD_ERR(v));
}
#define BCM2835_EMMC_CONTROL2_GET_ACKNOX_ERR(v)          BITS_EXTRACT32(v, 0 , 1 )
#define BCM2835_EMMC_CONTROL2_GET_ACTO_ERR(v)            BITS_EXTRACT32(v, 1 , 1 )
#define BCM2835_EMMC_CONTROL2_GET_ACCRC_ERR(v)           BITS_EXTRACT32(v, 2 , 1 )
#define BCM2835_EMMC_CONTROL2_GET_ACEND_ERR(v)           BITS_EXTRACT32(v, 3 , 1 )
#define BCM2835_EMMC_CONTROL2_GET_ACBAD_ERR(v)           BITS_EXTRACT32(v, 4 , 1 )
#define BCM2835_EMMC_CONTROL2_GET_NOTC12_ERR(v)          BITS_EXTRACT32(v, 7 , 1 )
#define BCM2835_EMMC_CONTROL2_GET_UHSMODE(v)             BITS_EXTRACT32(v, 16, 3 )
#define BCM2835_EMMC_CONTROL2_GET_TUNEON(v)              BITS_EXTRACT32(v, 22, 1 )
#define BCM2835_EMMC_CONTROL2_GET_TUNED(v)               BITS_EXTRACT32(v, 23, 1 )
#define BCM2835_EMMC_CONTROL2_CLR_SET_ACKNOX_ERR(v, set)         B_CLEAR_AND_SET32(v, set, 0 , 1 )
#define BCM2835_EMMC_CONTROL2_CLR_SET_ACTO_ERR(v, set)           B_CLEAR_AND_SET32(v, set, 1 , 1 )
#define BCM2835_EMMC_CONTROL2_CLR_SET_ACCRC_ERR(v, set)          B_CLEAR_AND_SET32(v, set, 2 , 1 )
#define BCM2835_EMMC_CONTROL2_CLR_SET_ACEND_ERR(v, set)          B_CLEAR_AND_SET32(v, set, 3 , 1 )
#define BCM2835_EMMC_CONTROL2_CLR_SET_ACBAD_ERR(v, set)          B_CLEAR_AND_SET32(v, set, 4 , 1 )
#define BCM2835_EMMC_CONTROL2_CLR_SET_NOTC12_ERR(v, set)         B_CLEAR_AND_SET32(v, set, 7 , 1 )
#define BCM2835_EMMC_CONTROL2_CLR_SET_UHSMODE(v, set)            B_CLEAR_AND_SET32(v, set, 16, 3 )
#define BCM2835_EMMC_CONTROL2_CLR_SET_TUNEON(v, set)             B_CLEAR_AND_SET32(v, set, 22, 1 )
#define BCM2835_EMMC_CONTROL2_CLR_SET_TUNED(v, set)              B_CLEAR_AND_SET32(v, set, 23, 1 )
#define BCM2835_EMMC_CONTROL2_CLR_ACKNOX_ERR(v)          B_CLEAR32(v, 0 , 1 )
#define BCM2835_EMMC_CONTROL2_CLR_ACTO_ERR(v)            B_CLEAR32(v, 1 , 1 )
#define BCM2835_EMMC_CONTROL2_CLR_ACCRC_ERR(v)           B_CLEAR32(v, 2 , 1 )
#define BCM2835_EMMC_CONTROL2_CLR_ACEND_ERR(v)           B_CLEAR32(v, 3 , 1 )
#define BCM2835_EMMC_CONTROL2_CLR_ACBAD_ERR(v)           B_CLEAR32(v, 4 , 1 )
#define BCM2835_EMMC_CONTROL2_CLR_NOTC12_ERR(v)          B_CLEAR32(v, 7 , 1 )
#define BCM2835_EMMC_CONTROL2_CLR_UHSMODE(v)             B_CLEAR32(v, 16, 3 )
#define BCM2835_EMMC_CONTROL2_CLR_TUNEON(v)              B_CLEAR32(v, 22, 1 )
#define BCM2835_EMMC_CONTROL2_CLR_TUNED(v)               B_CLEAR32(v, 23, 1 )
#define BCM2835_EMMC_CONTROL2_MASK_ACKNOX_ERR            BF_MAKE_MASK32(0, 1)
#define BCM2835_EMMC_CONTROL2_MASK_ACTO_ERR              BF_MAKE_MASK32(1, 1)
#define BCM2835_EMMC_CONTROL2_MASK_ACCRC_ERR             BF_MAKE_MASK32(2, 1)
#define BCM2835_EMMC_CONTROL2_MASK_ACEND_ERR             BF_MAKE_MASK32(3, 1)
#define BCM2835_EMMC_CONTROL2_MASK_ACBAD_ERR             BF_MAKE_MASK32(4, 1)
#define BCM2835_EMMC_CONTROL2_MASK_NOTC12_ERR            BF_MAKE_MASK32(7, 1)
#define BCM2835_EMMC_CONTROL2_MASK_UHSMODE               BF_MAKE_MASK32(16, 3)
#define BCM2835_EMMC_CONTROL2_MASK_TUNEON                BF_MAKE_MASK32(22, 1)
#define BCM2835_EMMC_CONTROL2_MASK_TUNED                 BF_MAKE_MASK32(23, 1)
#define BCM2835_EMMC_CONTROL2_SHIFT_ACKNOX_ERR           0
#define BCM2835_EMMC_CONTROL2_SHIFT_ACTO_ERR             1
#define BCM2835_EMMC_CONTROL2_SHIFT_ACCRC_ERR            2
#define BCM2835_EMMC_CONTROL2_SHIFT_ACEND_ERR            3
#define BCM2835_EMMC_CONTROL2_SHIFT_ACBAD_ERR            4
#define BCM2835_EMMC_CONTROL2_SHIFT_NOTC12_ERR           7
#define BCM2835_EMMC_CONTROL2_SHIFT_UHSMODE              16
#define BCM2835_EMMC_CONTROL2_SHIFT_TUNEON               22
#define BCM2835_EMMC_CONTROL2_SHIFT_TUNED                23


static inline int bcm2835_emmc_control2_to_string(char *buf, int bufsz, uint32_t v)
{
  return snprintf(buf, bufsz, "%08x,ACKNOX_ERR:%x,ACTO_ERR:%x,ACCRC_ERR:%x,ACEND_ERR:%x,ACBAD_ERR:%x,NOTC12_ERR:%x,UHSMODE:%x,TUNEON:%x,TUNED:%x",
    v,
    (int)BCM2835_EMMC_CONTROL2_GET_ACKNOX_ERR(v),
    (int)BCM2835_EMMC_CONTROL2_GET_ACTO_ERR(v),
    (int)BCM2835_EMMC_CONTROL2_GET_ACCRC_ERR(v),
    (int)BCM2835_EMMC_CONTROL2_GET_ACEND_ERR(v),
    (int)BCM2835_EMMC_CONTROL2_GET_ACBAD_ERR(v),
    (int)BCM2835_EMMC_CONTROL2_GET_NOTC12_ERR(v),
    (int)BCM2835_EMMC_CONTROL2_GET_UHSMODE(v),
    (int)BCM2835_EMMC_CONTROL2_GET_TUNEON(v),
    (int)BCM2835_EMMC_CONTROL2_GET_TUNED(v));
}
#define BCM2835_EMMC_BOOT_TIMEOUT_GET_TIMEOUT(v)         BITS_EXTRACT32(v, 0 , 32)
#define BCM2835_EMMC_BOOT_TIMEOUT_CLR_SET_TIMEOUT(v, set)        B_CLEAR_AND_SET32(v, set, 0 , 32)
#define BCM2835_EMMC_BOOT_TIMEOUT_CLR_TIMEOUT(v)         B_CLEAR32(v, 0 , 32)
#define BCM2835_EMMC_BOOT_TIMEOUT_MASK_TIMEOUT           BF_MAKE_MASK32(0, 32)
#define BCM2835_EMMC_BOOT_TIMEOUT_SHIFT_TIMEOUT          0


static inline int bcm2835_emmc_boot_timeout_to_string(char *buf, int bufsz, uint32_t v)
{
  return snprintf(buf, bufsz, "%08x,TIMEOUT:%x",
    v,
    (int)BCM2835_EMMC_BOOT_TIMEOUT_GET_TIMEOUT(v));
}
#define BCM2835_EMMC_DBG_SEL_GET_SELECT(v)               BITS_EXTRACT32(v, 0 , 1 )
#define BCM2835_EMMC_DBG_SEL_CLR_SET_SELECT(v, set)              B_CLEAR_AND_SET32(v, set, 0 , 1 )
#define BCM2835_EMMC_DBG_SEL_CLR_SELECT(v)               B_CLEAR32(v, 0 , 1 )
#define BCM2835_EMMC_DBG_SEL_MASK_SELECT                 BF_MAKE_MASK32(0, 1)
#define BCM2835_EMMC_DBG_SEL_SHIFT_SELECT                0

static inline int bcm2835_emmc_dbg_sel_bitmask_to_string(char *buf, int bufsz, uint32_t v)
{
  int n = 0;
  int first = 1;
  if (BCM2835_EMMC_DBG_SEL_GET_SELECT(v)) {
    n += snprintf(buf + n, bufsz - n, "%sSELECT", first ? "" : ",");
    first = 0;
  }
  return n;
}


static inline int bcm2835_emmc_dbg_sel_to_string(char *buf, int bufsz, uint32_t v)
{
  return snprintf(buf, bufsz, "%08x,SELECT:%x",
    v,
    (int)BCM2835_EMMC_DBG_SEL_GET_SELECT(v));
}
#define BCM2835_EMMC_EXRDFIFO_CFG_GET_RD_THRSH(v)        BITS_EXTRACT32(v, 0 , 3 )
#define BCM2835_EMMC_EXRDFIFO_CFG_CLR_SET_RD_THRSH(v, set)       B_CLEAR_AND_SET32(v, set, 0 , 3 )
#define BCM2835_EMMC_EXRDFIFO_CFG_CLR_RD_THRSH(v)        B_CLEAR32(v, 0 , 3 )
#define BCM2835_EMMC_EXRDFIFO_CFG_MASK_RD_THRSH          BF_MAKE_MASK32(0, 3)
#define BCM2835_EMMC_EXRDFIFO_CFG_SHIFT_RD_THRSH         0


static inline int bcm2835_emmc_exrdfifo_cfg_to_string(char *buf, int bufsz, uint32_t v)
{
  return snprintf(buf, bufsz, "%08x,RD_THRSH:%x",
    v,
    (int)BCM2835_EMMC_EXRDFIFO_CFG_GET_RD_THRSH(v));
}
#define BCM2835_EMMC_EXRDFIFO_EN_GET_ENABLE(v)           BITS_EXTRACT32(v, 0 , 1 )
#define BCM2835_EMMC_EXRDFIFO_EN_CLR_SET_ENABLE(v, set)          B_CLEAR_AND_SET32(v, set, 0 , 1 )
#define BCM2835_EMMC_EXRDFIFO_EN_CLR_ENABLE(v)           B_CLEAR32(v, 0 , 1 )
#define BCM2835_EMMC_EXRDFIFO_EN_MASK_ENABLE             BF_MAKE_MASK32(0, 1)
#define BCM2835_EMMC_EXRDFIFO_EN_SHIFT_ENABLE            0

static inline int bcm2835_emmc_exrdfifo_en_bitmask_to_string(char *buf, int bufsz, uint32_t v)
{
  int n = 0;
  int first = 1;
  if (BCM2835_EMMC_EXRDFIFO_EN_GET_ENABLE(v)) {
    n += snprintf(buf + n, bufsz - n, "%sENABLE", first ? "" : ",");
    first = 0;
  }
  return n;
}


static inline int bcm2835_emmc_exrdfifo_en_to_string(char *buf, int bufsz, uint32_t v)
{
  return snprintf(buf, bufsz, "%08x,ENABLE:%x",
    v,
    (int)BCM2835_EMMC_EXRDFIFO_EN_GET_ENABLE(v));
}
#define BCM2835_EMMC_TUNE_STEP_GET_DELAY(v)              BITS_EXTRACT32(v, 0 , 3 )
#define BCM2835_EMMC_TUNE_STEP_CLR_SET_DELAY(v, set)             B_CLEAR_AND_SET32(v, set, 0 , 3 )
#define BCM2835_EMMC_TUNE_STEP_CLR_DELAY(v)              B_CLEAR32(v, 0 , 3 )
#define BCM2835_EMMC_TUNE_STEP_MASK_DELAY                BF_MAKE_MASK32(0, 3)
#define BCM2835_EMMC_TUNE_STEP_SHIFT_DELAY               0


static inline int bcm2835_emmc_tune_step_to_string(char *buf, int bufsz, uint32_t v)
{
  return snprintf(buf, bufsz, "%08x,DELAY:%x",
    v,
    (int)BCM2835_EMMC_TUNE_STEP_GET_DELAY(v));
}
#define BCM2835_EMMC_TUNE_STEPS_STD_GET_STEPS(v)         BITS_EXTRACT32(v, 0 , 6 )
#define BCM2835_EMMC_TUNE_STEPS_STD_CLR_SET_STEPS(v, set)        B_CLEAR_AND_SET32(v, set, 0 , 6 )
#define BCM2835_EMMC_TUNE_STEPS_STD_CLR_STEPS(v)         B_CLEAR32(v, 0 , 6 )
#define BCM2835_EMMC_TUNE_STEPS_STD_MASK_STEPS           BF_MAKE_MASK32(0, 6)
#define BCM2835_EMMC_TUNE_STEPS_STD_SHIFT_STEPS          0


static inline int bcm2835_emmc_tune_steps_std_to_string(char *buf, int bufsz, uint32_t v)
{
  return snprintf(buf, bufsz, "%08x,STEPS:%x",
    v,
    (int)BCM2835_EMMC_TUNE_STEPS_STD_GET_STEPS(v));
}
#define BCM2835_EMMC_TUNE_STEPS_DDR_GET_STEPS(v)         BITS_EXTRACT32(v, 0 , 6 )
#define BCM2835_EMMC_TUNE_STEPS_DDR_CLR_SET_STEPS(v, set)        B_CLEAR_AND_SET32(v, set, 0 , 6 )
#define BCM2835_EMMC_TUNE_STEPS_DDR_CLR_STEPS(v)         B_CLEAR32(v, 0 , 6 )
#define BCM2835_EMMC_TUNE_STEPS_DDR_MASK_STEPS           BF_MAKE_MASK32(0, 6)
#define BCM2835_EMMC_TUNE_STEPS_DDR_SHIFT_STEPS          0


static inline int bcm2835_emmc_tune_steps_ddr_to_string(char *buf, int bufsz, uint32_t v)
{
  return snprintf(buf, bufsz, "%08x,STEPS:%x",
    v,
    (int)BCM2835_EMMC_TUNE_STEPS_DDR_GET_STEPS(v));
}
#define BCM2835_EMMC_SPI_INT_SPT_GET_SELECT(v)           BITS_EXTRACT32(v, 0 , 8 )
#define BCM2835_EMMC_SPI_INT_SPT_CLR_SET_SELECT(v, set)          B_CLEAR_AND_SET32(v, set, 0 , 8 )
#define BCM2835_EMMC_SPI_INT_SPT_CLR_SELECT(v)           B_CLEAR32(v, 0 , 8 )
#define BCM2835_EMMC_SPI_INT_SPT_MASK_SELECT             BF_MAKE_MASK32(0, 8)
#define BCM2835_EMMC_SPI_INT_SPT_SHIFT_SELECT            0


static inline int bcm2835_emmc_spi_int_spt_to_string(char *buf, int bufsz, uint32_t v)
{
  return snprintf(buf, bufsz, "%08x,SELECT:%x",
    v,
    (int)BCM2835_EMMC_SPI_INT_SPT_GET_SELECT(v));
}
#define BCM2835_EMMC_SLOTISR_VER_GET_SLOT_STATUS(v)      BITS_EXTRACT32(v, 0 , 8 )
#define BCM2835_EMMC_SLOTISR_VER_GET_SDVERSION(v)        BITS_EXTRACT32(v, 16, 8 )
#define BCM2835_EMMC_SLOTISR_VER_GET_VENDOR(v)           BITS_EXTRACT32(v, 24, 8 )
#define BCM2835_EMMC_SLOTISR_VER_CLR_SET_SLOT_STATUS(v, set)     B_CLEAR_AND_SET32(v, set, 0 , 8 )
#define BCM2835_EMMC_SLOTISR_VER_CLR_SET_SDVERSION(v, set)       B_CLEAR_AND_SET32(v, set, 16, 8 )
#define BCM2835_EMMC_SLOTISR_VER_CLR_SET_VENDOR(v, set)          B_CLEAR_AND_SET32(v, set, 24, 8 )
#define BCM2835_EMMC_SLOTISR_VER_CLR_SLOT_STATUS(v)      B_CLEAR32(v, 0 , 8 )
#define BCM2835_EMMC_SLOTISR_VER_CLR_SDVERSION(v)        B_CLEAR32(v, 16, 8 )
#define BCM2835_EMMC_SLOTISR_VER_CLR_VENDOR(v)           B_CLEAR32(v, 24, 8 )
#define BCM2835_EMMC_SLOTISR_VER_MASK_SLOT_STATUS        BF_MAKE_MASK32(0, 8)
#define BCM2835_EMMC_SLOTISR_VER_MASK_SDVERSION          BF_MAKE_MASK32(16, 8)
#define BCM2835_EMMC_SLOTISR_VER_MASK_VENDOR             BF_MAKE_MASK32(24, 8)
#define BCM2835_EMMC_SLOTISR_VER_SHIFT_SLOT_STATUS       0
#define BCM2835_EMMC_SLOTISR_VER_SHIFT_SDVERSION         16
#define BCM2835_EMMC_SLOTISR_VER_SHIFT_VENDOR            24


static inline int bcm2835_emmc_slotisr_ver_to_string(char *buf, int bufsz, uint32_t v)
{
  return snprintf(buf, bufsz, "%08x,SLOT_STATUS:%x,SDVERSION:%x,VENDOR:%x",
    v,
    (int)BCM2835_EMMC_SLOTISR_VER_GET_SLOT_STATUS(v),
    (int)BCM2835_EMMC_SLOTISR_VER_GET_SDVERSION(v),
    (int)BCM2835_EMMC_SLOTISR_VER_GET_VENDOR(v));
}
#define BCM2835_EMMC_CARD_STATUS_GET_AKE_SEQ_ERROR(v)    BITS_EXTRACT32(v, 3 , 1 )
#define BCM2835_EMMC_CARD_STATUS_GET_APP_CMD(v)          BITS_EXTRACT32(v, 5 , 1 )
#define BCM2835_EMMC_CARD_STATUS_GET_FX_EVENT(v)         BITS_EXTRACT32(v, 6 , 1 )
#define BCM2835_EMMC_CARD_STATUS_GET_READY_FOR_DATA(v)   BITS_EXTRACT32(v, 8 , 1 )
#define BCM2835_EMMC_CARD_STATUS_GET_CURRENT_STATE(v)    BITS_EXTRACT32(v, 9 , 4 )
#define BCM2835_EMMC_CARD_STATUS_GET_ERASE_RESET(v)      BITS_EXTRACT32(v, 13, 1 )
#define BCM2835_EMMC_CARD_STATUS_GET_CARD_ECC_DISABLED(v) BITS_EXTRACT32(v, 14, 1 )
#define BCM2835_EMMC_CARD_STATUS_GET_WP_ERASE_SKIP(v)    BITS_EXTRACT32(v, 15, 1 )
#define BCM2835_EMMC_CARD_STATUS_GET_CSD_OVERWRITE(v)    BITS_EXTRACT32(v, 16, 1 )
#define BCM2835_EMMC_CARD_STATUS_GET_ERROR(v)            BITS_EXTRACT32(v, 19, 1 )
#define BCM2835_EMMC_CARD_STATUS_GET_CC_ERROR(v)         BITS_EXTRACT32(v, 20, 1 )
#define BCM2835_EMMC_CARD_STATUS_GET_CARD_ECC_FAILED(v)  BITS_EXTRACT32(v, 21, 1 )
#define BCM2835_EMMC_CARD_STATUS_GET_ILLEGAL_COMMAND(v)  BITS_EXTRACT32(v, 22, 1 )
#define BCM2835_EMMC_CARD_STATUS_GET_COM_CRC_ERROR(v)    BITS_EXTRACT32(v, 23, 1 )
#define BCM2835_EMMC_CARD_STATUS_GET_LOCK_UNLOCK_FAILED(v) BITS_EXTRACT32(v, 24, 1 )
#define BCM2835_EMMC_CARD_STATUS_GET_CARD_IS_LOCKED(v)   BITS_EXTRACT32(v, 25, 1 )
#define BCM2835_EMMC_CARD_STATUS_GET_WP_VIOLATION(v)     BITS_EXTRACT32(v, 26, 1 )
#define BCM2835_EMMC_CARD_STATUS_GET_ERASE_PARAM(v)      BITS_EXTRACT32(v, 27, 1 )
#define BCM2835_EMMC_CARD_STATUS_GET_ERASE_SEQ_ERROR(v)  BITS_EXTRACT32(v, 28, 1 )
#define BCM2835_EMMC_CARD_STATUS_GET_BLOCK_LEN_ERROR(v)  BITS_EXTRACT32(v, 29, 1 )
#define BCM2835_EMMC_CARD_STATUS_GET_ADDRESS_ERROR(v)    BITS_EXTRACT32(v, 30, 1 )
#define BCM2835_EMMC_CARD_STATUS_GET_OUT_OF_RANGE(v)     BITS_EXTRACT32(v, 31, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_SET_AKE_SEQ_ERROR(v, set)   B_CLEAR_AND_SET32(v, set, 3 , 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_SET_APP_CMD(v, set)         B_CLEAR_AND_SET32(v, set, 5 , 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_SET_FX_EVENT(v, set)        B_CLEAR_AND_SET32(v, set, 6 , 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_SET_READY_FOR_DATA(v, set)  B_CLEAR_AND_SET32(v, set, 8 , 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_SET_CURRENT_STATE(v, set)   B_CLEAR_AND_SET32(v, set, 9 , 4 )
#define BCM2835_EMMC_CARD_STATUS_CLR_SET_ERASE_RESET(v, set)     B_CLEAR_AND_SET32(v, set, 13, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_SET_CARD_ECC_DISABLED(v, set) B_CLEAR_AND_SET32(v, set, 14, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_SET_WP_ERASE_SKIP(v, set)   B_CLEAR_AND_SET32(v, set, 15, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_SET_CSD_OVERWRITE(v, set)   B_CLEAR_AND_SET32(v, set, 16, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_SET_ERROR(v, set)           B_CLEAR_AND_SET32(v, set, 19, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_SET_CC_ERROR(v, set)        B_CLEAR_AND_SET32(v, set, 20, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_SET_CARD_ECC_FAILED(v, set) B_CLEAR_AND_SET32(v, set, 21, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_SET_ILLEGAL_COMMAND(v, set) B_CLEAR_AND_SET32(v, set, 22, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_SET_COM_CRC_ERROR(v, set)   B_CLEAR_AND_SET32(v, set, 23, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_SET_LOCK_UNLOCK_FAILED(v, set) B_CLEAR_AND_SET32(v, set, 24, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_SET_CARD_IS_LOCKED(v, set)  B_CLEAR_AND_SET32(v, set, 25, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_SET_WP_VIOLATION(v, set)    B_CLEAR_AND_SET32(v, set, 26, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_SET_ERASE_PARAM(v, set)     B_CLEAR_AND_SET32(v, set, 27, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_SET_ERASE_SEQ_ERROR(v, set) B_CLEAR_AND_SET32(v, set, 28, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_SET_BLOCK_LEN_ERROR(v, set) B_CLEAR_AND_SET32(v, set, 29, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_SET_ADDRESS_ERROR(v, set)   B_CLEAR_AND_SET32(v, set, 30, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_SET_OUT_OF_RANGE(v, set)    B_CLEAR_AND_SET32(v, set, 31, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_AKE_SEQ_ERROR(v)    B_CLEAR32(v, 3 , 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_APP_CMD(v)          B_CLEAR32(v, 5 , 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_FX_EVENT(v)         B_CLEAR32(v, 6 , 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_READY_FOR_DATA(v)   B_CLEAR32(v, 8 , 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_CURRENT_STATE(v)    B_CLEAR32(v, 9 , 4 )
#define BCM2835_EMMC_CARD_STATUS_CLR_ERASE_RESET(v)      B_CLEAR32(v, 13, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_CARD_ECC_DISABLED(v) B_CLEAR32(v, 14, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_WP_ERASE_SKIP(v)    B_CLEAR32(v, 15, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_CSD_OVERWRITE(v)    B_CLEAR32(v, 16, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_ERROR(v)            B_CLEAR32(v, 19, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_CC_ERROR(v)         B_CLEAR32(v, 20, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_CARD_ECC_FAILED(v)  B_CLEAR32(v, 21, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_ILLEGAL_COMMAND(v)  B_CLEAR32(v, 22, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_COM_CRC_ERROR(v)    B_CLEAR32(v, 23, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_LOCK_UNLOCK_FAILED(v) B_CLEAR32(v, 24, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_CARD_IS_LOCKED(v)   B_CLEAR32(v, 25, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_WP_VIOLATION(v)     B_CLEAR32(v, 26, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_ERASE_PARAM(v)      B_CLEAR32(v, 27, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_ERASE_SEQ_ERROR(v)  B_CLEAR32(v, 28, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_BLOCK_LEN_ERROR(v)  B_CLEAR32(v, 29, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_ADDRESS_ERROR(v)    B_CLEAR32(v, 30, 1 )
#define BCM2835_EMMC_CARD_STATUS_CLR_OUT_OF_RANGE(v)     B_CLEAR32(v, 31, 1 )
#define BCM2835_EMMC_CARD_STATUS_MASK_AKE_SEQ_ERROR      BF_MAKE_MASK32(3, 1)
#define BCM2835_EMMC_CARD_STATUS_MASK_APP_CMD            BF_MAKE_MASK32(5, 1)
#define BCM2835_EMMC_CARD_STATUS_MASK_FX_EVENT           BF_MAKE_MASK32(6, 1)
#define BCM2835_EMMC_CARD_STATUS_MASK_READY_FOR_DATA     BF_MAKE_MASK32(8, 1)
#define BCM2835_EMMC_CARD_STATUS_MASK_CURRENT_STATE      BF_MAKE_MASK32(9, 4)
#define BCM2835_EMMC_CARD_STATUS_MASK_ERASE_RESET        BF_MAKE_MASK32(13, 1)
#define BCM2835_EMMC_CARD_STATUS_MASK_CARD_ECC_DISABLED  BF_MAKE_MASK32(14, 1)
#define BCM2835_EMMC_CARD_STATUS_MASK_WP_ERASE_SKIP      BF_MAKE_MASK32(15, 1)
#define BCM2835_EMMC_CARD_STATUS_MASK_CSD_OVERWRITE      BF_MAKE_MASK32(16, 1)
#define BCM2835_EMMC_CARD_STATUS_MASK_ERROR              BF_MAKE_MASK32(19, 1)
#define BCM2835_EMMC_CARD_STATUS_MASK_CC_ERROR           BF_MAKE_MASK32(20, 1)
#define BCM2835_EMMC_CARD_STATUS_MASK_CARD_ECC_FAILED    BF_MAKE_MASK32(21, 1)
#define BCM2835_EMMC_CARD_STATUS_MASK_ILLEGAL_COMMAND    BF_MAKE_MASK32(22, 1)
#define BCM2835_EMMC_CARD_STATUS_MASK_COM_CRC_ERROR      BF_MAKE_MASK32(23, 1)
#define BCM2835_EMMC_CARD_STATUS_MASK_LOCK_UNLOCK_FAILED BF_MAKE_MASK32(24, 1)
#define BCM2835_EMMC_CARD_STATUS_MASK_CARD_IS_LOCKED     BF_MAKE_MASK32(25, 1)
#define BCM2835_EMMC_CARD_STATUS_MASK_WP_VIOLATION       BF_MAKE_MASK32(26, 1)
#define BCM2835_EMMC_CARD_STATUS_MASK_ERASE_PARAM        BF_MAKE_MASK32(27, 1)
#define BCM2835_EMMC_CARD_STATUS_MASK_ERASE_SEQ_ERROR    BF_MAKE_MASK32(28, 1)
#define BCM2835_EMMC_CARD_STATUS_MASK_BLOCK_LEN_ERROR    BF_MAKE_MASK32(29, 1)
#define BCM2835_EMMC_CARD_STATUS_MASK_ADDRESS_ERROR      BF_MAKE_MASK32(30, 1)
#define BCM2835_EMMC_CARD_STATUS_MASK_OUT_OF_RANGE       BF_MAKE_MASK32(31, 1)
#define BCM2835_EMMC_CARD_STATUS_SHIFT_AKE_SEQ_ERROR     3
#define BCM2835_EMMC_CARD_STATUS_SHIFT_APP_CMD           5
#define BCM2835_EMMC_CARD_STATUS_SHIFT_FX_EVENT          6
#define BCM2835_EMMC_CARD_STATUS_SHIFT_READY_FOR_DATA    8
#define BCM2835_EMMC_CARD_STATUS_SHIFT_CURRENT_STATE     9
#define BCM2835_EMMC_CARD_STATUS_SHIFT_ERASE_RESET       13
#define BCM2835_EMMC_CARD_STATUS_SHIFT_CARD_ECC_DISABLED 14
#define BCM2835_EMMC_CARD_STATUS_SHIFT_WP_ERASE_SKIP     15
#define BCM2835_EMMC_CARD_STATUS_SHIFT_CSD_OVERWRITE     16
#define BCM2835_EMMC_CARD_STATUS_SHIFT_ERROR             19
#define BCM2835_EMMC_CARD_STATUS_SHIFT_CC_ERROR          20
#define BCM2835_EMMC_CARD_STATUS_SHIFT_CARD_ECC_FAILED   21
#define BCM2835_EMMC_CARD_STATUS_SHIFT_ILLEGAL_COMMAND   22
#define BCM2835_EMMC_CARD_STATUS_SHIFT_COM_CRC_ERROR     23
#define BCM2835_EMMC_CARD_STATUS_SHIFT_LOCK_UNLOCK_FAILED 24
#define BCM2835_EMMC_CARD_STATUS_SHIFT_CARD_IS_LOCKED    25
#define BCM2835_EMMC_CARD_STATUS_SHIFT_WP_VIOLATION      26
#define BCM2835_EMMC_CARD_STATUS_SHIFT_ERASE_PARAM       27
#define BCM2835_EMMC_CARD_STATUS_SHIFT_ERASE_SEQ_ERROR   28
#define BCM2835_EMMC_CARD_STATUS_SHIFT_BLOCK_LEN_ERROR   29
#define BCM2835_EMMC_CARD_STATUS_SHIFT_ADDRESS_ERROR     30
#define BCM2835_EMMC_CARD_STATUS_SHIFT_OUT_OF_RANGE      31


static inline int bcm2835_emmc_card_status_to_string(char *buf, int bufsz, uint32_t v)
{
  return snprintf(buf, bufsz, "%08x,AKE_SEQ_ERROR:%x,APP_CMD:%x,FX_EVENT:%x,READY_FOR_DATA:%x,CURRENT_STATE:%x,ERASE_RESET:%x,CARD_ECC_DISABLED:%x,WP_ERASE_SKIP:%x,CSD_OVERWRITE:%x,ERROR:%x,CC_ERROR:%x,CARD_ECC_FAILED:%x,ILLEGAL_COMMAND:%x,COM_CRC_ERROR:%x,LOCK_UNLOCK_FAILED:%x,CARD_IS_LOCKED:%x,WP_VIOLATION:%x,ERASE_PARAM:%x,ERASE_SEQ_ERROR:%x,BLOCK_LEN_ERROR:%x,ADDRESS_ERROR:%x,OUT_OF_RANGE:%x",
    v,
    (int)BCM2835_EMMC_CARD_STATUS_GET_AKE_SEQ_ERROR(v),
    (int)BCM2835_EMMC_CARD_STATUS_GET_APP_CMD(v),
    (int)BCM2835_EMMC_CARD_STATUS_GET_FX_EVENT(v),
    (int)BCM2835_EMMC_CARD_STATUS_GET_READY_FOR_DATA(v),
    (int)BCM2835_EMMC_CARD_STATUS_GET_CURRENT_STATE(v),
    (int)BCM2835_EMMC_CARD_STATUS_GET_ERASE_RESET(v),
    (int)BCM2835_EMMC_CARD_STATUS_GET_CARD_ECC_DISABLED(v),
    (int)BCM2835_EMMC_CARD_STATUS_GET_WP_ERASE_SKIP(v),
    (int)BCM2835_EMMC_CARD_STATUS_GET_CSD_OVERWRITE(v),
    (int)BCM2835_EMMC_CARD_STATUS_GET_ERROR(v),
    (int)BCM2835_EMMC_CARD_STATUS_GET_CC_ERROR(v),
    (int)BCM2835_EMMC_CARD_STATUS_GET_CARD_ECC_FAILED(v),
    (int)BCM2835_EMMC_CARD_STATUS_GET_ILLEGAL_COMMAND(v),
    (int)BCM2835_EMMC_CARD_STATUS_GET_COM_CRC_ERROR(v),
    (int)BCM2835_EMMC_CARD_STATUS_GET_LOCK_UNLOCK_FAILED(v),
    (int)BCM2835_EMMC_CARD_STATUS_GET_CARD_IS_LOCKED(v),
    (int)BCM2835_EMMC_CARD_STATUS_GET_WP_VIOLATION(v),
    (int)BCM2835_EMMC_CARD_STATUS_GET_ERASE_PARAM(v),
    (int)BCM2835_EMMC_CARD_STATUS_GET_ERASE_SEQ_ERROR(v),
    (int)BCM2835_EMMC_CARD_STATUS_GET_BLOCK_LEN_ERROR(v),
    (int)BCM2835_EMMC_CARD_STATUS_GET_ADDRESS_ERROR(v),
    (int)BCM2835_EMMC_CARD_STATUS_GET_OUT_OF_RANGE(v));
}
