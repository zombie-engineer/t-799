KERNEL_ADDR_SPACE_SIZE = 512M;
KERNEL_START = 0x80000;
KERNEL_ADDR_SPACE_START = ~((1<<48) - 1);

MEMORY
{
  RAM :
    ORIGIN = 0,
    LENGTH = 0x40000000
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

PAGE_TABLE_SIZE_L3 = 0x40000000 / 4096 * 8;
PAGE_TABLE_SIZE_L2 = MAX(PAGE_TABLE_SIZE_L3 / 4096 * 8, 1);
PAGE_TABLE_SIZE_L1 = MAX(PAGE_TABLE_SIZE_L2 / 4096 * 8, 1);
PAGE_TABLE_SIZE_L0 = MAX(PAGE_TABLE_SIZE_L1 / 4096 * 8, 1);
PAGE_TABLE_SIZE = PAGE_TABLE_SIZE_L0
  + PAGE_TABLE_SIZE_L1
  + PAGE_TABLE_SIZE_L2
  + PAGE_TABLE_SIZE_L3 + 0x1000;

PAGE_TABLE_SIZE = ALIGN(PAGE_TABLE_SIZE, 0x1000);

  .pagetable : {
    . = ALIGN(4096);
    __pagetable_start = .;
    . += PAGE_TABLE_SIZE;
    __pagetable_end = .;
  } > RAM

  .data_nommu : {
  } > RAM

  .bss_nommu : {
  } > RAM

  VMA = KERNEL_ADDR_SPACE_START + __pagetable_end;

  .text VMA : {
    start.o(.start_el1_mmu)
    *.o(.text)
  } >VIRTUAL_MEM AT>RAM

  VMA += SIZEOF(.text);

  .data VMA : {
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

  . = ALIGN(4096);
  .kernel_memory : {
    kmalloc.o(.kernel_memory)
  } > VIRTUAL_MEM
  __kernel_memory_start = ADDR(.kernel_memory);
  __kernel_memory_end = __kernel_memory_start + SIZEOF(.kernel_memory);
}
