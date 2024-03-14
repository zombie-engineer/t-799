#!/bin/bash

QEMU=${QEMU_PATH:-qemu-system-aarch64}
RUNCMD=$QEMU
if [ -v DEBUG_QEMU ]; then
	RUNCMD="gdb -silent -x debug_qemu.gdb --args $QEMU"
fi

TRACE_ARGS=
# TRACE_ARGS="-trace 'bcm2835_*'"

echo $RUNCMD
$RUNCMD -kernel kernel8.bin\
	-machine raspi3b \
	-nographic \
	$TRACE_ARGS \
	-serial stdio \
	-nodefaults \
	-s -S

#	-monitor stdio \
