#include "types.h"
#include "fdt.h"
#include "string.h"

static inline uint32_t bswap(uint32_t x)
{
  uint32_t y = (x & 0x00FF00FF) <<  8 | (x & 0xFF00FF00) >>  8;
  uint32_t z = (y & 0x0000FFFF) << 16 | (y & 0xFFFF0000) >> 16;
  return z;
}

uint32_t fdt_bswap(uint32_t x)
{
  return bswap(x);
}

static inline int isstring(char c)
{
  if (c >= 'A' && c <= 'Z')
    return 1;
  if (c >= 'a' && c <= 'z')
    return 1;
  if (c >= '0' && c <= '9')
    return 1;
  if (c == '\0' || c == ' ' || c == ',' || c == '-')
    return 1;
  return 0;
}

static uint32_t *fdt_scan_helper(
  uint32_t *lex,
  const char *strings,
  struct fdt_scan_node *node,
  const struct fdt_cb *cb)
{
  struct fdt_scan_node child;
  struct fdt_scan_prop prop;
  int last = 0;

  child.parent = node;
  // these are the default cell counts, as per the FDT spec
  child.address_cells = 2;
  child.size_cells = 1;
  prop.node = node;

  while (1) {
    switch (bswap(lex[0])) {
      case FDT_NOP: {
        lex += 1;
        break;
      }
      case FDT_PROP: {
        // assert (!last);
        prop.name  = strings + bswap(lex[2]);
        prop.len   = bswap(lex[1]);
        prop.value = lex + 3;
        if (node && !strcmp(prop.name, "#address-cells")) { node->address_cells = bswap(lex[3]); }
        if (node && !strcmp(prop.name, "#size-cells"))    { node->size_cells    = bswap(lex[3]); }
        lex += 3 + (prop.len+3)/4;
        cb->prop(&prop, cb->extra);
        break;
      }
      case FDT_BEGIN_NODE: {
        uint32_t *lex_next;
        if (!last && node && cb->done) cb->done(node, cb->extra);
        last = 1;
        child.name = (const char *)(lex+1);
        if (cb->open) cb->open(&child, cb->extra);
        lex_next = fdt_scan_helper(
          lex + 2 + strlen(child.name)/4,
          strings, &child, cb);
        if (cb->close && cb->close(&child, cb->extra) == -1)
          while (lex != lex_next) *lex++ = bswap(FDT_NOP);
        lex = lex_next;
        break;
      }
      case FDT_END_NODE: {
        if (!last && node && cb->done) cb->done(node, cb->extra);
        return lex + 1;
      }
      default: { // FDT_END
        if (!last && node && cb->done) cb->done(node, cb->extra);
        return lex;
      }
    }
  }
}

void fdt_scan(uintptr_t fdt, const struct fdt_cb *cb)
{
  struct fdt_header *header = (struct fdt_header *)fdt;

  // Only process FDT that we understand
  if (bswap(header->magic) != FDT_MAGIC ||
      bswap(header->last_comp_version) > FDT_VERSION) return;

  const char *strings = (const char *)(fdt + bswap(header->off_dt_strings));
  uint32_t *lex = (uint32_t *)(fdt + bswap(header->off_dt_struct));

  fdt_scan_helper(lex, strings, 0, cb);
}

uint32_t fdt_size(uintptr_t fdt)
{
  struct fdt_header *header = (struct fdt_header *)fdt;

  // Only process FDT that we understand
  if (bswap(header->magic) != FDT_MAGIC ||
      bswap(header->last_comp_version) > FDT_VERSION) return 0;
  return bswap(header->totalsize);
}

const uint32_t *fdt_get_address(const struct fdt_scan_node *node, const uint32_t *value, uint64_t *result)
{
  *result = 0;
  for (int cells = node->address_cells; cells > 0; --cells)
    *result = (*result << 32) + bswap(*value++);
  return value;
}

const uint32_t *fdt_get_size(const struct fdt_scan_node *node, const uint32_t *value, uint64_t *result)
{
  *result = 0;
  for (int cells = node->size_cells; cells > 0; --cells)
    *result = (*result << 32) + bswap(*value++);
  return value;
}

int fdt_string_list_index(const struct fdt_scan_prop *prop, const char *str)
{
  const char *list = (const char *)prop->value;
  const char *end = list + prop->len;
  int index = 0;
  while (end - list > 0) {
    if (!strcmp(list, str)) return index;
    ++index;
    list += strlen(list) + 1;
  }
  return -1;
}

///////////////////////////////////////////// HART SCAN //////////////////////////////////////////

/*
extern size_t n_hart;
static uint32_t hart_phandles[MAX_HARTS];
uint64_t hart_mask;

struct hart_scan {
  const struct fdt_scan_node *cpu;
  int hart;
  const struct fdt_scan_node *controller;
  int cells;
  uint32_t phandle;
};

static void hart_open(const struct fdt_scan_node *node, void *extra)
{
  struct hart_scan *scan = (struct hart_scan *)extra;
  if (!scan->cpu) {
    scan->hart = -1;
  }
  if (!scan->controller) {
    scan->cells = 0;
    scan->phandle = 0;
  }
}

static void hart_prop(const struct fdt_scan_prop *prop, void *extra)
{
  struct hart_scan *scan = (struct hart_scan *)extra;
  if (!strcmp(prop->name, "device_type") && !strcmp((const char*)prop->value, "cpu")) {
    // assert (!scan->cpu);
    scan->cpu = prop->node;
  } else if (!strcmp(prop->name, "interrupt-controller")) {
    // assert (!scan->controller);
    scan->controller = prop->node;
  } else if (!strcmp(prop->name, "#interrupt-cells")) {
    scan->cells = bswap(prop->value[0]);
  } else if (!strcmp(prop->name, "phandle")) {
    scan->phandle = bswap(prop->value[0]);
  } else if (!strcmp(prop->name, "reg")) {
    uint64_t reg;
    fdt_get_address(prop->node->parent, prop->value, &reg);
    scan->hart = reg;
  }
}

static void hart_done(const struct fdt_scan_node *node, void *extra)
{
  struct hart_scan *scan = (struct hart_scan *)extra;

  if (scan->cpu == node) {
    // assert (scan->hart >= 0);
  }

  if (scan->controller == node && scan->cpu) {
    // assert (scan->phandle > 0);
    // assert (scan->cells == 1);

    if (scan->hart < MAX_HARTS) {
      hart_phandles[scan->hart] = scan->phandle;
      hart_mask |= 1 << scan->hart;
      ++n_hart;
    }
  }
}

static int hart_close(const struct fdt_scan_node *node, void *extra)
{
  struct hart_scan *scan = (struct hart_scan *)extra;
  if (scan->cpu == node) scan->cpu = 0;
  if (scan->controller == node) scan->controller = 0;
  return 0;
}

void query_harts(uintptr_t fdt)
{
  struct fdt_cb cb;
  struct hart_scan scan;

  memset(&cb, 0, sizeof(cb));
  memset(&scan, 0, sizeof(scan));
  cb.open = hart_open;
  cb.prop = hart_prop;
  cb.done = hart_done;
  cb.close= hart_close;
  cb.extra = &scan;

  fdt_scan(fdt, &cb);

  // The current hart should have been detected
  // assert ((hart_mask >> read_csr(mhartid)) != 0);
}
*/
