.PHONY: fat32_test

fat32_test:
	gcc main.c fat32.c -Iinclude -g -o $@

d: fat32_test
	gdb -x gdb.gdb fat32_test

all: fat32_test
