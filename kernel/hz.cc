#include "types.h"
#include "riscv.h"
#include "kernel.hh"
#include "string.h"
#include "fdt.h"

u64 cpuhz;

void
microdelay(u64 delay)
{
  u64 tscdelay = (cpuhz * delay) / 1000000;
  u64 s = rdcycle();
  while (rdcycle() - s < tscdelay);
}

void query_harts(uintptr_t fdt);

void
inithz(void *fdt)
{
  query_harts((uintptr_t)fdt);
}

struct hart_scan {
  const struct fdt_scan_node *cpu;
  int hart;
  const struct fdt_scan_node *controller;
  int cells;
  uint32_t phandle;
  uint32_t freq;
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
    scan->cpu = prop->node;
  } else if (!strcmp(prop->name, "interrupt-controller")) {
    scan->controller = prop->node;
  } else if (!strcmp(prop->name, "#interrupt-cells")) {
    scan->cells = fdt_bswap(prop->value[0]);
  } else if (!strcmp(prop->name, "phandle")) {
    scan->phandle = fdt_bswap(prop->value[0]);
  } else if (!strcmp(prop->name, "reg")) {
    uint64_t reg;
    fdt_get_address(prop->node->parent, prop->value, &reg);
    scan->hart = reg;
  } else if (!strcmp(prop->name, "clock-frequency")) {
    scan->freq = fdt_bswap(prop->value[0]);
    cpuhz = scan->freq;
  }
}

static void hart_done(const struct fdt_scan_node *node, void *extra)
{
  struct hart_scan *scan = (struct hart_scan *)extra;

  if (scan->controller == node && scan->cpu) {
    if (scan->hart < NPROC) {
      ++ncpu;
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

  ncpu = 0;
  fdt_scan(fdt, &cb);
}
