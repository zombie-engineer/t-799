#!/bin/bash

QEMU=qemu-system-aarch64
QEMU=/home/user-user/git/qemu/build/qemu-system-aarch64
RUNCMD=$QEMU
echo DEBUG_QEMU=$DEBUG_QEMU
if [ -v DEBUG_QEMU ]; then
	RUNCMD="gdb --args $QEMU"
fi

echo $RUNCMD
$RUNCMD -kernel kernel.bin\
	-machine raspi3b \
	-nographic \
	-serial stdio \
	-nodefaults \
	-s -S

