#include <drivers/gpio.h>
#include <memory_map.h>
#include <ioreg.h>
#include <common.h>

#define BCM2835_GPIO_MAX_PIN 53
#define GPIO_MAX_PIN BCM2835_GPIO_MAX_PIN
#define GPIO_BASE BCM2835_MEM_GPIO_BASE
#define GPIO_REG(__offset) ((ioreg32_t)(GPIO_BASE + __offset))
#define GPIO_REG_GPFSEL0   GPIO_REG(0x00)
#define GPIO_REG_GPFSEL1   GPIO_REG(0x04)
#define GPIO_REG_GPFSEL2   GPIO_REG(0x08)
#define GPIO_REG_GPFSEL3   GPIO_REG(0x0C)
#define GPIO_REG_GPFSEL4   GPIO_REG(0x10)
#define GPIO_REG_GPFSEL5   GPIO_REG(0x14)
#define GPIO_REG_GPSET0    GPIO_REG(0x1C)
#define GPIO_REG_GPSET1    GPIO_REG(0x20)
#define GPIO_REG_GPCLR0    GPIO_REG(0x28)
#define GPIO_REG_GPCLR1    GPIO_REG(0x2c)
#define GPIO_REG_GPLEV0    GPIO_REG(0x34)
#define GPIO_REG_GPLEV1    GPIO_REG(0x38)
#define GPIO_REG_GPEDS0    GPIO_REG(0x40)
#define GPIO_REG_GPEDS1    GPIO_REG(0x44)
#define GPIO_REG_GPREN0    GPIO_REG(0x4c)
#define GPIO_REG_GPREN1    GPIO_REG(0x50)
#define GPIO_REG_GPFEN0    GPIO_REG(0x58)
#define GPIO_REG_GPFEN1    GPIO_REG(0x5c)
#define GPIO_REG_GPHEN0    GPIO_REG(0x64)
#define GPIO_REG_GPHEN1    GPIO_REG(0x68)
#define GPIO_REG_GPLEN0    GPIO_REG(0x70)
#define GPIO_REG_GPLEN1    GPIO_REG(0x74)
#define GPIO_REG_GPAREN0   GPIO_REG(0x7c)
#define GPIO_REG_GPAREN1   GPIO_REG(0x80)
#define GPIO_REG_GPAFEN0   GPIO_REG(0x88)
#define GPIO_REG_GPAFEN1   GPIO_REG(0x8c)
#define GPIO_REG_GPPUD     GPIO_REG(0x94)
#define GPIO_REG_GPPUDCLK0 GPIO_REG(0x98)
#define GPIO_REG_GPPUDCLK1 GPIO_REG(0x9C)

static inline int gpio_function_to_bits(gpio_function_t f)
{
  const char map[] = {
    [GPIO_FUNCTION_INPUT]  = 0b000,
    [GPIO_FUNCTION_OUTPUT] = 0b001,
    [GPIO_FUNCTION_ALT_0]  = 0b100,
    [GPIO_FUNCTION_ALT_1]  = 0b101,
    [GPIO_FUNCTION_ALT_2]  = 0b110,
    [GPIO_FUNCTION_ALT_3]  = 0b111,
    [GPIO_FUNCTION_ALT_4]  = 0b011,
    [GPIO_FUNCTION_ALT_5]  = 0b010
  };

  ASSERT(f < ARRAY_SIZE(map));
  return map[f];
}

void gpio_set_pin_function(int pin, gpio_function_t function)
{
  ASSERT(pin <= GPIO_MAX_PIN);

  ioreg32_t r = GPIO_REG_GPFSEL0 + (pin / 10);
  int offset = (pin % 10) * 3;
  uint32_t mask = ~(7<<offset);
  uint32_t v = ioreg32_read(r) & mask;
  v |= gpio_function_to_bits(function) << offset;
  ioreg32_write(r, v);
}

static inline int gpio_pullupdown_mode_to_bits(gpio_pullupdown_mode_t mode)
{
  if (mode == GPIO_PULLUPDOWN_MODE_UP)
    return 0b10;
  if (mode == GPIO_PULLUPDOWN_MODE_DOWN)
    return 0b01;
  return 0;
}

static inline void gpio_set_gppudclk(int pin)
{
  /* TODO: provide race protection to gpio resource */
  ioreg32_t r = GPIO_REG_GPPUDCLK0 + pin / 32;
  int offset = pin % 32;

  ioreg32_write(r, 1 << offset);
}

void gpio_set_pin_pullupdown_mode(int pin, gpio_pullupdown_mode_t mode)
{
  /*
   * According to datasheet BCM2835 ARM Peripherals, page 101
   * 1. Write to GPPUD to set the required control signal (i.e.
   * Pull-up or Pull-Down or neither
   * to remove the current Pull-up/down)
   * 2. Wait 150 cycles – this provides the required set-up time
   * for the control signal
   * 3. Write to GPPUDCLK0/1 to clock the control signal into the GPIO pads you wish to
   * modify – NOTE only the pads which receive a clock will be modified, all others will
   * retain their previous state.
   * 4. Wait 150 cycles – this provides the required hold time for the control signal
   * 5. Write to GPPUD to remove the control signal
   * 6. Write to GPPUDCLK0/1 to remove the clock
   *
   */
  volatile int i;
  ASSERT(pin <= GPIO_MAX_PIN);
  ioreg32_write(GPIO_REG_GPPUD, gpio_pullupdown_mode_to_bits(mode));
  for (i = 0; i < 150; ++i)
    asm volatile("nop");

  gpio_set_gppudclk(pin);
  for (i = 0; i < 150; ++i)
    asm volatile("nop");

  ioreg32_write(GPIO_REG_GPPUD, 0);
  ioreg32_write(GPIO_REG_GPPUDCLK0, 0);
  ioreg32_write(GPIO_REG_GPPUDCLK1, 0);
}

bool gpio_pin_is_set(int pin)
{
  ioreg32_t r;

  ASSERT(pin <= GPIO_MAX_PIN);

  r = GPIO_REG_GPLEV0 + pin / 32;

  return (ioreg32_read(r) >> (pin % 32)) & 1;
}

void gpio_set_pin_output(int pin, bool is_set)
{
  ioreg32_t r;

  ASSERT(pin <= GPIO_MAX_PIN);

  r = is_set ? GPIO_REG_GPSET0 : GPIO_REG_GPCLR0;
  r += pin / 32;

  ioreg32_write(r, 1 << (pin % 32));
}

void gpio_toggle_pin_output(int pin)
{
  gpio_set_pin_output(pin, !gpio_pin_is_set(pin));
}

bool gpio_get_reg32_addr(gpio_reg_type_t t, unsigned pin, ioreg32_t *out,
  unsigned *bitmask)
{
  if (pin / 32 > 1)
    return false;

  if (t == GPIO_REG_ADDR_SET) {
    *out = GPIO_REG_GPSET0 + pin / 32;
    *bitmask = 1 << (pin % 32);
    return true;
  }

  if (t == GPIO_REG_ADDR_CLR) {
    *out = GPIO_REG_GPCLR0 + pin / 32;
    *bitmask = 1 << (pin % 32);
    return true;
  }

  return false;
}
