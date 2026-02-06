CORE_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= $(abspath $(PWD))
$(info t-799 Makefile)

ifndef APP_ROOT
$(error APP_ROOT not defined)
endif

ifndef APP_OBJS
$(error APP_OBJS not defined)
endif

include t-799/make/toolchain.mk
include t-799/make/common.mk

include t-799/src/Makefile
include src/Makefile

CORE_INCLUDES := -I$(CORE_ROOT)/include
CFLAGS += $(CORE_INCLUDES)
ASFLAGS += $(CORE_INCLUDES)

CORE_OBJS := $(addprefix $(BUILD_DIR)/t-799/src/, $(CORE_OBJS))
APP_OBJS := $(addprefix $(BUILD_DIR)/src/, $(APP_OBJS))
OBJS := $(addsuffix .o, $(CORE_OBJS) $(APP_OBJS))
OBJS_BUILD_REL := $(subst $(BUILD_DIR)/,, $(OBJS))
LINKER_SCRIPT_PATH := $(CORE_ROOT)/src/link.ld

include t-799/make/rules.mk

#.PHONY: os.json


# Using fixed version of openocd to support configuration scripts for
# Raspberry PI 3B+, found on the internet
# LID_LIBRARY_PATH is used to find custom build libjaylink library
OPENOCD := openocd

#ddb:
#	$(OBJDUMP) -d $(BUILDDIR)/start.o
#
#db: $(BUILDDIR)/kernel8.bin
#	$(OBJDUMP) -z -m aarch64 -b binary -D $^
#
#dbe: $(BUILDDIR)/kernel8.elf
#	$(OBJDUMP) -D $^
#
#e: $(BUILDDIR)/kernel8.bin
#	./scripts/gdb/emulate_pi.sh
#
#j:
#	@echo "Connecting to Raspberry PI 3B+ over JTAG"
#	$(OPENOCD) -f openocd-raspberrypi3.cfg

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
