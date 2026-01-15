#include <kmalloc.h>
#include <common.h>
#include <math.h>
#include <list.h>
#include <stringlib.h>
#include <log.h>
#include <cpu.h>
#include <list.h>

struct dma_mem_header {
  void *addr;
  struct list_head list;
};

struct dma_mem_area {
  struct dma_mem_header *chunks_table;
  char *chunks_mem;
  int chunk_sz_log;
  int num_chunks;
  struct list_head free_list;
  struct list_head busy_list;
};

#undef DEBUG_DMA_ALLOC
#define __DEFINE_MEM(__log_sz, __n, __align, __section) \
  SECTION(__section) \
  ALIGNED(__align) \
  char dma_mem_ ## __log_sz[(1<<__log_sz) * __n]

#define __DEFINE_MEM_HEADER(__log_sz, __n) \
  struct dma_mem_header dma_mem_hdr_ ## __log_sz[__n]

#define DMA_MEM_GROUP(__log_sz, __n, __align) \
  __DEFINE_MEM(__log_sz, __n, __align, ".dma_memory"); \
  __DEFINE_MEM_HEADER(__log_sz, __n)

#define DMA_MEM_AREA(__log_sz) {\
  .chunks_table = dma_mem_hdr_ ## __log_sz,\
  .chunks_mem = dma_mem_ ## __log_sz,\
  .chunk_sz_log = __log_sz,\
  .num_chunks = ARRAY_SIZE(dma_mem_hdr_ ## __log_sz),\
}

extern char __dma_memory_start;
extern char __dma_memory_end;

#define dma_memory_start (void *)&__dma_memory_start
#define dma_memory_end   (void *)&__dma_memory_end

#define DMA_SIZE(__n) (ARRAY_SIZE(chunks_ ## __n) << __n)
#define DMA_MEMORY_SIZE_4096 (DMA_MEMORY_SIZE - DMA_SIZE(6) - DMA_SIZE(7) - DMA_SIZE(9))

DMA_MEM_GROUP(6 , 64, 64);
DMA_MEM_GROUP(7 , 64, 64);
DMA_MEM_GROUP(9 , 64, 1024);
DMA_MEM_GROUP(12, 32, 4096);
DMA_MEM_GROUP(16, 64, 4096);
DMA_MEM_GROUP(19, 512, 4096);
DMA_MEM_GROUP(21, 8 , 4096);
DMA_MEM_GROUP(22, 4 , 4096);
DMA_MEM_GROUP(23, 2 , 4096);
DMA_MEM_GROUP(24, 2 , 4096);

static struct dma_mem_area dma_chunk_areas[] = {
  DMA_MEM_AREA(6),
  DMA_MEM_AREA(7),
  DMA_MEM_AREA(9),
  DMA_MEM_AREA(12),
  DMA_MEM_AREA(16),
  DMA_MEM_AREA(19),
  DMA_MEM_AREA(21),
  DMA_MEM_AREA(22),
  DMA_MEM_AREA(23),
  DMA_MEM_AREA(24)
};

static int logsize_to_area_idx[] = {
  /* starting from 5 log2(32) = 5 */
  0, /*  5: 32      -> 64 */
  0, /*  6: 64      -> 64 */
  1, /*  7: 128     -> 128 */
  2, /*  8: 256     -> 512 */
  2, /*  9: 512     -> 512 */
  3, /* 10: 1024    -> 4096 */
  3, /* 11: 2048    -> 4096 */
  3, /* 12: 4096    -> 4096 */
  4, /* 13: 8192    -> 524288 */
  4, /* 14: 16384   -> 524288 */
  4, /* 15: 32768   -> 524288 */
  4, /* 16: 65636   -> 524288 */
  5, /* 17: 131072  -> 524288 */
  5, /* 18: 262144  -> 524288 */
  5, /* 19: 524288  -> 524288 */
  6, /* 20: 1048576 -> 2097152 */
  6, /* 21: 2097152 -> 2097152 */
  7, /* 22: 4194304 -> 4194304 */
  8, /* 23: 8388608 -> 8388608 */
  9, /* 24: 16777216 -> 16777216 */

};

static inline int chunk_area_get_szlog(struct dma_mem_area *a)
{
  return a->chunk_sz_log;
}

static inline struct dma_mem_header *chunk_get_by_addr(void *addr,
  struct dma_mem_area **chunk_area)
{
  int i;
  struct dma_mem_area *d;
  struct dma_mem_header *entry;
  uint64_t base_addr = (uint64_t)dma_memory_start;
  uint64_t tmp;
  BUG_IF(addr < dma_memory_start, "attempt to free area before dma range");
  BUG_IF(addr >= dma_memory_end, "attempt to free area after dma range");

  /*
   * First detect the right area
   */
  for (i = 0; i < ARRAY_SIZE(dma_chunk_areas); ++i) {
    d = &dma_chunk_areas[i];
    base_addr += d->num_chunks << chunk_area_get_szlog(d);
    if ((uint64_t)addr <= base_addr)
      break;
  }

  /*
   * Now get exact chunk by address
   */
  base_addr = (uint64_t)d->chunks_table[0].addr;
  tmp = (uint64_t)addr - base_addr;
  i = tmp >> chunk_area_get_szlog(d);
  entry = &d->chunks_table[i];
  *chunk_area = d;
  return entry;
}

static inline struct dma_mem_area *chunk_area_get_by_sz(int sz)
{
  int logsz = get_biggest_log2(sz);

  if (logsz < 5)
    logsz = 5;

  if ((logsz - 5) > ARRAY_SIZE(logsize_to_area_idx)) {
    os_log("chunk_area_get_by_sz: size too big, size:%d,"
      "biggest_log: %d, array_len: %d"__endline, sz, logsz,
      ARRAY_SIZE(logsize_to_area_idx));

    return NULL;
  }

  return &dma_chunk_areas[logsize_to_area_idx[logsz - 5]];
}

uint64_t dma_memory_get_start_addr(void)
{
  return (uint64_t)dma_memory_start;
}

uint64_t dma_memory_get_end_addr(void)
{
  return (uint64_t)dma_memory_end;
}

void *dma_alloc(size_t sz, bool zero)
{
  struct dma_mem_header *c;
  struct dma_mem_area *a;
  a = chunk_area_get_by_sz(sz);
#if DEBUG_DMA_ALLOC
  os_log("dma_alloc: %d, area: %016lx, log:%d\r\n", sz, a, a->chunk_sz_log);
#endif
  BUG_IF(!a, "dma_alloc: size too big");
  if (list_empty(&a->free_list))
  {
    os_log("Failed to allocate dma chunk with size %ld bytes, "
      "no free chunks\r\n", sz);
    return NULL;
  }
  c = list_first_entry(&a->free_list, struct dma_mem_header, list);
  list_del(&c->list);
  list_add_tail(&c->list, &a->busy_list);

  if (zero)
    memset(c->addr, 0, sz);

  return c->addr;
}

void dma_free(void *addr)
{
  struct dma_mem_header *c;
  struct dma_mem_area *a;
  c = chunk_get_by_addr(addr, &a);
  BUG_IF(!c, "Failed to find chunk by given address");
  list_del(&c->list);
  list_add_tail(&c->list, &a->free_list);
}

void OPTIMIZED dma_memory_init(void)
{
  struct dma_mem_area *ca, *ca_end;
  struct dma_mem_header *c, *c_end;
  char *chunk_mem;
  uint64_t t;
  size_t i;
  t = get_boottime_msec();

  ca = dma_chunk_areas;
  ca_end = ca + ARRAY_SIZE(dma_chunk_areas);

  for (; ca < ca_end; ca++) {
#if 0
    printf("dma_init: init area for chunks (sz: %d,n: %d at %016lx)\r\n",
      1<<ca->chunk_sz_log, ca->num_chunks, ca->chunks_mem);
#endif

    INIT_LIST_HEAD(&ca->free_list);
    INIT_LIST_HEAD(&ca->busy_list);

    c = ca->chunks_table;
    c_end = c + ca->num_chunks;
    chunk_mem = ca->chunks_mem;
    i = 0;
    for (; c < c_end; ++c) {

      INIT_LIST_HEAD(&c->list);
      list_add_tail(&c->list, &ca->free_list);
      c->addr = chunk_mem;
      chunk_mem += (1 << ca->chunk_sz_log);
      // printf("dma_init:   chunk %d at addr %016lx\r\n", i, c->addr);
      ++i;
    }
  }
  t = get_boottime_msec() - t;
  os_log("dma_memory_init complete. took %lldms"__endline, t);
}
