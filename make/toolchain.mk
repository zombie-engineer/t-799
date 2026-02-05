CROSSCOMPILE := aarch64-none-elf
CC := $(CROSSCOMPILE)-gcc
LD := $(CROSSCOMPILE)-ld
OBJDUMP := $(CROSSCOMPILE)-objdump
OBJCOPY := $(CROSSCOMPILE)-objcopy

CFLAGS := -g -Werror -Wall
# Required to implement standard library functions like sprintf and memset
CFLAGS += -ffreestanding

