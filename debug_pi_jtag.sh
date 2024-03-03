#!/bin/bash

GDB_SCRIPT=jtag.gdb
if [ $# -ge 1 -a $1 = '--attach' ] ; then
  GDB_SCRIPT=jtag_attach.gdb
fi

gdb-multiarch -q -x $GDB_SCRIPT -s kernel8.elf
