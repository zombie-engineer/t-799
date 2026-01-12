CROSSCOMPILE := aarch64-none-elf

# Using fixed version of openocd to support configuration scripts for
# Raspberry PI 3B+, found on the internet
# LID_LIBRARY_PATH is used to find custom build libjaylink library
OPENOCD := openocd

CC := $(CROSSCOMPILE)-gcc
LD := $(CROSSCOMPILE)-ld
OBJDUMP := $(CROSSCOMPILE)-objdump
OBJCOPY := $(CROSSCOMPILE)-objcopy
BUILDDIR := build
SRCDIR := $(PWD)/src
$(info $(SRCDIR))

CFLAGS := -Iinclude -g -Werror -Wall
# Required to implement standard library functions like sprintf and memset
CFLAGS += -ffreestanding

include src/Makefile
CFILES := $(addprefix src/, $(addsuffix .c, $(OBJS)))
OBJS_REL := $(addsuffix .o, $(OBJS))
OBJS := $(addprefix build/, $(addsuffix .o, $(OBJS)))
OBJ_DIRS := $(sort $(dir $(OBJS)))

$(BUILDDIR)/kernel8.bin: $(BUILDDIR)/kernel8.elf $(BUILDDIR)/os.json
	$(OBJCOPY) -O binary $< $@

.PHONY: .force
.force: $(OBJS) $(OBJ_DIRS)

$(BUILDDIR)/kernel8.elf: $(OBJ_DIRS) $(OBJS) src/link.ld
	cd $(BUILDDIR) && $(LD) $(OBJS_REL) -o kernel8.elf -T $(SRCDIR)/link.ld -Map=kernel8.map -print-memory-usage

$(BUILDDIR)/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/%.o: src/%.S
	$(CC) -Iinclude -g -c $< -o $@

$(OBJ_DIRS):
	@echo Recepie for $(OBJ_DIRS)
	mkdir -p $(OBJ_DIRS)

.PHONY: os.json

$(BUILDDIR)/task_offsets.o: src/task_offsets.c .force
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/task_offsets.bin: $(BUILDDIR)/task_offsets.o
	$(OBJCOPY) -O binary --only-section .task_struct_layout $< $@

$(BUILDDIR)/os.json: $(BUILDDIR)/kernel8.elf $(BUILDDIR)/task_offsets.bin
	. scripts/gen_gdb_json.sh > $@
	cat $@

ddb:
	$(OBJDUMP) -d $(BUILDDIR)/start.o

db: $(BUILDDIR)/kernel8.bin
	$(OBJDUMP) -z -m aarch64 -b binary -D $^

dbe: $(BUILDDIR)/kernel8.elf
	$(OBJDUMP) -D $^

e: $(BUILDDIR)/kernel8.bin
	./scripts/gdb/emulate_pi.sh

j:
	@echo "Connecting to Raspberry PI 3B+ over JTAG"
	$(OPENOCD) -f openocd-raspberrypi3.cfg

de:
	@echo "Starting session to debug emulated Raspberry PI 3B+"
	./scripts/gdb/debug_emulated_pi.sh

dj:
	@echo "Starting session to debug emulated Raspberry PI 3B+"
	./scripts/gdb/debug_pi_jtag.sh

dja:
	@echo "Starting session to debug emulated Raspberry PI 3B+"
	./scripts/gdb/debug_pi_jtag.sh --attach

gitclean:
	git clean -xfd

clean:
	@rm -v $(OBJS) 2>/dev/null || true

help:
	@echo j  - run OpenOCD to attach Raspberry PI via JTAG and wait for GDB connection
	@echo dj - start GDB to debug Raspberry PI over JTAG
	@echo e - run QEMU and wait for GDB connection
	@echo de - start GDB to debug Raspberry PI in QEMU
	@echo db - Disassemble raw binary file
	@echo dbe - Disassemble elf file
	@echo clean - cleans project from *.o files
