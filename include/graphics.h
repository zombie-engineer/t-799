#pragma once

struct vec2 {
  uint32_t x;
  uint32_t y;
};

struct rectangle {
  struct vec2 pos;
  struct vec2 size;
};

