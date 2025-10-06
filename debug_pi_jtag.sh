#!/bin/bash
GDB=gdb-multiarch
GDB=/home/user_user/gcc-arm-10.3-2021.07-x86_64-aarch64-none-elf/bin/aarch64-none-elf-gdb
GDB_SCRIPT=jtag.gdb
# if [ $# -ge 1 -a $1 = '--attach' ] ; then
#  GDB_SCRIPT=jtag_attach.gdb
# fi

$GDB -q -x $GDB_SCRIPT -s kernel8.elf
