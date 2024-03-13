#include <stringlib.h>
#include <limits.h>
#include <common.h>
#include <compiler.h>
#include <stdint.h>

#define IS_DIGIT(ch) ((ch) >= '0' && (ch) <= '9')

#define LLU(x) ((unsigned long long)(x))

enum {
  FMT_SIZE_NO      = 0x00,
  FMT_SIZE_LONG    = 0x01,
  FMT_SIZE_LLONG   = 0x02,
  FMT_SIZE_INTMAX  = 0x04,
  FMT_SIZE_SIZE_T  = 0x08,
  FMT_SIZE_T       = 0x10,
  FMT_SIZE_INT64   = 0x20,
  FMT_SIZE_HALF    = 0x40,
  FMT_SIZE_HHALF   = 0x80,
  FMT_SIZE_DOUBLE  = 0x100,
  FMT_SIZE_PTRDIFF = 0x200
};

struct printf_ctx {
  const char *fmt;
  char *dst;
  char *dst_virt;
  char *dst_end;
  int flag_plus;
  int flag_space;
  int flag_zero;
  int flag_left_align;
  int flag_hex_prefix;

  int width;
  int precision;
  int size;
  char type;

  double arg_double;
  unsigned long long arg;
};

struct print_num_spec {
  int space_padding;
  int padding;
  int wordlen;
  int is_neg_sign;
};

static inline char to_char_16(int i, int uppercase) {
  if (i >= 10)
    return i + (1 - uppercase) * ('a' - 'A') + 'A' - 10;
  return i + '0';
}

static inline char digit_to_char_base10(int value) {
  /*
   * As an error checking we output '#' if base is inconsistent with the
   * value
   */
  if (value > 9)
    return '#';
  return value + '0';
}

#define APPEND(c) { if (c->dst < end) *(dst++) = c; counter++; }

#define DST_FREE_BYTES() (c->dst_end - c->dst)

#define DST_APPEND_C(ch)\
  do {\
    if (c->dst < c->dst_end)\
      *c->dst++ = ch;\
    c->dst_virt++;\
  } while(0)

#define DST_APPEND_S(s)\
  do {\
    const char *__s = s;\
    char ch = *__s++;\
    while(ch) {\
      DST_APPEND_C(ch);\
      ch = *__s++;\
    }\
  } while(0)

#define DST_MEMCPY(src, len) \
  {\
    int op_len = MIN(len, DST_FREE_BYTES());\
    memcpy(c->dst, src, op_len);\
    c->dst += op_len;\
    c->dst_virt += op_len;\
  }

#define DST_MEMSET(val, len) \
  {\
    int op_len = MIN(len, DST_FREE_BYTES());\
    memset(c->dst, val, op_len);\
    c->dst += op_len;\
    c->dst_virt += len;\
  }

static const char *__mem_find_any_byte_of_2(const char *start_addr,
  const char *end_addr, uint64_t bytearray)
{
  const char *p = start_addr;
  char a = bytearray & 0xff;
  char b = (bytearray >> 8) & 0xff;
  char t;
  while(p < end_addr) {
    t = *p;
    if (t == a || t == b)
      break;
    ++p;
  }
  return p;
}

static inline const char *__find_special_or_null(const char *pos,
  const char *end)
{
  while(1) {
    pos = __mem_find_any_byte_of_2(pos, end, ('%' << 8) | ('\0' << 0));
    if (*pos && *(pos+1) != '%')
      break;
    if (!*pos)
      break;
    pos += 1;
  }
  return pos;
}

#define FMT_FETCH_SPECIAL() \
  do { \
    special = *(++c->fmt); \
  } while(0)

#define FMT_HANDLE_FLAG(fname, fchar) \
  do { \
    c->flag_ ## fname = 0; \
    if (special == '0') { \
      c->flag_ ## fname = 1; \
      FMT_FETCH_SPECIAL(); \
    } \
  } while(0)

#define FMT_HANDLE_STAR_OR_DIGIT(digit_var, args) \
  do { \
    digit_var = -1; \
    if (special == '*') { \
      /* digit in next argument */ \
      digit_var = __builtin_va_arg(args, int); \
      FMT_FETCH_SPECIAL(); \
    } else if (IS_DIGIT(special)) { \
      /* digit in fmt as decimal */ \
      digit_var = 0; \
      do { \
        digit_var = digit_var * 10 + (special - '0'); \
        FMT_FETCH_SPECIAL(); \
      } while(IS_DIGIT(special)); \
    } \
  } while(0)

#define FMT_HANDLE_SIZE() \
  do { \
    switch(special) { \
      case 'c': c->fmt--; c->size = FMT_SIZE_NO; break; \
      case 'f': c->fmt--; c->size = FMT_SIZE_DOUBLE; break; \
      case 'l': c->size = FMT_SIZE_LONG; break; \
      case 'h': c->size = FMT_SIZE_HALF; break; \
      case 'j': c->size = FMT_SIZE_INTMAX; break; \
      case 'z': c->size = FMT_SIZE_SIZE_T; break; \
      case 't': c->size = FMT_SIZE_PTRDIFF; break; \
      case 'L': c->size = FMT_SIZE_INT64; break; \
      default:  c->size = FMT_SIZE_NO; c->fmt--; break; \
    } \
    FMT_FETCH_SPECIAL(); \
    if (c->size == FMT_SIZE_LONG && special == 'l') { \
      c->size = FMT_SIZE_LLONG; \
      FMT_FETCH_SPECIAL(); \
    } \
    else if (c->size == FMT_SIZE_HALF && special == 'h') { \
      c->size = FMT_SIZE_HHALF; \
      FMT_FETCH_SPECIAL(); \
    } \
    else if (special == 'p' || special == 's') \
      c->size = FMT_SIZE_LLONG; \
  } while(0)

static unsigned long long __decimal_table[] = {
  1,
  10,
  100,
  1000,
  10000,
  100000,
  1000000,
  10000000,
  100000000,
  1000000000,
  10000000000,
  100000000000,
  1000000000000,
  10000000000000,
  100000000000000,
  1000000000000000,
  10000000000000000,
  100000000000000000,
  1000000000000000000,
  10000000000000000000ull
};

static int OPTIMIZED get_num_digit_chars_base10(uint64_t val)
{
  int num_digit_chars = 1;
  uint64_t divider = 10;
  while(val / divider) {
    divider *= 10;
    num_digit_chars++;
  }
  return num_digit_chars;
}

static int OPTIMIZED closest_round_base10_nodiv(uint64_t val)
{
  const int max_idx = ARRAY_SIZE(__decimal_table); /* 20 */
  int base = 0;
  int delta = max_idx / 2;
  int pivot;

  do {
    pivot = base + delta;
    delta >>= 1;
    if (val >= __decimal_table[pivot])
      base = pivot;
  } while(base != pivot || delta);

  if (pivot == 0)
    return 0;

  if (val >= __decimal_table[pivot]) {
    if (pivot <= ARRAY_SIZE(__decimal_table) - 2 && val >= __decimal_table[pivot + 1])
        return pivot + 1;
    return pivot;
  }

  return pivot - 1;
}

static OPTIMIZED int get_digit(unsigned long long num,
  unsigned long long powered_base, unsigned long *out_round)
{
  int i;
  unsigned long long cmp = powered_base;
  for (i = 1; i < 11; ++i) {
    if (num < cmp) {
      *out_round = cmp - powered_base;
      return i - 1;
    }
    cmp += powered_base;
  }
  *out_round = 0;
  return 0;
}


static inline void __print_fn_prep_spec(struct printf_ctx *c, struct print_num_spec *s)
{
#ifdef NODIV
  s->wordlen = closest_round_base10_nodiv(c->arg);
#else
  s->wordlen = get_num_digit_chars_base10(c->arg);
#endif
  s->wordlen += s->is_neg_sign | c->flag_plus | c->flag_space;
  s->padding = MAX(c->width - s->wordlen, 0);
  s->space_padding = MAX(s->padding - MAX(MAX(c->precision, 0) - s->wordlen, 0), 0);
  if (c->flag_zero && c->precision == -1)
    s->space_padding = 0;
}

static inline void __print_fn_number_10_div(struct printf_ctx *c,
  uint64_t value, int wordlen)
{
  uint64_t divider = __decimal_table[wordlen - 1];
  char digit_char;
  uint8_t digit;

  while(divider) {
    digit = (uint8_t)(value / divider);
    digit_char = digit_to_char_base10(digit);
    DST_APPEND_C(digit_char);
    value = value % divider;
    divider /= 10;
  }
}

static inline void __print_fn_number_10_nodiv(struct printf_ctx *c,
  uint64_t value, int wordlen)
{
  char tmp;
  uint64_t tmp_arg, subtract_round;
  int tmp_num;

  tmp_arg = value;
  for (;wordlen > 0; wordlen--) {
    tmp_num = get_digit(tmp_arg, __decimal_table[wordlen - 1],
      &subtract_round);
    tmp = digit_to_char_base10(tmp_num);
    DST_APPEND_C(tmp);
    tmp_arg -= subtract_round;
  }
}

static inline void __print_fn_number_10(struct printf_ctx *c,
  struct print_num_spec *s)
{
  DST_MEMSET(' ', s->space_padding);
  s->padding -= s->space_padding;

  if (s->is_neg_sign) {
    DST_APPEND_C('-');
    s->wordlen--;
  }
  else if (c->flag_plus) {
    DST_APPEND_C('+');
    s->wordlen--;
  }
  else if (c->flag_space) {
    DST_APPEND_C(' ');
    s->wordlen--;
  }

  DST_MEMSET('0', s->padding);
#ifdef NODIV
  __print_fn_number_10_nodiv(c, s->wordlen);
#else
  __print_fn_number_10_div(c, c->arg, s->wordlen);
#endif
}

static NOINLINE void __print_fn_number_10_u(struct printf_ctx *c)
{
  struct print_num_spec s = { 0 };

  if (sizeof(long) == sizeof(int) && c->size == FMT_SIZE_LONG)
    c->arg &= 0xffffffff;
  else if (c->size == FMT_SIZE_NO)
    c->arg &= 0xffffffff;

  __print_fn_prep_spec(c, &s);
  __print_fn_number_10(c, &s);
}

static NOINLINE void __print_fn_number_10_d(struct printf_ctx *c)
{
  struct print_num_spec s = { 0 };
  int max_bit_pos = 31;
  uint64_t type_mask;
  // TODO: Support more types
  if (c->size & (FMT_SIZE_LLONG |
#if (__LP64__ == 1)
        FMT_SIZE_LONG |
#endif
        FMT_SIZE_INT64)) {
     max_bit_pos = 63;
  }

  /* zero-out top part of arg for smaller types */
  // type_mask = (1<<(max_bit_pos + 1)) - 1; does not work for 64
  switch(max_bit_pos) {
    case  7: type_mask = 0xff;               break;
    case 15: type_mask = 0xffff;             break;
    case 31: type_mask = 0xffffffff;         break;
    default: type_mask = 0xffffffffffffffff; break;
  }

  c->arg &= type_mask;

  if (c->arg & (1ll << max_bit_pos)) {
    c->arg = (~c->arg + 1) & type_mask;
    s.is_neg_sign = 1;
  }

  __print_fn_prep_spec(c, &s);
  __print_fn_number_10(c, &s);
}

static void NOINLINE  __print_fn_number_16(struct printf_ctx *c)
{
  int nibble;
  int leading_zeroes = 0;
  int maxbits = 0;
  int rightmost_nibble;
  int space_padding, padding;
  char tmp;

  switch(c->size) {
  case FMT_SIZE_LLONG:
    leading_zeroes = __builtin_clzll(c->arg);
    maxbits = sizeof(long long) * 8;
    break;
  case FMT_SIZE_LONG:
    leading_zeroes = __builtin_clzl((long)c->arg);
    maxbits = sizeof(long) * 8;
    break;
  case FMT_SIZE_NO:
    leading_zeroes = __builtin_clz((int)c->arg);
    maxbits = sizeof(int) * 8;
    break;
  default:
    DST_APPEND_S("<>");
    break;
  }

  /* padding */
  rightmost_nibble = (maxbits - leading_zeroes - 1) / 4;
  padding = MAX(MAX(c->width, c->precision) - (rightmost_nibble + 1), 0);
  if (c->flag_hex_prefix)
    padding = MAX(padding - 2, 0);
  space_padding = padding;
  if (c->precision != -1) {
    c->precision = MAX(c->precision - (rightmost_nibble + 1), 0);
    space_padding = padding - c->precision;
  }
  else if (c->flag_zero)
    space_padding = 0;

  DST_MEMSET(' ', space_padding);
  padding -= space_padding;
  if (c->flag_hex_prefix) {
    DST_APPEND_C('0');
    DST_APPEND_C('x');
  }

  DST_MEMSET('0', padding);

  for (;rightmost_nibble >= 0; rightmost_nibble--) {
    nibble = (int)((LLU(c->arg) >> (rightmost_nibble * 4)) & 0xf);
    tmp = to_char_16(nibble, /* uppercase */ c->type < 'a');
    DST_APPEND_C(tmp);
  }
}

static inline char *__printf_strlcpy(char *dst, const char *src, int n)
{
  const char *src_end = src + n;
  while(*src && src != src_end)
    *dst++ = *src++;
  return dst;
}

static void NOINLINE __print_fn_string(struct printf_ctx *c)
{
  const char *src = (const char *)c->arg;
  c->dst = __printf_strlcpy(c->dst, src, c->dst_end - c->dst);

  while(*src) {
    c->dst_virt++;
    src++;
  }
}

static void NOINLINE __print_fn_char(struct printf_ctx *c)
{
  char ch = (char)c->arg;
  if (!isprint(ch))
    ch = '.';
  if (c->dst < c->dst_end)
    *(c->dst++) = ch;
}

static void NOINLINE __print_fn_ignore(struct printf_ctx *c) {}
static void NOINLINE __print_fn_float_hex(struct printf_ctx *c) { }
static void NOINLINE __print_fn_float_exp(struct printf_ctx *c) { }
static void NOINLINE __print_fn_float(struct printf_ctx *c)
{
  int precision = 4;
  float arg = (float)c->arg_double;
  int whole, fractional;

  if (arg < 0) {
    arg *= -1.0f;
    DST_APPEND_C('-');
  }
  whole = (int)arg;
  fractional = (arg - (float)whole) * __decimal_table[precision + 1];

  __print_fn_number_10_div(c, whole,
    get_num_digit_chars_base10(whole));

  DST_APPEND_C('.');
  __print_fn_number_10_div(c, fractional,
    get_num_digit_chars_base10(fractional));
}

static void NOINLINE __print_fn_number_8(struct printf_ctx *c) { }
static void NOINLINE __print_fn_address(struct printf_ctx *c)
{
  c->flag_hex_prefix = 1;
  __print_fn_number_16(c);
}

typedef void (*__print_fn_t)(struct printf_ctx *);
ALIGNED(64) __print_fn_t __printf_fn_map[] = {
#define DECL(ch, fn) [ch - 'a'] = fn
#define IGNR(ch) [ch - 'a'] = __print_fn_ignore

  DECL('a', __print_fn_float_hex),
  IGNR('b'),
  DECL('c', __print_fn_char),
  DECL('d', __print_fn_number_10_d),
  DECL('e', __print_fn_float_exp),
  DECL('f', __print_fn_float),
  DECL('g', __print_fn_float),
  IGNR('h'),
  DECL('i', __print_fn_number_10_d),
  IGNR('j'),
  IGNR('k'),
  IGNR('l'),
  IGNR('m'),
  IGNR('n'),
  DECL('o', __print_fn_number_8),
  DECL('p', __print_fn_address),
  IGNR('q'),
  IGNR('r'),
  DECL('s', __print_fn_string),
  IGNR('t'),
  DECL('u', __print_fn_number_10_u),
  IGNR('v'),
  IGNR('w'),
  DECL('x', __print_fn_number_16),
  DECL('y', __print_fn_number_16),
  DECL('z', __print_fn_number_16)
};

void fmt_print_type(struct printf_ctx *c)
{
  /* To lower type 'A'(0x41) -> 'a'(0x61), because 0x61=0x41|0x20
   * but we keep type in original.
   */
  int gentype = c->type | 0x20;
  if (gentype >= 'a' && gentype <= 'x')
    __printf_fn_map[gentype - 'a'](c);
}

/*
 * Returns NULL if reached zero termination or any format specifier,
 * after % in case it was not %%, ex 'string%p' returns 'p'
 * c->fmt points at position where return value wat taken.
 */
static inline char fmt_skip_to_special(struct printf_ctx *c)
{
  char special = 0;
  const char *fmt_pos;

  fmt_pos = __find_special_or_null(c->fmt, c->fmt + 512);

  DST_MEMCPY(c->fmt, fmt_pos - c->fmt);
  c->fmt = fmt_pos;

  special = *c->fmt;
  if (special == '%')
    special = *(++(c->fmt));

  return special;
}

static inline int fmt_parse_token(struct printf_ctx *c,
  __builtin_va_list *args)
{
  char special;
  special = fmt_skip_to_special(c);
  /* zero-termination check */
  if (!special)
    return 1;

  FMT_HANDLE_FLAG(zero      , '0');
  FMT_HANDLE_FLAG(left_align, '-');
  FMT_HANDLE_FLAG(plus      , '+');
  FMT_HANDLE_FLAG(space     , ' ');

  // TODO implement left-align flag '-'
  // This is to disable compiler warning
  if (c->flag_left_align);

  FMT_HANDLE_STAR_OR_DIGIT(c->width, *args);
  FMT_HANDLE_STAR_OR_DIGIT(c->precision, *args);
  FMT_HANDLE_SIZE();

  switch(c->size) {
    case FMT_SIZE_SIZE_T:
      c->arg = __builtin_va_arg(*args, size_t);
      break;
    case FMT_SIZE_LONG:
      c->arg = __builtin_va_arg(*args, long);
      break;
    case FMT_SIZE_DOUBLE:
      c->arg_double = __builtin_va_arg(*args, double);
      break;
    case FMT_SIZE_LLONG:
      c->arg = __builtin_va_arg(*args, long long);
      break;
    default:
      c->arg = __builtin_va_arg(*args, int);
      break;
  }
  c->type = special;
  c->fmt++;
  return 0;
}

int /*optimized*/ __vsnprintf(char *dst, size_t dst_len, const char *fmt,
  __builtin_va_list *args)
{
  struct printf_ctx ctx = { 0 };
  struct printf_ctx *c = &ctx;
  c->fmt = fmt;
  c->dst = dst;

  /*
   * dst_len is harder than you think. If dst_len is 0 - we don't write
   * anything, just calculate resulting string length.
   * if dst_len is greater than 0, 1 byte from this length is for
   * zero-termination. if dst_len == 1, the first byte in dst
   * will be written 0. So the actual string length decays to dst_len - 1.
   */
  c->dst_end = c->dst + dst_len;
  c->dst_virt = c->dst;

  while(*c->fmt && !fmt_parse_token(c, args))
    fmt_print_type(c);

  /*
   * Two possible outcomes are possible.
   * - c->dst has reached end.
   * - c->dst haven't readed end.
   * In first - we still need to write zero-termination somewhere.
   * So we overwrite the end of the resulting string.
   * In second - we safely put zero-termination after the text,
   * because there is still space to do that.
   */
  if (dst_len) {
    if (c->dst == c->dst_end)
      *(c->dst - 1) = 0;
    else
      *c->dst = 0;
  }

  return c->dst_virt - dst;
}

int /*optimized*/ vsnprintf(char *dst, size_t dst_len, const char *fmt,
  __builtin_va_list *args)
{
  return __vsnprintf(dst, dst_len, fmt, args);
}

int /*optimized*/ vsprintf(char *dst, const char *fmt, __builtin_va_list *args)
{
  return __vsnprintf(dst, 4096, fmt, args);
}

int sprintf(char *dst, const char *fmt, ...)
{
  __builtin_va_list args;
  __builtin_va_start(args, fmt);
  return __vsnprintf(dst, 4096, fmt, &args);
}

int snprintf(char *dst, size_t dst_size, const char *fmt, ...)
{
  __builtin_va_list args;
  __builtin_va_start(args, fmt);

  return __vsnprintf(dst, dst_size, fmt, &args);
}
