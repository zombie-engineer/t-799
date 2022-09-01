CROSSCOMPILE= /home/user-user/crosscompile/gcc-arm-10.3-2021.07-x86_64-aarch64-none-elf/bin/aarch64-none-elf

CC := $(CROSSCOMPILE)-gcc
LD := $(CROSSCOMPILE)-ld
OBJDUMP := $(CROSSCOMPILE)-objdump
OBJCOPY := $(CROSSCOMPILE)-objcopy
$(info $(CC))

OBJS := start.o kernel.o uart_pl011.o

kernel.bin: kernel.elf
	$(OBJCOPY) -O binary $< $@

kernel.elf: $(OBJS) link.ld
	$(LD) $(OBJS) -o $@ -T link.ld -Map=kernel.map

start.o: start.S
	$(CC) -g -c $? -o $@

kernel.o: kernel.c
	$(CC) -g -c $? -o $@

db: kernel.bin
	$(OBJDUMP) -m aarch64 -b binary -D $^

e: kernel.bin
	./emulate_pi.sh

d:
	./debug_emulated_pi.sh

clean:
	git clean -xfd
