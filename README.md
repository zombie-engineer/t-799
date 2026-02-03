# T-799

### Real-Time Operating System On Raspberry PI
**Architecture: ARMv8**

**Target board: Raspberry Pi 3B+ (BCM2837)**

---

## Overview

**T-799** is a real-time operating system for Raspberry Pi that runs on the ARM part of the soc

The project implements a complete kernel environment without Linux, userspace, or vendor runtime libraries. All system services are written explicitly and run entirely in kernel space.

A fully functional **camera → ISP → encoder → display** pipeline is used as a validation workload to prove correctness, performance, and real-time behavior of the OS.

---

## Design Goals

- Explicit control of hardware with no hidden abstractions
- Deterministic behavior under sustained load
- Correct interaction between interrupts, DMA, and scheduling
- Validation through a demanding real-world workload (camera pipeline)

---

## System Architecture

### Hardware Target

- Raspberry Pi 3B+
- BCM2837 (ARM Cortex-A53, AArch64)
- VideoCore IV
- CSI camera
- SPI-connected ILI9341 display

---

### Kernel Capabilities

- Single CPU only
- Interrupt handling
- Context switching
- Task scheduling, based on time slice and cooperative multitasking.
- Basic MMU for identity mapping and cache control
- BCM2835 DMA
- SDHost driver supports high-speed writes to sdcard
- VCHIQ driver (VideoCore interface)
- MMAL (VideoCore Multi-Media Abstraction Layer)
- Dynamic memory allocation
---

## Camera and Video Pipeline

### Camera Stack

- Direct interaction with VideoCore MMAL services
- No Linux MMAL, no userspace components
- Custom implementation of MMAL ports, buffers, and connections


---
### Building SD card

Clone his repo:  
https://github.com/raspberrypi/firmware.git  
At least this commit worked 5c83250276c4fa9d79a93ebc5a739a5ab6a4d6a7  
In the repo go to boot dir and copy these files  
- start_x.elf  
- fixup_x.dat  
- bootcode.bin  
All files that should be present:  
bootcode.bin - from repo above - this is VideoCore's bootloader  
fixup_x.dat  - from repo above - fixups? with save suffix as main firmware start_x.elf  
start_x.elf  - from repo above - main VideoCore's firmware , 'x' means support camera stuff  
kernel8.img  - binary that we are building here, (here it is named kernel8.bin and should be renamed to kernel8.img)  
cmdline.txt  - firmware, that starts kernel on arm will put contents of this file as kernel arguments  
config.txt   - config contents, very important, will be listed below  
UART         - just an empty file, bootcode.bin checks for its presence to turn on debug logs  

---
#### Contents of config.txt:

start_x=1  
enable_uart=1  
uart_2ndstage=1  
arm_64bit=1  
enable_jtag_gpio=1  
dtoverlay=pi3-disable-bt  
dtoverlay=sdhost  
dtparam=sdhci=off  
init_uart_baud=115200  

---
#### Context of MBR that worked:  

Units: sectors of 1 * 512 = 512 bytes  
Sector size (logical/physical): 512 bytes / 512 bytes  
I/O size (minimum/optimal): 512 bytes / 512 bytes  
Disklabel type: dos  
Disk identifier: 0x99873050  

Device     Boot   Start      End  Sectors  Size Id Type  
/dev/sda1          8192  1056767  1048576  512M  c W95 FAT32 (LBA)  
/dev/sda2       1056768 15564799 14508032  6,9G 83 Linux  



## JTAG support
This section describes steps to take to be able to debug binary execution  
on Raspberry PI 3B+. Below is a list of general steps to make it work:  
1. Get any proven JTAG debugging device.  
2. Ensure proper pin connection between a JTAG debugger and your Raspberry PI  
3. Install or build openocd, that supports your JTAG debugger  
4. Install or build gdb, that supports aarch64 debugging  
5a. Add parameter "enable_jtag_gpio=1" to config.txt file on SDcard or  
5b. OR program GPIO pins to JTAG mode (ALT_4 function) in the firmware itself  
    (5a is prefferable)  
6. Run openocd with proper configuration - proper JTAG driver is chosen and  
   all access points, debug ports and what not had proper values  
  
  
List of JTAG debugger devices, that worked for me:  
- OLYMEX ARM JTAG  
- SEGGER J-Link JTAG  
  
  
Building OpenOCD:  
  For connecting to Raspberry PI via JTAG, it may be useful to build openocd.  
  Build configuration options will depend on JTAG device. Here I describe  
  steps for J-Link emulator:  
    git clone https://github.com/sysprogs/openocd.git  
    cd openocd  
    ./bootstrap  
    ./configure --enable-jlink --enable-internal-libjaylink --enable-verbose  
    make -j12  
    sudo make install  
    
  By default openocd binary is installed in /usr/local/bin/openocd  
  
  
What pins should be connected:  
  - In "BCM2835 ARM Peripherals" document (the one that has most details about  
    Raspberry PI) see page 102 with mapping of GPIO pins to alternative  
    functions.  
  - There you can see a range of GPIOs starting from GPIO22 up to GPIO27 that  
    have ARM JTAG functions under column ALT4. That means that these 6 gpio  
    pins, namely 22,23,24,25,26,27 should all be configured with function ALT4  
    in respective GPFSELx register.  
  - Look for pinout pictures for Raspberry PI on the internet together with  
    ARM JTAG pin layout and use them to properly wire these two devices  
    together.  
  - Additional pins that you have to connect:  
    - GND pin - any GND on JTAG debugger to any GND on Raspberry PI  
    - Voltage pin - VCC pin on debugger to pin #1 (3.3v DC power) pin on a  
      a Raspberry PI  
  - In total you will have 6 JTAG pins + GND pin + VCC pin connected = 8 wires  
  
Running openocd:  
  Checkout openocd-raspberrypi3.cfg file in this repository. It enlists all the  
  required commands to configure Debug Access Port (DAP), as well as create  
  CPU target descriptors. I will update the manual as soon as I know more myself,  
  But in general if you run "openocd -f openocd-raspberrypi3.cfg" and all of  
  the above is done properly, you will have a functioning openocd JTAG  
  communication with you Raspberry PI.  
  If all worked well, opencod will outputs something like:  
    "Info : VTarget = 3.280 V                                        "  
    "Info : clock speed 1000 kHz                                     "  
    "Info : JTAG tap: bcm2837.cpu tap/device found: 0x4ba00477 ...   "  
    "Info : bcm2837.cpu.0: hardware has 6 breakpoints, 4 watchpoints "  
    "Info : bcm2837.cpu.1: hardware has 6 breakpoints, 4 watchpoints "  
    "Info : bcm2837.cpu.2: hardware has 6 breakpoints, 4 watchpoints "  
    "Info : bcm2837.cpu.3: hardware has 6 breakpoints, 4 watchpoints "  
  
  Also watch for these lines:  
    "Info : Listening on port 4444 for telnet connections"  
    and  
    "Info : starting gdb server for bcm2837.cpu.0 on 3333"  
    To know which ports you can attach to  
  
  Raw openocd command operation inolves running telnet:  
  "telnel localhost 4444" and then study output of "help" command  
  But in general more straightforward debugging is to use gdb with aarch64  
  support. You can find gdb in crosscompilation package or install gdb-multiarch.  
  then run gdb with symbols:  
    "gdb-multiarch kernel8.img"  
  then attach to openocd gdb port:  
    "target remote :3333"  
  
Good related material to read:  
- Great explanation about Aarch64 exception model in good details  
  https://krinkinmu.github.io/2021/01/10/aarch64-interrupt-handling.html  
