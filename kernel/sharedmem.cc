#include "types.h"
#include "kernel.hh"
#include "vm.hh"

class shared_memory_region : public pageable {
public:
  explicit shared_memory_region(size_t npages);
  void onzero() override;
  NEW_DELETE_OPS(shared_memory_region);
  sref<page_info> *pages;
  size_t num_pages;
  sref<page_info> get_page_info(u64 page_idx) override;
};

shared_memory_region::shared_memory_region(size_t npages)
  : num_pages(npages)
{
  this->pages = (sref<page_info> *) kmalloc(npages * sizeof(sref<page_info>), "MAP_SHARED metadata");
  for (size_t i = 0; i < npages; i++) {
    void *p = zalloc("MAP_ANON|MAP_SHARED");
    if (!p)
      throw_bad_alloc();
    auto pi = sref<page_info>::transfer(new (page_info::of(p)) page_info());
    new(&this->pages[i]) sref<page_info>(pi);
  }
}

void
shared_memory_region::onzero()
{
  for (size_t i = 0; i < num_pages; i++) {
    this->pages[i].~sref<page_info>();
  }
  kmfree(this->pages, num_pages * sizeof(sref<page_info>));
  delete this;
}

sref<page_info>
shared_memory_region::get_page_info(u64 page_idx)
{
  if (page_idx >= num_pages)
    return sref<page_info>();

  return this->pages[page_idx];
}

sref<pageable>
new_shared_memory_region(size_t pages)
{
  return make_sref<shared_memory_region>(pages);
}
