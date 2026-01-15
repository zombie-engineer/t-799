#include <compiler.h>
#include <stdint.h>
#include <common.h>
#include <sched.h>
#include <task.h>
#include <os_api.h>
#include <printf.h>
#include <sched_mon.h>
#include <arch/armv8/cpuctx_armv8.h>

extern char __bss_start;
extern char __bss_end;
extern char __text_start;
extern char __text_end;
extern char __stacks_start;
extern char __stacks_end;
#define EXCEPTION_VECTOR __attribute__((section(".exception_vector")))

/*
 * Incomplete list of ARMv8 exception class types encoded into ELx_ECR register.
 * See ARM DDI0487B_b chapter D10.2.28
 */
#define ARMV8_EXCEPTION_CLASS_UNKNOWN           0b000000
#define ARMV8_EXCEPTION_CLASS_TRAP_WFI_WFE      0b000001
#define ARMV8_EXCEPTION_CLASS_TRAP_MCR_MRC      0b000011
#define ARMV8_EXCEPTION_CLASS_TRAP_MCRR_MRRC    0b000100
#define ARMV8_EXCEPTION_CLASS_TRAP_MCR_MRC2     0b000101
#define ARMV8_EXCEPTION_CLASS_TRAP_LDC_STC      0b000110
#define ARMV8_EXCEPTION_CLASS_SME_SVE_SIMD      0b000111
#define ARMV8_EXCEPTION_CLASS_TRAP_LD64B_ST64B  0b001010
#define ARMV8_EXCEPTION_CLASS_TRAP_MRRC_2       0b001100
#define ARMV8_EXCEPTION_CLASS_ILLEGAL_EXECUTION 0b001110
#define ARMV8_EXCEPTION_CLASS_SVC_AARCH32       0b010001
#define ARMV8_EXCEPTION_CLASS_SVC_AARCH64       0b010101
#define ARMV8_EXCEPTION_CLASS_TRAP_MSR_MRS      0b011000
#define ARMV8_EXCEPTION_CLASS_SVE_ACCESS        0b011001
#define ARMV8_EXCEPTION_CLASS_TSTART_EL0        0b011011
#define ARMV8_EXCEPTION_CLASS_PTR_AUTH          0b011100
#define ARMV8_EXCEPTION_CLASS_SME               0b011101
#define ARMV8_EXCEPTION_CLASS_GRANULE_PROTECT   0b011110
#define ARMV8_EXCEPTION_CLASS_INST_ABRT_LO_EL   0b100000
#define ARMV8_EXCEPTION_CLASS_INST_ABRT_EQ_EL   0b100001
#define ARMV8_EXCEPTION_CLASS_INST_ALIGNMENT    0b100010
#define ARMV8_EXCEPTION_CLASS_DATA_ABRT_LO_EL   0b100100
#define ARMV8_EXCEPTION_CLASS_DATA_ABRT_EQ_EL   0b100101
#define ARMV8_EXCEPTION_CLASS_STACK_ALIGN_FLT   0b100110
#define ARMV8_EXCEPTION_CLASS_MEMOP             0b100111
#define ARMV8_EXCEPTION_CLASS_FLOATING_POINT    0b101100
#define ARMV8_EXCEPTION_CLASS_SERROR            0b101111
#define ARMV8_EXCEPTION_CLASS_BKPT_LOW_LVL      0b110000
#define ARMV8_EXCEPTION_CLASS_BKPT_SAME_LVL     0b110001
#define ARMV8_EXCEPTION_CLASS_SW_STEP_LOW_LVL   0b110010
#define ARMV8_EXCEPTION_CLASS_SW_STEP_SAME_LVL  0b110011
#define ARMV8_EXCEPTION_CLASS_WATCHPT_LOW_LVL   0b110100
#define ARMV8_EXCEPTION_CLASS_WATCHPT_SAME_LVL  0b110101
#define ARMV8_EXCEPTION_CLASS_BKPT_AARCH32      0b111000
#define ARMV8_EXCEPTION_CLASS_BRK_AARCH64       0b111100

#define SYNC_HANDLER(__suffix) \
static void armv8_handle_exception_sync_ ##__suffix(uint32_t iss)

SYNC_HANDLER(unknown)
{
}

SYNC_HANDLER(trap_wfi_wfe)
{
}

SYNC_HANDLER(trap_mcr_mrc)
{
}

SYNC_HANDLER(trap_mcrr_mrrc)
{
}

SYNC_HANDLER(trap_mcr_mrc2)
{
}

SYNC_HANDLER(trap_ldc_stc)
{
}

SYNC_HANDLER(sme_sve_simd)
{
}

SYNC_HANDLER(trap_ld64b_st64b)
{
}

SYNC_HANDLER(trap_mrrc_2)
{
}

SYNC_HANDLER(illegal_execution)
{
}

SYNC_HANDLER(svc_aarch32)
{
}

SYNC_HANDLER(svc_aarch64)
{
  svc_handler(iss);
}

SYNC_HANDLER(trap_msr_mrs)
{
}

SYNC_HANDLER(sve_access)
{
}

SYNC_HANDLER(tstart_el0)
{
}

SYNC_HANDLER(ptr_auth)
{
}

SYNC_HANDLER(sme)
{
}

SYNC_HANDLER(granule_protect)
{
}

#define ADDR_IN_SECTION(__addr, __section) \
  ((__addr) >= ((void *)&__ ## __section ## _start) && \
  (__addr) < ((void *)&__ ## __section ## _end))
static const char *addr_to_section_name(uint64_t addr)
{
  void *p = (void *)addr;
  if (ADDR_IN_SECTION(p, bss))
    return ".bss";
  if (ADDR_IN_SECTION(p, text))
    return ".text";
  if (ADDR_IN_SECTION(p, stacks))
    return ".stacks";
  return NULL;
}

static inline void dump_exception_context(bool dump_far, const char *exception)
{
  uint64_t pc;
  uint64_t sp;
  uint64_t far;
  uint64_t value;
  size_t i;
  const char *section_name;
  char mem_desc[64];

  struct task *t = sched_get_current_task();
  const struct armv8_cpuctx *c;

  if (!t) {
    printf("aarch64 exception: '%s' no current task");
    return;
  }

  c = t->cpuctx;
  pc = c->pc;
  sp = c->sp;
  printf("aarch64 exception: '%s', task: %s, pc:%016lx, sp:%016lx\r\n",
    exception, t->name, pc, sp);
  if (dump_far) {
    aarch64_get_far_el1(far);
    printf("far_el1: %016lx\r\n", far);
  }

  printf("Registers: \r\n");
  for (i = 0; i < ARRAY_SIZE(c->gpregs); ++i) {
    value = c->gpregs[i];
    section_name = addr_to_section_name(value);
    if (section_name)
      snprintf(mem_desc, sizeof(mem_desc), ", '%s'", section_name);
    else
      mem_desc[0] = 0;
    printf("x%d: %016lx%s\r\n", i, value, mem_desc);
#if 0
    if (((i + 1) % 3) == 0)
      printf("\r\n");
#endif
  }

  if ((i + 1) % 3)
    printf("\r\n");

  printf("Stack: \r\n");
  for (i = 0; i < 32; ++i) {
    value = ((const uint64_t *)sp)[i];
    section_name = addr_to_section_name(value);
    if (section_name)
      snprintf(mem_desc, sizeof(mem_desc), ", '%s'", section_name);
    else
      mem_desc[0] = 0;

    printf("  %016lx: %016lx%s\r\n", sp + i * 8, value, mem_desc);
  }
}

SYNC_HANDLER(inst_abrt_lo_el)
{
  dump_exception_context(true, "instruction abort lower EL");
}

SYNC_HANDLER(inst_abrt_eq_el)
{
  dump_exception_context(true, "instruction abort same EL");
}

SYNC_HANDLER(inst_alignment)
{
  dump_exception_context(true, "instruction alignment");
  while(1) { }
}

SYNC_HANDLER(data_abrt_lo_el)
{
  dump_exception_context(true, "data abort lower EL");
  while(1) { }
}

SYNC_HANDLER(data_abrt_eq_el)
{
  dump_exception_context(true, "data abort same EL");
  while(1);
}

SYNC_HANDLER(stack_aling_flt)
{
}

SYNC_HANDLER(memop)
{
}

SYNC_HANDLER(floating_point)
{
}

SYNC_HANDLER(serror)
{
}

SYNC_HANDLER(bkpt_low_lvl)
{
}

SYNC_HANDLER(bkpt_same_lvl)
{
}

SYNC_HANDLER(sw_step_low_lvl)
{
}

SYNC_HANDLER(sw_step_same_lvl)
{
}

SYNC_HANDLER(watchpt_low_lvl)
{
}

SYNC_HANDLER(watchpt_same_lvl)
{
}

SYNC_HANDLER(bkpt_aarch32)
{
}

SYNC_HANDLER(brk_aarch64)
{
}

#define ITEM(__exception_class, __handler_name) \
  [ARMV8_EXCEPTION_CLASS_##__exception_class] = \
    armv8_handle_exception_sync_ ## __handler_name

typedef void (*armv8_exception_handler_sync_fn)(uint32_t iss);

static __attribute__((section(".rodata_nommu"))) armv8_exception_handler_sync_fn const armv8_exception_sync_handlers[] = {
  ITEM(UNKNOWN, unknown),
  ITEM(TRAP_WFI_WFE, trap_wfi_wfe),
  ITEM(TRAP_MCR_MRC, trap_mcr_mrc),
  ITEM(TRAP_MCRR_MRRC, trap_mcrr_mrrc),
  ITEM(TRAP_MCR_MRC2, trap_mcr_mrc2),
  ITEM(TRAP_LDC_STC, trap_ldc_stc),
  ITEM(SME_SVE_SIMD, sme_sve_simd),
  ITEM(TRAP_LD64B_ST64B, trap_ld64b_st64b),
  ITEM(TRAP_MRRC_2, trap_mrrc_2),
  ITEM(ILLEGAL_EXECUTION, illegal_execution),
  ITEM(SVC_AARCH32, svc_aarch32),
  ITEM(SVC_AARCH64, svc_aarch64),
  ITEM(TRAP_MSR_MRS, trap_msr_mrs),
  ITEM(SVE_ACCESS, sve_access),
  ITEM(TSTART_EL0, tstart_el0),
  ITEM(PTR_AUTH, ptr_auth),
  ITEM(SME, sme),
  ITEM(GRANULE_PROTECT, granule_protect),
  ITEM(INST_ABRT_LO_EL, inst_abrt_lo_el),
  ITEM(INST_ABRT_EQ_EL, inst_abrt_eq_el),
  ITEM(INST_ALIGNMENT, inst_alignment),
  ITEM(DATA_ABRT_LO_EL, data_abrt_lo_el),
  ITEM(DATA_ABRT_EQ_EL, data_abrt_eq_el),
  ITEM(STACK_ALIGN_FLT, stack_aling_flt),
  ITEM(MEMOP, memop),
  ITEM(FLOATING_POINT, floating_point),
  ITEM(SERROR, serror),
  ITEM(BKPT_LOW_LVL, bkpt_low_lvl),
  ITEM(BKPT_SAME_LVL, bkpt_same_lvl),
  ITEM(SW_STEP_LOW_LVL, sw_step_low_lvl),
  ITEM(SW_STEP_SAME_LVL, sw_step_same_lvl),
  ITEM(WATCHPT_LOW_LVL, watchpt_low_lvl),
  ITEM(WATCHPT_SAME_LVL, watchpt_same_lvl),
  ITEM(BKPT_AARCH32, bkpt_aarch32),
  ITEM(BRK_AARCH64, brk_aarch64)
};

#define ESR_GET_SYNC_EXCEPTION_CLASS(__value) \
  ((__value >> 26) & 0x7f)

#define ESR_GET_SYNC_ISS(__value) \
  (__value & ((1<<25) - 1))

EXCEPTION_VECTOR void armv8_exception_handler_sync(uint64_t esr)
{
  armv8_exception_handler_sync_fn sync_cb;
#if defined(CONFIG_SCHED_MON)
  sched_mon_save_ctx();
#endif

  int exception_class = ESR_GET_SYNC_EXCEPTION_CLASS(esr);
  int iss = ESR_GET_SYNC_ISS(esr);

  if (exception_class >= ARRAY_SIZE(armv8_exception_sync_handlers))
    panic();

  sync_cb = armv8_exception_sync_handlers[exception_class];
  if (!sync_cb)
    panic();

  sync_cb(iss);
  sched_mon_restore_ctx();
}
