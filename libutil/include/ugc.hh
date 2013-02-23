#include "ilist.hh"

typedef unsigned char mark_sense_t;
class gcd_memory;
class gcptr;

struct gcptr_ref
{
  class gcptr*     ptr;
  ilink<gcptr_ref> link;
};

class gcptr
{
  typedef ilist<gcptr_ref, &gcptr_ref::link> ref_list;

  friend gcd_memory;

public:
  gcptr();
  virtual ~gcptr();
  gcptr_ref* ref(gcptr* child);
  void uref(gcptr_ref* child);
  static gcptr* new_root();

  gcptr(const gcptr &o) = delete;
  gcptr(gcptr &&o) = delete;
  gcptr &operator=(const gcptr &o) = delete;
  gcptr &operator=(gcptr &&o) = delete;

  // Our link in the node list
  ilink<gcptr> node_link_;
  // Our link in the work list
  ilink<gcptr> work_link_;

private:
  gcptr(bool track);
  mark_sense_t mark_;

  // Everyone that we reference
  ref_list ref_list_;
};

void gcollect(gcptr* ptr);
