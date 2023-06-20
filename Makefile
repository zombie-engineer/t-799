CROSSCOMPILE := /home/user_user/gcc-arm-10.3-2021.07-x86_64-aarch64-none-elf/bin/aarch64-none-elf

# Using fixed version of openocd to support configuration scripts for
# Raspberry PI 3B+, found on the internet
# LID_LIBRARY_PATH is used to find custom build libjaylink library
OPENOCD := openocd

CC := $(CROSSCOMPILE)-gcc
LD := $(CROSSCOMPILE)-ld
OBJDUMP := $(CROSSCOMPILE)-objdump
OBJCOPY := $(CROSSCOMPILE)-objcopy

CFLAGS := -Iinclude -g
OBJS := \
  armv8 \
  mbox/mbox_routines \
  mbox/mbox \
  mbox/mbox_props \
  start \
  stringlib \
  main \
  uart_pl011 \
  gpio

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
