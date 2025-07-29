#include <stdint.h>
#include <mmu.h>
#include <common.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory_map.h>
#include "armv8_mair.h"
#include <stringlib.h>
#include <printf.h>
#include <log.h>

#define MMU_GRANULE_4K 4096
#define MMU_GRANULE_16K (1024 * 16)
#define MMU_GRANULE_64K (1024 * 64)

#define MMU_PAGE_GRANULE MMU_GRANULE_4K
#define NO_MMU __attribute__((section(".init.nommu")))

#define KERNEL_RAM0_PADDR_START 0
#define KERNEL_RAM0_PADDR_END   0x10000000

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
 *             |        |        |     (table 3)[x]->(page address)+[11:0]off
 *             |        |        |
 *             |        |       (table 2)[x]->(table 3)[x]
 *             |        |
 *             |      (table 1)[x]->(table 2)[x]
 *             |
 *          (table 0)[x] -> (table 1)[x]
 *
 *
 *
 * num entries in table = 2**(n - 3) = 2 ** 9 = 512 (3bits - are 8byte align)
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

struct mmu_info {
  uint64_t *pagetable_start;
  uint64_t *pagetable_end;

  /* Max input address */
  uint32_t virtual_mem_size;
  int num_pages;
  int num_l0_ptes;
  int num_l1_ptes;
  int num_l2_ptes;
  int num_l3_ptes;

  int num_l0_pages;
  int num_l1_pages;
  int num_l2_pages;
  int num_l3_pages;

  uint64_t *l0_pte_base;
  uint64_t *l1_pte_base;
  uint64_t *l2_pte_base;
  uint64_t *l3_pte_base;
  const uint64_t *page_table_real_end;

  int memattr_idx_normal;
  int memattr_idx_device;
  int memattr_idx_dma;
};

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

static inline NO_MMU uint64_t mmu_make_page_desc(uint64_t page_idx,
  int memattr_idx)
{
  uint64_t lower_attributes = ((memattr_idx & 7) << 2) | (1 << 10);
  uint64_t output_address = (page_idx << 12) & ((1ull << 48) - 1);
  return output_address | lower_attributes | 3;
}


static NO_MMU void mmu_page_table_init(struct mmu_info *mmui,
  uint32_t max_mem_size, uint64_t dma_mem_start, uint64_t dma_mem_end)
{
  int i;
  uint64_t desc;

  uint64_t page_idx_ram_0_start = KERNEL_RAM0_PADDR_START / MMU_PAGE_GRANULE;
  uint64_t page_idx_ram_0_end = dma_mem_start / MMU_PAGE_GRANULE;
  uint64_t page_idx_dma_start = dma_mem_start / MMU_PAGE_GRANULE;
  uint64_t page_idx_dma_end = dma_mem_end / MMU_PAGE_GRANULE;
  uint64_t page_idx_periph_start = PERIPH_ADDR_RANGE_START / MMU_PAGE_GRANULE;
  uint64_t page_idx_periph_end = PERIPH_ADDR_RANGE_END / MMU_PAGE_GRANULE;

  asm volatile ("ldr %0, =__pagetable_start\n" :"=r"(mmui->pagetable_start));
  asm volatile ("ldr %0, =__pagetable_end\n" :"=r"(mmui->pagetable_end));

  mmui->virtual_mem_size = max_mem_size;
  mmui->num_pages = (max_mem_size + (MMU_PAGE_GRANULE - 1)) / MMU_PAGE_GRANULE;
  mmui->num_l3_ptes = MAX(mmui->num_pages, 1);
  mmui->num_l2_ptes = MAX((mmui->num_l3_ptes * 8) / MMU_PAGE_GRANULE, 1);
  mmui->num_l1_ptes = MAX((mmui->num_l2_ptes * 8) / MMU_PAGE_GRANULE, 1);
  mmui->num_l0_ptes = MAX((mmui->num_l1_ptes * 8) / MMU_PAGE_GRANULE, 1);

#define NUM_DESCRIPTOR_PAGES(__num_descriptors) \
  ((__num_descriptors * 8 + (MMU_PAGE_GRANULE - 1)) / MMU_PAGE_GRANULE);

  mmui->num_l0_pages = NUM_DESCRIPTOR_PAGES(mmui->num_l0_ptes);
  mmui->num_l1_pages = NUM_DESCRIPTOR_PAGES(mmui->num_l1_ptes);
  mmui->num_l2_pages = NUM_DESCRIPTOR_PAGES(mmui->num_l2_ptes);
  mmui->num_l3_pages = NUM_DESCRIPTOR_PAGES(mmui->num_l3_ptes);

  mmui->l0_pte_base = mmui->pagetable_start;
  mmui->l1_pte_base = mmui->l0_pte_base +
    mmui->num_l0_pages * MMU_PAGE_GRANULE / 8;

  mmui->l2_pte_base = mmui->l1_pte_base +
    mmui->num_l1_pages * MMU_PAGE_GRANULE / 8;

  mmui->l3_pte_base = mmui->l2_pte_base +
    mmui->num_l2_pages * MMU_PAGE_GRANULE / 8;

  mmui->page_table_real_end = mmui->l3_pte_base + mmui->num_l3_ptes;

  if (mmui->page_table_real_end > mmui->pagetable_end)
    while(1);

  for (i = 0; i < mmui->num_l0_ptes; ++i) {
    desc = ((uint64_t)(mmui->l1_pte_base + 512 * i)) | 3;
    mmui->l0_pte_base[i] = desc;
  }

  for (i = 0; i < mmui->num_l1_ptes; ++i) {
    desc = ((uint64_t)(mmui->l2_pte_base + 512 * i)) | 3;
    mmui->l1_pte_base[i] = desc;
  }

  for (i = 0; i < mmui->num_l2_ptes; ++i) {
    desc = ((uint64_t)(mmui->l3_pte_base + 512 * i)) | 3;
    mmui->l2_pte_base[i] = desc;
  }

  for (i = page_idx_ram_0_start; i < page_idx_ram_0_end; ++i)
    mmui->l3_pte_base[i] = mmu_make_page_desc(i, mmui->memattr_idx_normal);

  for (i = page_idx_dma_start; i < page_idx_dma_end; ++i)
    mmui->l3_pte_base[i] = mmu_make_page_desc(i, mmui->memattr_idx_dma);

  for (i = page_idx_periph_start; i < page_idx_periph_end; ++i)
    mmui->l3_pte_base[i] = mmu_make_page_desc(i, mmui->memattr_idx_device);
}

NO_MMU int mmu_get_num_paddr_bits(void)
{
  paddr_bits_t num_paddr_bits;

  asm volatile(
    "mrs x0, id_aa64mmfr0_el1\n"
    "and %0, x0, #0xf\n"
    : "=r"(num_paddr_bits)
  );

  return num_paddr_bits;
}

#define ARMV8_MAIR_DEVICE_MEM MEMATTR_DEVICE_NGNRNE
#define ARMV8_MAIR_NORMAL_MEM ARMV8_MAIR_NORMAL(\
  MEMATTR_WRITEBACK_NONTRANS(MEMATTR_RA, MEMATTR_WA),\
  MEMATTR_WRITEBACK_NONTRANS(MEMATTR_RA, MEMATTR_WA))

#define ARMV8_MAIR_DMA_MEM MEMATTR_DEVICE_GRE

NO_MMU void mmu_init(uint64_t dma_memory_start, uint64_t dma_memory_end)
{
  struct armv8_mair mair = { 0 };
  struct mmu_info mmu;

  int va_size = 48;

  mmu.memattr_idx_normal = 0;
  mmu.memattr_idx_device = 1;
  mmu.memattr_idx_dma    = 2;

  mair.memattrs[mmu.memattr_idx_normal] = ARMV8_MAIR_NORMAL_MEM;
  mair.memattrs[mmu.memattr_idx_device] = ARMV8_MAIR_DEVICE_MEM;
  mair.memattrs[mmu.memattr_idx_dma] = ARMV8_MAIR_DMA_MEM;

  asm volatile(
      "msr mair_el1, %0\n" :: "r"(mair.value)
      );

  mmu_page_table_init(&mmu, 0x40000000, dma_memory_start, dma_memory_end);

  if (va_size > mmu_get_max_va_size())
    while(1);

  mmu_program_tcr(va_size, va_size);

  asm volatile (
    "msr ttbr0_el1, %0\n"
    "msr ttbr1_el1, %0\n"
    :: "r"(mmu.pagetable_start));

  asm volatile("at s1e1r, %0"::"r"(0));

  asm volatile (
    "mrs x0, sctlr_el1\n"
    "orr x0, x0, #1\n"
    "msr sctlr_el1, x0\n"
  );
}

static inline void par_to_string(uint64_t par, char *par_desc,
  int par_desc_len)
{
  char sh, memattr;
  const char *sh_str;

  if (!par_desc_len)
    return;

  *par_desc = 0;
  sh = (par >> 7) & 3;
  memattr = (par >> 56) & 0xff;
  switch(sh) {
    case 0: sh_str = "Non-Shareable"  ; break;
    case 1: sh_str = "Reserved"       ; break;
    case 2: sh_str = "Outer-Shareable"; break;
    case 3: sh_str = "Inner-Shareable"; break;
    default:sh_str = "Undefined"      ; break;
  }

  snprintf(par_desc, par_desc_len, "memattr:%02x,sh:%d(%s)", memattr, sh,
    sh_str);
}

static inline void mair_to_string(char attr, char *attr_desc,
  int attr_desc_len)
{
  int i, n = 0;
  if (!attr_desc_len)
    return;

  *attr_desc = 0;
  if ((attr & 0x0c) == attr) {
    n = snprintf(attr_desc, attr_desc_len, "Device, ");
    switch(attr >> 2) {
        /* read documentation of mmu_memattr.h */
      case 0: snprintf(attr_desc + n, attr_desc_len - n, "nGnRnE"); return;
      case 1: snprintf(attr_desc + n, attr_desc_len - n, "nGnRE");  return;
      case 2: snprintf(attr_desc + n, attr_desc_len - n, "nGRE");   return;
      case 3: snprintf(attr_desc + n, attr_desc_len - n, "GRE");    return;
      default: return;
    }
  }

  if (attr == 0xf0) {
    snprintf(attr_desc, attr_desc_len,
      "Tagged Normal In/Out Write-Back Non-Trans Rd/Wr-Alloc");
    return;
  }

  if ((attr & 0xf0) && (attr & 0x0f)) {
    const char *in_out_string[2] = {"In", "Out"};
    n = snprintf(attr_desc, attr_desc_len, "Normal");
    for (i = 0; i < 2; ++i) {
      char nattr = (attr >> (4 * i)) & 0xf;
      n += snprintf(attr_desc + n, attr_desc_len - n, ",%s", in_out_string[i]);
      if (nattr == 0xc) {
        n += snprintf(attr_desc + n, attr_desc_len - n, ",Non-Cacheable");
      } else {
        switch(nattr & 0xc) {
          case 0x0:
            n += snprintf(attr_desc + n, attr_desc_len - n, ",Wr-Thr Trans");
          break;
          case 0x4:
            n += snprintf(attr_desc + n, attr_desc_len - n, " Wr-Back Trans");
          break;
          case 0x8:
            n += snprintf(attr_desc + n, attr_desc_len - n,
              " Wr-Thr Non-Trans");
          break;
          case 0xc:
            n += snprintf(attr_desc + n, attr_desc_len - n,
              " Wr-Back Non-Trans");
          break;
        }
        if (nattr & 1)
          n += snprintf(attr_desc + n, attr_desc_len - n, " WrAlloc");
        if (nattr & 2)
          n += snprintf(attr_desc + n, attr_desc_len - n, " RdAlloc");
      }
    }
  }
}

bool mmu_get_pddr(uint64_t va, uint64_t *pa)
{
  uint64_t par = 0xffffffff;
  asm volatile ("at s1e1r, %1\nmrs %0, PAR_EL1" : "=r"(par) : "r"(va));

  if (par & 1)
    return false;

  if (!pa)
    return false;

  uint64_t bits_51_48 = (par >> 48) & 0xf;
  uint64_t bits_47_12 = (par >> 12) & 0xffffffff;
  uint64_t bits_11_0 = va & 0xfff;
  *pa = bits_11_0 | (bits_47_12 << 12) | (bits_51_48 << 48);
  return true;
}

#if 0
void mmu_print_va(uint64_t addr, int verbose)
{
  uint64_t va = addr;
  uint64_t par = 0xffffffff;
  char par_desc[256];
  char attr_desc[256];
  char memattr;
  asm volatile ("at s1e1r, %1\nmrs %0, PAR_EL1" : "=r"(par) : "r"(va));
  par_to_string(par, par_desc, sizeof(par_desc));
  printf("MMUINFO: VA:%016llx -> PAR:%016llx %s" __endline, va, par,
    par_desc);

  if (verbose) {
    attr_desc[0] = 0;
    memattr = (par >> 56) & 0xff;
    mair_to_string(memattr, attr_desc, sizeof(attr_desc));
    printf("-------: MEMATTR:%02x %s" __endline, memattr, attr_desc);
  }
}
#endif
