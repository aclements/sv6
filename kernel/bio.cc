#include "types.h"
#include "kernel.hh"
#include "spinlock.h"
#include "condvar.h"
#include "bufcache.hh"
#include "cpputil.hh"
#include "ns.hh"
#include "kstream.hh"

static console_stream debug(true);

static uniqcache<buf> bufcache;

buf*
buf::get(u32 dev, u64 sector)
{
  return bufcache.get({ dev, sector });
}
