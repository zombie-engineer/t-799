#include <stdint.h>
#include <mmu.h>
#include <common.h>
#include <stddef.h>
#include <stdbool.h>

#define MMU_GRANULE_4K 4096
#define MMU_GRANULE_16K (1024 * 16)
#define MMU_GRANULE_64K (1024 * 64)

#define MMU_PAGE_GRANULE MMU_GRANULE_4K
#define NO_MMU __attribute__((section(".init.nommu")))

/*
 * We setup 4096 bytes GRANULE.
 *
 * - Any long-format translation table entry always has 8 bytes size.
 * - Any translation table should fit into page of GRANULE size (4096)
 * - For each table level next 9 bits of VA are taken from the right to
 *   get index of table entry (one of 512 entries in table)
 * - Last table points
 * bit math: 
 * 4096 addresses from 0 to 4095 are covered by 12 bits
 * 512  addresses from 0 to 511  are covered by 9  bits
 * 
 *              L0      L1       L2       L3                           
 * VA [63 : 48][47 : 39][38 : 30][29 : 21][20 : 12][11 : 0]
 *             |        |        |        |                \
 *             |        |        |        |                 \_
 *             |        |        |        |                   \_
 *             |        |        |        |                     \_
 *             |        |        |        |                       \_
 *             |        |        |        |                         \
 *             |        |        |     (table 3)[x]->(page address)+([11:0]offset)
 *             |        |        |
 *             |        |       (table 2)[x]->(table 3)[x]
 *             |        |
 *             |      (table 1)[x]->(table 2)[x]
 *             |
 *          (table 0)[x] -> (table 1)[x]
 *
 *
 *
 * num entries in table = 2**(n - 3) = 2 ** 9 = 512 (3bits - are 8byte alignement)
 * 4096 - 2**12
 * OA[11:0] = IA[11:0]
 * 48 - 11 = 37
 * IA[47:12] 
 * 8-bytes sized descriptor may be addressed by index 
 * in page address   resolves IA[(n-1):0]        = IA[11:0]
 * last lookup level resolves IA[(2n-4):n]       = IA[20:12]
 * prev lookup level resolves IA[(3n-7):(2n-3)]  = IA[29:21]
 * prev lookup level resolves IA[(4n-10):(3n-6)] = IA[38:30]
 * prev lookup level resolves IA[(5n-13):(4n-9)] = IA[47:39]
 */


/*
 * MAP a range of 32 megabytes
 * map range 32Mb
 * 32Mb = 32 * 1024 * 1024 = 8 * 4 * 1024 * 1024 = 8 * 1024 * 4096 
 * = 8192 x 4K pages 
 * = 8192 l3 pte's
 * 8192 l3 pte's = 16 * 512 
 * = 16 l2 pte's
 * =  1 l1 pte's
 * =  1 l0 pte's

 * MAP a range of 256 megabytes
 * 256MB = 256 * 1024 * 1024 = 64 * 1024 * 4K
 * = 65536 l3 pte's
 * = 128   l2 pte's
 * = 1     l1 pte
 * = 1     l0 pte

 * MAP a range of 1 gigabytes
 * 1024Mb = 1024 * 1024 * 1024 = 256 * 1024 * 4K
 * = 262144 l3 pte's
 * = 512    l2 pte's
 * = 1      l1 pte
 * = 1      l0 pte
 */
extern char __pagetable_start;
extern char __pagetable_end;

NO_MMU void mmu_map_range(uint64_t vaddr_start, uint32_t paddr_start,
  size_t size, uint64_t *page_table_base, uint64_t *page_table_end)
{
  int i;
  uint64_t desc;

  int num_pages = size / MMU_PAGE_GRANULE;
  int num_l3_ptes = MAX(num_pages, 1);
  int num_l2_ptes = MAX((num_l3_ptes * 8) / MMU_PAGE_GRANULE, 1);
  int num_l1_ptes = MAX((num_l2_ptes * 8) / MMU_PAGE_GRANULE, 1);
  int num_l0_ptes = MAX((num_l1_ptes * 8) / MMU_PAGE_GRANULE, 1);

#define NUM_DESCRIPTOR_PAGES(__num_descriptors) \
  ((__num_descriptors * 8 + (MMU_PAGE_GRANULE - 1)) / MMU_PAGE_GRANULE);

  int num_l0_pages = NUM_DESCRIPTOR_PAGES(num_l0_ptes);
  int num_l1_pages = NUM_DESCRIPTOR_PAGES(num_l1_ptes);
  int num_l2_pages = NUM_DESCRIPTOR_PAGES(num_l2_ptes);
  int num_l3_pages = NUM_DESCRIPTOR_PAGES(num_l3_ptes);

  uint64_t *l0_pte_base = page_table_base;
  uint64_t *l1_pte_base = l0_pte_base + num_l0_pages * MMU_PAGE_GRANULE / 8;
  uint64_t *l2_pte_base = l1_pte_base + num_l1_pages * MMU_PAGE_GRANULE / 8;
  uint64_t *l3_pte_base = l2_pte_base + num_l2_pages * MMU_PAGE_GRANULE / 8;
  uint64_t *page_table_real_end = l3_pte_base + num_l3_ptes;

  if (page_table_real_end > page_table_end)
    while(1);

  for (i = 0; i < num_l0_ptes; ++i) {
    desc = ((uint64_t)l1_pte_base + 512 * i) | 3;
    l0_pte_base[i] = desc;
  }

  for (i = 0; i < num_l1_ptes; ++i) {
    desc = ((uint64_t)l2_pte_base + 512 * i) | 3;
    l1_pte_base[i] = desc;
  }

  for (i = 0; i < num_l2_ptes; ++i) {
    desc = ((uint64_t)l3_pte_base + 512 * i) | 3;
    l2_pte_base[i] = desc;
  }

  for (i = 0; i < num_l3_ptes; ++i) {
    desc = (paddr_start + i * 4096) | 3 | (1<<10);
    l3_pte_base[i] = desc;
  }
}

typedef enum {
  /* 4GB */
  NUM_PADDR_BITS_32,
  /* 64GB */
  NUM_PADDR_BITS_36,
  /* 1TB */
  NUM_PADDR_BITS_40,
  /* 4TB */
  NUM_PADDR_BITS_42,
  /* 16TB */
  NUM_PADDR_BITS_44,
  /* 256TB */
  NUM_PADDR_BITS_48,
  /* 4PB */
  NUM_PADDR_BITS_52
} paddr_bits_t;

NO_MMU void mmu_program_tcr(int address_size0, int address_size1)
{
  int sz0 = 64 - address_size0;
  int sz1 = 64 - address_size1;

  int sz0_min = 16;
  int sz1_min = 16;

  int granule_size_bits =
#if (MMU_PAGE_GRANULE == MMU_GRANULE_4K)
    0b00
#elif (MMU_PAGE_GRANULE == MMU_GRANULE_16K)
    0b01
#elif (MMU_PAGE_GRANULE == MMU_GRANULE_64K)
    0b10
#endif
  ;

  int ds_bit = 0;
#if (MMU_PAGE_GRANULE == MMU_GRANULE_4K || MMU_PAGE_GRANULE == MMU_GRANULE_16K)
  if (address_size0 > 48) {
    ds_bit = 1;
    sz0_min = 12;
    sz1_min = 12;
  }
#endif

  if (sz0 < sz0_min || sz1 < sz1_min)
    while(1);

  /* Program TG0,TG1 bits */
  asm volatile (
    "mrs x2, tcr_el1\n"
    "bic x2, x2, #(3 << 30)\n"
    "lsl x1, %0, #30\n"
    "orr x2, x2, %0\n"
    "bic x2, x2, #(3 << 14)\n"
    "lsl x1, %0, #14\n"
    "orr x2, x2, %0\n"
    "msr tcr_el1, x2\n"
    :: "r"(granule_size_bits)
  );

  /* Program DS bit */
  asm volatile (
    "mrs x2, tcr_el1\n"
    "bic x2, x2, #(1 << 59)\n"
    "lsl %0, %0, #59\n"
    "orr x2, x2, %0\n"
    "msr tcr_el1, x2\n"
    :: "r"(ds_bit)
  );

  /* Program sz0 */
  asm volatile (
    "mrs x2, tcr_el1\n"
    "bic x2, x2, #(0x3f << 0)\n"
    "orr x2, x2, %0\n"
    "msr tcr_el1, x2\n"
    :: "r"(sz0)
  );

  /* Program sz1 */
  asm volatile (
    "mrs x2, tcr_el1\n"
    "bic x2, x2, #(0x3f << 16)\n"
    "lsl %0, %0, 16\n"
    "orr x2, x2, %0\n"
    "msr tcr_el1, x2\n"
    :: "r"(sz1)
  );
}

NO_MMU bool armv8_impl_has_feature_lpa2(void)
{
  int id_aa64mmfr0_tgran_4;
  asm volatile(
    "mrs %0, id_aa64mmfr0_el1\n"
    "lsr %0, %0, #28\n"
    "and %0, %0, #0xf\n" : "=r"(id_aa64mmfr0_tgran_4)
  );
  return id_aa64mmfr0_tgran_4 == 1;
}

NO_MMU bool armv8_impl_has_feature_lva(void)
{
  int id_aa64mmfr2_va_range;
  asm volatile(
    "mrs %0, id_aa64mmfr2_el1\n"
    "lsr %0, %0, #16\n"
    "and %0, %0, #0xf\n" : "=r"(id_aa64mmfr2_va_range)
  );
  return id_aa64mmfr2_va_range == 1;
}

NO_MMU int mmu_get_max_va_size(void)
{
  int max_va_size = 48;
#if MMU_PAGE_GRANULE == MMU_GRANULE_64K
  if (armv8_impl_has_feature_lva())
    max_va_size = 52;
#else
  if (armv8_impl_has_feature_lpa2())
    max_va_size = 52;
#endif

  return max_va_size;
}

NO_MMU void mmu_init(void)
{
  int va_size = 48;

  paddr_bits_t num_paddr_bits;

  asm volatile(
    "mrs x0, id_aa64mmfr0_el1\n"
    "and %0, x0, #0xf\n"
    : "=r"(num_paddr_bits)
  );

  mmu_map_range(0xffff000000080000, 0x00000, 0x100000,
    (uint64_t *)&__pagetable_start, (uint64_t *)&__pagetable_end);

  if (va_size > mmu_get_max_va_size())
    while(1);

  mmu_program_tcr(va_size, va_size);

  asm volatile (
    "msr ttbr0_el1, %0\n"
    "msr ttbr1_el1, %0\n"
    :: "r"(&__pagetable_start));

  asm volatile("at s1e1r, %0"::"r"(0));

  asm volatile (
    "mrs x0, sctlr_el1\n"
    "orr x0, x0, #1\n"
    "msr sctlr_el1, x0\n"
  );
}