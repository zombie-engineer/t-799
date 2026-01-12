#include <delay.h>
#include <cpu.h>

/* delay for n microseconds */
void delay_us(uint32_t n)
{
  uint64_t counter, counter_target;
  uint32_t counter_freq, wait_counts;

  counter = read_cpu_counter_64();
  counter_freq = get_cpu_counter_64_freq();
  wait_counts = (counter_freq / 1000000) * n;

  counter_target = counter + wait_counts;
  while(read_cpu_counter_64() < counter_target);
}
