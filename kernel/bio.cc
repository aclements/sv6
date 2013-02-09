#include "types.h"
#include "kernel.hh"
#include "buf.hh"
#include "weakcache.hh"

static weakcache<buf::key_t, buf, 257> bufcache;

sref<buf>
buf::get(u32 dev, u64 sector)
{
  buf::key_t k = { dev, sector };
  for (;;) {
    sref<buf> b = bufcache.lookup(k);
    if (b.get() != nullptr) {
      // Wait for buffer to load, by getting a read seqlock,
      // which waits for the write seqlock bit to be cleared.
      b->seq_.read_begin();
      return b;
    }

    sref<buf> nb = sref<buf>::transfer(new buf(dev, sector));
    auto locked = nb->write();
    if (bufcache.insert(k, nb.get())) {
      nb->inc();  // keep it in the cache
      ideread(dev, sector, locked->data);
      return nb;
    }
  }
}

void
buf::writeback()
{
  lock_guard<sleeplock> l(&writeback_lock_);
  mark_clean();
  auto copy = read();

  // write copy[] to disk; don't need to wait for write to finish,
  // as long as write order to disk has been established.
  idewrite(dev_, sector_, copy->data);
}

void
buf::onzero()
{
  bufcache.cleanup(weakref_);
  delete this;
}
