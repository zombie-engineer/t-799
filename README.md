[JTAG support]
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
