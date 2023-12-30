KERNEL_ADDR_SPACE_SIZE = 512M;
KERNEL_START = 0x80000;
KERNEL_ADDR_SPACE_START = ~((1<<48) - 1);

MEMORY
{
  RAM :
    ORIGIN = 0,
    LENGTH = 0x100000
  VIRTUAL_MEM :
    ORIGIN = KERNEL_ADDR_SPACE_START,
    LENGTH = KERNEL_ADDR_SPACE_SIZE
}

REGION_ALIAS("REGION_TEXT", RAM);

SECTIONS
{
  __bootloader = .;
  .bootloader (NOLOAD) : {
    . += 0x2000;
  } > RAM

  .init.nommu KERNEL_START : {
    start.o(.init.nommu)
    *.o(.init.nommu)
    __stack_no_mmu_top = ALIGN(0x10);
    . += 0x400;
    . = ALIGN(0x10);
    __stack_no_mmu_base = .;
  } > RAM

  .exception_vector : {
    *.o(.exception_vector)
  } > RAM

  .pagetable : {
    . = ALIGN(4096);
    __pagetable_start = .;
    . += 0x10000;
    __pagetable_end = .;
  } > RAM

  .data_nommu : {
  } > RAM

  .bss_nommu : {
  } > RAM

  .text KERNEL_ADDR_SPACE_START + 0x93000 : {
    start.o(.start_el1_mmu)
    *.o(.text)
  } >VIRTUAL_MEM AT>RAM

  .data : {
    data_start = .;
    *.(.data)
  } >VIRTUAL_MEM AT>RAM

  .rodata : {
    *.(.rodata)
  } >VIRTUAL_MEM AT>RAM

  .bss (NOLOAD) : {
    __bss_start = .;
    *(.bss)
    *(COMMON)
    __bss_end = .;
  } > VIRTUAL_MEM

  . = ALIGN(0x10);
  .stacks : {
    *.o(.stacks)
  } > VIRTUAL_MEM
}
