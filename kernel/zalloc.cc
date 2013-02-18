#include "types.h"
#include "amd64.h"
#include "kernel.hh"
#include "percpu.hh"
#include "wq.hh"
#include "cpputil.hh"
#include "ilist.hh"
#include "mtrace.h"

extern "C" void zpage(void*);
extern "C" void zpage_nc(void*);

static const bool prezero = true;

struct free_page
{
  ilink<free_page> link;
  typedef ilist<free_page, &free_page::link> list_t;
};

struct zallocator {
  // pages and nPages must only be accessed by the local CPU and must
  // be accessed with interrupts disabled.
  free_page::list_t pages;
  unsigned nPages;
  wframe frame;
};
DEFINE_PERCPU(zallocator, z_);

struct zwork : public work {
  zwork(wframe* frame)
    : frame_(frame)
  {
    frame_->inc();
  }

  virtual void run() override {
    for (int i = 0; i < 32; i++) {
      auto *r = (struct free_page*)kalloc("zpage");
      if (r == nullptr)
        break;
      zpage_nc(r);
      scoped_cli cli;
      z_->pages.push_front(r);
      ++z_->nPages;
    }
    frame_->dec();
    delete this;
  }

  wframe* frame_;

  NEW_DELETE_OPS(zwork);
};

static void
tryrefill(void)
{
  int cpu = myid();
  if (prezero && z_[cpu].nPages < 16 && z_[cpu].frame.zero()) {
    zwork* w = new zwork(&z_[cpu].frame);
    if (wqcrit_push(w, cpu) < 0)
      delete w;
  }
}

// Allocate a zeroed page.  This page can be freed with kfree or, if
// it is known to be zeroed when it is freed, zfree.
char*
zalloc(const char* name)
{
  char* p = nullptr;

  {
    scoped_cli cli;
    if (!z_->pages.empty()) {
      p = (char*)&z_->pages.front();
      z_->pages.pop_front();
      --z_->nPages;
    }
  }

  if (p == nullptr) {
    p = kalloc(name);
    if (p != nullptr)
      zpage(p);
  } else {
    mtunlabel(mtrace_label_block, p);
    mtlabel(mtrace_label_block, p, PGSIZE, name, strlen(name));
    // Zero the free_page header
    memset(p, 0, sizeof(struct free_page));
    if (0)
      for (int i = 0; i < PGSIZE; i++)
        assert(p[i] == 0);
  }
  tryrefill();
  return p;
}

// Free a page that is known to be zero
void
zfree(void* p)
{
  if (0) 
    for (int i = 0; i < 4096; i++)
      assert(((char*)p)[i] == 0);

  scoped_cli cli;
  mtunlabel(mtrace_label_block, p);
  z_->pages.push_front((struct free_page*)p);
  ++z_->nPages;
}

void
initz(void)
{
}
