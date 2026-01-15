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

  PHY_ADDR = KERNEL_START;

  .init.nommu PHY_ADDR : AT(PHY_ADDR) {
    kernel/start.o(.init.nommu)
    *.o(.init.nommu)
    __stack_no_mmu_top = ALIGN(0x10);
    . += 0x400;
    . = ALIGN(0x10);
    __stack_no_mmu_base = .;
  } > RAM

  PHY_ADDR += SIZEOF(.init.nommu);

  .exception_vector PHY_ADDR : AT(PHY_ADDR) {
    *.o(.exception_vector)
  } > RAM
  PHY_ADDR += SIZEOF(.exception_vector);

PAGE_TABLE_SIZE_L3 = 0x40000000 / 4096 * 8;
PAGE_TABLE_SIZE_L2 = MAX(PAGE_TABLE_SIZE_L3 / 4096 * 8, 1);
PAGE_TABLE_SIZE_L1 = MAX(PAGE_TABLE_SIZE_L2 / 4096 * 8, 1);
PAGE_TABLE_SIZE_L0 = MAX(PAGE_TABLE_SIZE_L1 / 4096 * 8, 1);
PAGE_TABLE_SIZE = PAGE_TABLE_SIZE_L0
  + PAGE_TABLE_SIZE_L1
  + PAGE_TABLE_SIZE_L2
  + PAGE_TABLE_SIZE_L3 + 0x1000;

PAGE_TABLE_SIZE = ALIGN(PAGE_TABLE_SIZE, 0x1000);

  .pagetable PHY_ADDR (NOLOAD) : AT(PHY_ADDR) {
    . = ALIGN(4096);
    __pagetable_start = .;
    . += PAGE_TABLE_SIZE;
    __pagetable_end = .;
  } > RAM

  PHY_ADDR += SIZEOF(.pagetable);

  .bss_nommu PHY_ADDR (NOLOAD) : AT(PHY_ADDR) {
    *.o(.bss_nommu)
  } > RAM

  PHY_ADDR += SIZEOF(.bss_nommu);

  .data_nommu PHY_ADDR : AT(PHY_ADDR) {
    *.o(.data_nommu)
  } > RAM

  PHY_ADDR += SIZEOF(.data_nommu);

  .rodata_nommu PHY_ADDR : AT(PHY_ADDR) {
    *.o(.rodata_nommu)
  } > RAM

  PHY_ADDR += SIZEOF(.rodata_nommu);

  .text KERNEL_ADDR_SPACE_START + PHY_ADDR : AT(PHY_ADDR) {
    __text_start = .;
    kernel/start.o(.start_el1_mmu)
    *.o(.text)
    __text_end = .;
    kernel/dma_memory.o(.text)
  }

  PHY_ADDR += SIZEOF(.text);

  .data KERNEL_ADDR_SPACE_START + PHY_ADDR : AT(PHY_ADDR) {
    data_start = .;
    *.(.data)
  }
  PHY_ADDR += SIZEOF(.data);

  .rodata KERNEL_ADDR_SPACE_START + PHY_ADDR : AT(PHY_ADDR) {
    *.o(.rodata*)
  }
  PHY_ADDR = ALIGN(PHY_ADDR + SIZEOF(.rodata), 16);

  .bss  KERNEL_ADDR_SPACE_START + PHY_ADDR (NOLOAD) : AT(PHY_ADDR) {
    __bss_start = .;
    *(.bss)
    *(COMMON)
    __bss_end = ALIGN(16);
  }
  __bss_start_phys = __bss_start - KERNEL_ADDR_SPACE_START;
  __bss_end_phys = __bss_end - KERNEL_ADDR_SPACE_START;
  PHY_ADDR += SIZEOF(.bss);
  PHY_ADDR = ALIGN(PHY_ADDR, 0x10);

  .stacks KERNEL_ADDR_SPACE_START + PHY_ADDR (NOLOAD) : AT(PHY_ADDR) {
    __stacks_start = .;
    *.o(.stacks)
    __stacks_end = .;
  }

  PHY_ADDR += SIZEOF(.stacks);
  PHY_ADDR = ALIGN(PHY_ADDR, 4096);
  .kernel_memory KERNEL_ADDR_SPACE_START + PHY_ADDR (NOLOAD) : AT(PHY_ADDR) {
    kernel/kmalloc.o(.kernel_memory)
  }
  __kernel_memory_start = ADDR(.kernel_memory);
  __kernel_memory_end = __kernel_memory_start + SIZEOF(.kernel_memory);

  PHY_ADDR += SIZEOF(.kernel_memory);
  PHY_ADDR = ALIGN(PHY_ADDR, 4096);
  .dma_memory KERNEL_ADDR_SPACE_START + PHY_ADDR (NOLOAD) : AT(PHY_ADDR) {
    kernel/dma_memory.o(.dma_memory)
  }

  __dma_memory_start = ADDR(.dma_memory);
  __dma_memory_end = __dma_memory_start + SIZEOF(.dma_memory);
  __dma_memory_start_phys = __dma_memory_start - KERNEL_ADDR_SPACE_START;
  __dma_memory_end_phys = __dma_memory_end - KERNEL_ADDR_SPACE_START;
  __dma_memory_size = SIZEOF(.dma_memory);
}
