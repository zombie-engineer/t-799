#!/bin/bash

qemu-system-aarch64 -kernel kernel.bin\
	-machine raspi3b \
	-nographic \
	-serial stdio \
	-s -S
