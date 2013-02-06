#include "ugc.hh"
#include <stdio.h>

class gcd_memory
{
public:
  void add(gcptr* ptr);
  void collect(gcptr* ptr);
  void collect_work_list();

  bool marked(gcptr* ptr);
  void mark(gcptr* ptr);

  void sweep();
  
  mark_sense_t current_sense_;  

private:
  void collect_ptr(gcptr& ptr);

  ilist<gcptr, &gcptr::node_link_> node_list_;
  ilist<gcptr, &gcptr::work_link_> work_list_;
};

static gcd_memory the_mem;

//
// gcd_memory
//

void
gcd_memory::mark(gcptr* ptr)
{
  ptr->mark_ = current_sense_;
}

bool
gcd_memory::marked(gcptr* ptr)
{
  return (ptr->mark_ == current_sense_);
}

void
gcd_memory::add(gcptr* ptr)
{
  node_list_.push_front(ptr);  
}

void
gcd_memory::sweep()
{
  auto it = node_list_.begin();
  while (it != node_list_.end()) {
    if (!marked(it.elem)) {
      gcptr* ptr = it.elem;
      it = node_list_.erase(it);
      delete ptr;
    } else {
      it++;
    }
  }
}

void
gcd_memory::collect_ptr(gcptr& ptr)
{
  for (gcptr_ref &ref : ptr.ref_list_) {
    if (!marked(ref.ptr)) {
      mark(ref.ptr);
      if (!ref.ptr->ref_list_.empty())
        work_list_.push_back(ref.ptr);
    }
  }
}

void
gcd_memory::collect_work_list()
{
  while (!work_list_.empty()) {
    auto it = work_list_.begin();
    work_list_.pop_front();
    collect_ptr(*it);
  }
}

void
gcd_memory::collect(gcptr* ptr)
{
  work_list_.push_back(ptr);

  while (!work_list_.empty()) {
    the_mem.current_sense_ = !the_mem.current_sense_;
    collect_work_list();
    sweep();
  }
}

//
// gcptr
//

gcptr::gcptr()
  : mark_(the_mem.current_sense_)
{
  the_mem.add(this);  
}

gcptr::gcptr(bool track)
  : mark_(the_mem.current_sense_)
{
  if (track)
    the_mem.add(this);  
}

gcptr::~gcptr()
{
  while (!ref_list_.empty()) {
    auto it = ref_list_.begin();
    gcptr_ref* ref = it.elem;

    ref_list_.erase(it);
    delete ref;
  }
}

gcptr_ref*
gcptr::ref(gcptr* child)
{
  gcptr_ref* ref = new gcptr_ref();

  ref->ptr = child;
  ref_list_.push_front(ref);

  return ref;
}

void
gcptr::uref(gcptr_ref* child)
{
  ref_list_.erase(ref_list::iterator(child));
}

gcptr*
gcptr::new_root()
{
  return new gcptr(false);
}

void
gcollect(gcptr* ptr)
{
  the_mem.collect(ptr);
}
