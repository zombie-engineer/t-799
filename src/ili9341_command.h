#pragma once

/* horizontal refresh direction */
#define ILI9341_CMD_MEM_ACCESS_CONTROL_MH  (1<<2)
/* pixel format default is bgr, with this flag it's rgb */
#define ILI9341_CMD_MEM_ACCESS_CONTROL_BGR (1<<3)
/* horizontal refresh direction */
#define ILI9341_CMD_MEM_ACCESS_CONTROL_ML  (1<<4)
/* swap rows and columns default is PORTRAIT, this flags makes it ALBUM */
#define ILI9341_CMD_MEM_ACCESS_CONTROL_MV  (1<<5)
/* column address order */
#define ILI9341_CMD_MEM_ACCESS_CONTROL_MX  (1<<6)
/* row address order */
#define ILI9341_CMD_MEM_ACCESS_CONTROL_MY  (1<<7)

#define ILI9341_SET_CURSOR_XY_ARG2BYTES(__a0, __a1) \
  { \
    BYTE_EXTRACT32((__a0), 1), \
    BYTE_EXTRACT32((__a0), 0), \
    BYTE_EXTRACT32((__a1), 1), \
    BYTE_EXTRACT32((__a1), 0), \
  }

#define ILI9341_CASET_INIT_ARG(__x0, __x1) \
  ILI9341_SET_CURSOR_XY_ARG2BYTES(__x0, __x1)

#define ILI9341_PASET_INIT_ARG(__y0, __y1) \
  ILI9341_SET_CURSOR_XY_ARG2BYTES(__y0, __y1)

#define ILI9341_CMD_SOFT_RESET            0x01
#define ILI9341_CMD_READ_ID               0x04
#define ILI9341_CMD_SLEEP_OUT             0x11
#define ILI9341_CMD_DISPLAY_OFF           0x28
#define ILI9341_CMD_DISPLAY_ON            0x29
#define ILI9341_CMD_CASET                 0x2a
#define ILI9341_CMD_PASET                 0x2b
#define ILI9341_CMD_RAMWR                 0x2c
#define ILI9341_CMD_MEM_ACCESS_CONTROL    0x36
#define ILI9341_CMD_COLMOD                0x3a
#define ILI9341_CMD_WRITE_MEMORY_CONTINUE 0x3c
#define ILI9341_CMD_POWER_CTL_A           0xcb
#define ILI9341_CMD_POWER_CTL_B           0xcf
#define ILI9341_CMD_TIMING_CTL_A          0xe8
#define ILI9341_CMD_TIMING_CTL_B          0xea
#define ILI9341_CMD_POWER_ON_SEQ          0xed
#define ILI9341_CMD_PUMP_RATIO            0xf7
#define ILI9341_CMD_POWER_CTL_1           0xc0
#define ILI9341_CMD_POWER_CTL_2           0xc1
#define ILI9341_CMD_VCOM_CTL_1            0xc5
#define ILI9341_CMD_VCOM_CTL_2            0xc7
#define ILI9341_CMD_FRAME_RATE_CTL        0xb1
#define ILI9341_CMD_BLANK_PORCH           0xb5
#define ILI9341_CMD_DISPL_FUNC            0xb6

/*
 * ILI9341 displays are able to update at any rate between 61Hz to up to
 *  119Hz. Default at power on is 70Hz.
 */
#define ILI9341_FRAMERATE_61_HZ 0x1F
#define ILI9341_FRAMERATE_63_HZ 0x1E
#define ILI9341_FRAMERATE_65_HZ 0x1D
#define ILI9341_FRAMERATE_68_HZ 0x1C
#define ILI9341_FRAMERATE_70_HZ 0x1B
#define ILI9341_FRAMERATE_73_HZ 0x1A
#define ILI9341_FRAMERATE_76_HZ 0x19
#define ILI9341_FRAMERATE_79_HZ 0x18
#define ILI9341_FRAMERATE_83_HZ 0x17
#define ILI9341_FRAMERATE_86_HZ 0x16
#define ILI9341_FRAMERATE_90_HZ 0x15
#define ILI9341_FRAMERATE_95_HZ 0x14
#define ILI9341_FRAMERATE_100_HZ 0x13
#define ILI9341_FRAMERATE_106_HZ 0x12
#define ILI9341_FRAMERATE_112_HZ 0x11
#define ILI9341_FRAMERATE_119_HZ 0x10
#define ILI9341_UPDATE_FRAMERATE ILI9341_FRAMERATE_119_HZ
