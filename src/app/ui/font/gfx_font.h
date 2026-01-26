#pragma once

struct gfx_glyph {
    uint16_t bitmap_offset;
    uint8_t  width;
    uint8_t  height;
    uint8_t  x_advance;
    int8_t   x_offset;
    int8_t   y_offset;
};

struct gfx_font {
    const uint8_t *bitmap;
    const struct gfx_glyph *glyph;
    uint8_t first;
    uint8_t last;
    uint8_t y_advance;
};

