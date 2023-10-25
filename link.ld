SECTIONS
{
  . = 0x0;
  __bootloader = .;
  . = 0x80000;
  .text : {
    *.o(.text.startup)
    *.o(.text)
  }
  .exception_vector : {
    *.o(.exception_vector)
  }

  . = 0x90000;
  . = ALIGN(16);
  .stacks : {
    *.o(*stacks)
  }
}
