CROSSCOMPILE := /home/user_user/gcc-arm-10.3-2021.07-x86_64-aarch64-none-elf/bin/aarch64-none-elf

# Using fixed version of openocd to support configuration scripts for
# Raspberry PI 3B+, found on the internet
# LID_LIBRARY_PATH is used to find custom build libjaylink library
OPENOCD := openocd

CC := $(CROSSCOMPILE)-gcc
LD := $(CROSSCOMPILE)-ld
OBJDUMP := $(CROSSCOMPILE)-objdump
OBJCOPY := $(CROSSCOMPILE)-objcopy

CFLAGS := -Iinclude -g -Werror
# Required to implement standard library functions like sprintf and memset
CFLAGS += -ffreestanding

OBJS := \
  armv8 \
  armv8_cpu_context \
  armv8_exception \
  armv8_exception_sync \
  armv8_exception_vector \
  bcm2835_systimer \
  bcm2835_ic \
  debug_led \
  delay \
  irq \
  mbox/mbox_routines \
  mbox/mbox \
  mbox/mbox_props \
  panic \
  start \
  sprintf \
  stringlib \
  sched \
  task \
  main \
  uart_pl011 \
  gpio \
  self_test_context_switch \
  spi

OBJS := $(addsuffix .o, $(OBJS))

kernel8.bin: kernel8.elf
	$(OBJCOPY) -O binary $< $@

kernel8.elf: $(OBJS) link.ld
	$(LD) $(OBJS) -o $@ -T link.ld -Map=kernel8.map

%.o: %.S
	$(CC) -g -c $? -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $? -o $@

db: kernel8.bin
	$(OBJDUMP) -m aarch64 -b binary -D $^

dbe: kernel8.elf
	$(OBJDUMP) -D $^

e: kernel8.bin
	./emulate_pi.sh

j:
	@echo "Connecting to Raspberry PI 3B+ over JTAG"
	$(OPENOCD) -f openocd-raspberrypi3.cfg

de:
	@echo "Starting session to debug emulated Raspberry PI 3B+"
	./debug_emulated_pi.sh

dj:
	@echo "Starting session to debug emulated Raspberry PI 3B+"
	./debug_pi_jtag.sh

clean:
	git clean -xfd

help:
	@echo j  - run OpenOCD to attach Raspberry PI via JTAG and wait for GDB connection
	@echo dj - start GDB to debug Raspberry PI over JTAG
	@echo e: - run QEMU and wait for GDB connection
	@echo de - start GDB to debug Raspberry PI in QEMU
