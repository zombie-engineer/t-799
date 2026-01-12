#!/bin/bash
GDB=gdb-multiarch
GDB=aarch64-none-elf-gdb
GDB_SCRIPT=scripts/gdb/jtag.gdb
# if [ $# -ge 1 -a $1 = '--attach' ] ; then
#  GDB_SCRIPT=jtag_attach.gdb
# fi

$GDB -q -x $GDB_SCRIPT -s build/kernel8.elf
