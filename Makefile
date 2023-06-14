CROSSCOMPILE := /home/user_user/gcc-arm-10.3-2021.07-x86_64-aarch64-none-elf/bin/aarch64-none-elf

CC := $(CROSSCOMPILE)-gcc
LD := $(CROSSCOMPILE)-ld
OBJDUMP := $(CROSSCOMPILE)-objdump
OBJCOPY := $(CROSSCOMPILE)-objcopy
$(info $(CC))

OBJS := \
  start \
  main \
  uart_pl011

OBJS := $(addsuffix .o, $(OBJS))

kernel8.bin: kernel8.elf
	$(OBJCOPY) -O binary $< $@

kernel8.elf: $(OBJS) link.ld
	$(LD) $(OBJS) -o $@ -T link.ld -Map=kernel8.map

%.o: %.S
	$(CC) -g -c $? -o $@

%.o: %.c
	$(CC) -g -c $? -o $@

db: kernel8.bin
	$(OBJDUMP) -m aarch64 -b binary -D $^

e: kernel8.bin
	./emulate_pi.sh

d:
	./debug_emulated_pi.sh

clean:
	git clean -xfd
