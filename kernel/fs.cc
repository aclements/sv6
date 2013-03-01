// File system implementation.  Four layers:
//   + Blocks: allocator for raw disk blocks.
//   + Files: inode allocator, reading, writing, metadata.
//   + Directories: inode with special contents (list of other inodes!)
//   + Names: paths like /usr/rtm/xv6/fs.c for convenient naming.
//
// Disk layout is: superblock, inodes, block in-use bitmap, data blocks.
//
// This file contains the low-level file system manipulation 
// routines.  The (higher-level) system call implementations
// are in sysfile.c.

/*
 * inode cache will be RCU-managed:
 * 
 * - to evict, mark inode as a victim
 * - lookups that encounter a victim inode must return an error (-E_RETRY)
 * - E_RETRY rolls back to the beginning of syscall/pagefault and retries
 * - out-of-memory error should be treated like -E_RETRY
 * - once an inode is marked as victim, it can be gc_delayed()
 * - the do_gc() method should remove inode from the namespace & free it
 * 
 * - inodes have a refcount that lasts beyond a GC epoch
 * - to bump refcount, first bump, then check victim flag
 * - if victim flag is set, reduce the refcount and -E_RETRY
 *
 */

#include "types.h"
#include <uk/stat.h>
#include "mmu.h"
#include "kernel.hh"
#include "spinlock.h"
#include "condvar.h"
#include "queue.h"
#include "proc.hh"
#include "fs.h"
#include "buf.hh"
#include "file.hh"
#include "cpu.hh"
#include "kmtrace.hh"
#include "dirns.hh"
#include "kstream.hh"
#include "lb.hh"

#define min(a, b) ((a) < (b) ? (a) : (b))
static sref<inode> the_root;

#define IADDRSSZ (sizeof(u32)*NINDIRECT)

// Read the super block.
static void
readsb(int dev, struct superblock *sb)
{
  sref<buf> bp = buf::get(dev, 1);
  auto copy = bp->read();
  memmove(sb, copy->data, sizeof(*sb));
}

// Zero a block.
static void
bzero(int dev, int bno)
{
  sref<buf> bp = buf::get(dev, bno);
  auto locked = bp->write();
  memset(locked->data, 0, BSIZE);
}

//
// Blocks
//
class out_of_blocks : public std::exception
{
  virtual const char* what() const throw() override
  {
    return "Out of blocks";
  }
};

static void inline
throw_out_of_blocks()
{
#if EXCEPTIONS
  throw out_of_blocks();
#else
  panic("out of blocks");
#endif
}

// Allocate a disk block.
static u32
balloc(u32 dev)
{
  superblock sb;
  readsb(dev, &sb);

  for(int b = 0; b < sb.size; b += BPB){
    sref<buf> bp = buf::get(dev, BBLOCK(b, sb.ninodes));
    auto locked = bp->write();

    for(int bi = 0; bi < BPB && bi < (sb.size - b); bi++){
      int m = 1 << (bi % 8);
      if((locked->data[bi/8] & m) == 0){  // Is block free?
        locked->data[bi/8] |= m;  // Mark block in use on disk.
        return b + bi;
      }
    }
  }
  
  throw_out_of_blocks();
  // Unreachable
  return 0;
}

// Free a disk block.
static void
bfree(int dev, u64 x)
{
  u32 b = x;
  bzero(dev, b);

  struct superblock sb;
  readsb(dev, &sb);

  sref<buf> bp = buf::get(dev, BBLOCK(b, sb.ninodes));
  auto locked = bp->write();

  int bi = b % BPB;
  int m = 1 << (bi % 8);
  if((locked->data[bi/8] & m) == 0)
    panic("freeing free block");
  locked->data[bi/8] &= ~m;  // Mark block free on disk.
}

// Inodes.
//
// An inode is a single, unnamed file in the file system.
// The inode disk structure holds metadata (the type, device numbers,
// and data size) along with a list of blocks where the associated
// data can be found.
//
// The inodes are laid out sequentially on disk immediately after
// the superblock.  The kernel keeps a cache of the in-use
// on-disk structures to provide a place for synchronizing access
// to inodes shared between multiple processes.
// 
// ip->ref counts the number of pointer references to this cached
// inode; references are typically kept in struct file and in proc->cwd.
// When ip->ref falls to zero, the inode is no longer cached.
// It is an error to use an inode without holding a reference to it.
//
// Processes are only allowed to read and write inode
// metadata and contents when holding the inode's lock,
// represented by the I_BUSY flag in the in-memory copy.
// Because inode locks are held during disk accesses, 
// they are implemented using a flag rather than with
// spin locks.  Callers are responsible for locking
// inodes before passing them to routines in this file; leaving
// this responsibility with the caller makes it possible for them
// to create arbitrarily-sized atomic operations.
//
// To give maximum control over locking to the callers, 
// the routines in this file that return inode pointers 
// return pointers to *unlocked* inodes.  It is the callers'
// responsibility to lock them before using them.  A non-zero
// ip->ref keeps these unlocked inodes in the cache.

u64
ino_hash(const pair<u32, u32> &p)
{
  return p.first ^ p.second;
}

static nstbl<pair<u32, u32>, inode*, ino_hash> *ins;

void
initinode(void)
{
  scoped_gc_epoch e;

  ins = new nstbl<pair<u32, u32>, inode*, ino_hash>();
  the_root = inode::alloc(ROOTDEV, ROOTINO);
  if (!ins->insert({the_root->dev, the_root->inum}, the_root.get()))
    panic("initinode: insert the_root failed");
  the_root->init();

  if (VERBOSE) {
    struct superblock sb;
    u64 blocks;

    readsb(ROOTDEV, &sb);
    blocks = sb.ninodes/IPB;
    cprintf("initinode: %lu inode blocks (%lu / core)\n",
            blocks, blocks/NCPU);
  }
}

template<size_t N>
struct inode_cache;

template<size_t N>
struct inode_cache : public balance_pool<inode_cache<N>>
{
  inode_cache()
    : balance_pool<inode_cache<N>> (N), 
      head_(0), length_(0), lock_("inode_cache", LOCKSTAT_FS)
  {
  }
  
  int
  alloc()
  {
    scoped_acquire _l(&lock_);
    return alloc_nolock();
  }

  void
  add(u32 inum)
  {
    scoped_acquire _l(&lock_);
    add_nolock(inum);
  }

  void
  balance_move_to(inode_cache<N>* target)
  {
    if (target < this) {
      target->lock_.acquire();
      lock_.acquire();
    } else {
      lock_.acquire();
      target->lock_.acquire();
    }

    u32 nmove = length_ / 2;
    for (; nmove; nmove--) {
      int inum = alloc_nolock();
      if (inum < 0) {
        console.println("inode_cache: unexpected failure");
        break;
      }
      target->add_nolock(inum);
    }

    if (target < this) {
      target->lock_.release();
      lock_.release();
    } else {
      lock_.release();
      target->lock_.release();
    }
  }

  u64
  balance_count() const
  {
    return length_;
  }

private:

  int
  alloc_nolock()
  {
    int inum = -1;
    if (length_) {
      length_--;
      head_--;
      inum = cache_[head_ % N];
    }
    return inum;
  }

  void
  add_nolock(u32 inum)
  {
    assert(inum != 0);
    if (length_ < N)
      length_++;
    cache_[head_ % N] = inum;
    head_++;
  }

  u32      cache_[N];
  u32      head_;
  u32      length_;
  spinlock lock_;
};

struct inode_cache_dir
{
  inode_cache_dir() : balancer_(this)
  {
  }

  inode_cache<512>*
  balance_get(int id) const
  {
    return &cache_[id];
  }

  void
  add(u32 inum)
  {
    // XXX(sbw) if cache->length_ == N should we call 
    // balancer_.balance()?
    cache_->add(inum);
  }

  int
  alloc()
  {
    int inum = cache_->alloc();
    if (inum > 0)
      return inum;
    balancer_.balance();
    return cache_->alloc();
  }

private:

  percpu<inode_cache<512>, NO_CRITICAL> cache_;
  balancer<inode_cache_dir, inode_cache<512>> balancer_;
};

static inode_cache_dir the_inode_cache;

static sref<inode>
try_ialloc(u32 inum, u32 dev, short type)
{
  sref<inode> ip = iget(dev, inum);
  if (ip->type || !cmpxch(&ip->type, (short) 0, type))
    return sref<inode>();

  ilock(ip, 1);
  auto w = ip->seq.write_begin();
  ip->gen += 1;
  if(ip->nlink() || ip->size || ip->addrs[0])
    panic("ialloc not zeroed");
  return ip;
}

// Allocate a new inode with the given type on device dev.
// Returns a locked inode.
sref<inode>
ialloc(u32 dev, short type)
{
  // XXX should be replaced with lb?

  scoped_gc_epoch e;

  superblock sb;
  sref<inode> ip;
  int inum;

  // Try the local cache first..
  while ((inum = the_inode_cache.alloc()) > 0) {
    ip = try_ialloc(inum, dev, type);
    if (ip)
      return ip;
  }

  // search through this core's inodes
  readsb(dev, &sb);
  for (int k = myid()*IPB; k < sb.ninodes; k += (NCPU*IPB)) {
    for(inum = k; inum < k+IPB && inum < sb.ninodes; inum++) {
      if (inum == 0)
        continue;
      ip = try_ialloc(inum, dev, type);
      if (ip)
        return ip;
    }
  }

  // search through all inodes
  for (int inum = 0; inum < sb.ninodes; inum++) {
    if (inum == 0)
      continue;
    ip = try_ialloc(inum, dev, type);
    if (ip)
      return ip;
  }

  cprintf("ialloc: 0/%u inodes\n", sb.ninodes);
  return sref<inode>();
}

// Copy inode, which has changed, from memory to disk.
void
iupdate(struct inode *ip)
{
  // XXX call iupdate to flush in-memory inode state to
  // buffer cache.  use seq value to detect updates.

  scoped_gc_epoch e;

  {
    sref<buf> bp = buf::get(ip->dev, IBLOCK(ip->inum));
    auto locked = bp->write();

    dinode *dip = (struct dinode*)locked->data + ip->inum%IPB;
    dip->type = ip->type;
    dip->major = ip->major;
    dip->minor = ip->minor;
    dip->nlink = ip->nlink();
    dip->size = ip->size;
    dip->gen = ip->gen;
    memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
  }

  if (ip->addrs[NDIRECT] != 0) {
    assert(ip->iaddrs.load() != nullptr);
    sref<buf> bp = buf::get(ip->dev, ip->addrs[NDIRECT]);
    auto locked = bp->write();
    memmove(locked->data, (void*)ip->iaddrs.load(), IADDRSSZ);
  }
}

// Find the inode with number inum on device dev
// and return the in-memory copy.
// The inode is not locked, so someone else might
// be modifying it.
// But it has a ref count, so it won't be freed or reused.
// Though unlocked, all fields will be present,
// so looking a ip->inum and ip->gen are OK even w/o lock.
inode::inode(u32 d, u32 i)
  : rcu_freed("inode", this, sizeof(*this)),
    dev(d), inum(i),
    valid(false),
    busy(false),
    readbusy(0)
{
  dir.store(nullptr);
  iaddrs.store(nullptr);
}

inode::~inode()
{
  auto d = dir.load();
  if (d) {
    d->remove(strbuf<DIRSIZ>("."));
    d->remove(strbuf<DIRSIZ>(".."));
    gc_delayed(d);
    assert(cmpxch(&dir, d, (decltype(d)) 0));
  }
  if (iaddrs.load() != nullptr) {
    kmfree((void*)iaddrs.load(), IADDRSSZ);
    iaddrs.store(nullptr);
  }
}

sref<inode>
iget(u32 dev, u32 inum)
{
  sref<inode> ip;

  // Assumes caller is holding a gc_epoch

 retry:
  // Try for cached inode.
  ip = sref<inode>::newref(ins->lookup(make_pair(dev, inum)));
  if (ip) {
    if (!ip->valid) {
      acquire(&ip->lock);
      while(!ip->valid)
        ip->cv.sleep(&ip->lock);
      release(&ip->lock);
    }
    return ip;
  }
  
  // Allocate fresh inode cache slot.
  ip = inode::alloc(dev, inum);
  if (ip == nullptr)
    panic("iget: should throw_bad_alloc()");
  
  // Lock the inode
  ip->busy = true;
  ip->readbusy = 1;

  if (!ins->insert({ip->dev, ip->inum}, ip.get())) {
    iunlock(ip);
    // reference counting will clean up memory allocation.
    goto retry;
  }

  ip->init();
  iunlock(ip);
  return ip;
}

sref<inode>
inode::alloc(u32 dev, u32 inum)
{
  sref<inode> ip = sref<inode>::transfer(new inode(dev, inum));
  if (ip == nullptr)
    return sref<inode>();

  snprintf(ip->lockname, sizeof(ip->lockname), "cv:ino:%d", ip->inum);
  ip->lock = spinlock(ip->lockname+3, LOCKSTAT_FS);
  ip->cv = condvar(ip->lockname);
  return ip;
}

void
inode::init(void)
{
  scoped_gc_epoch e;
  sref<buf> bp = buf::get(dev, IBLOCK(inum));
  auto copy = bp->read();
  const dinode *dip = (const struct dinode*)copy->data + inum%IPB;

  type = dip->type;
  major = dip->major;
  minor = dip->minor;
  nlink_ = dip->nlink;
  size = dip->size;
  gen = dip->gen;
  memmove(addrs, dip->addrs, sizeof(addrs));

  if (nlink_ > 0)
    inc();
  valid = true;
}

void
inode::link(void)
{
  // Must hold ilock if inode is accessible by multiple threads
  auto w = seq.write_begin();
  if (++nlink_ == 1) {
    // A non-zero nlink_ holds a reference to the inode
    inc();
  }
}

void
inode::unlink(void)
{
  // Must hold ilock if inode is accessible by multiple threads
  auto w = seq.write_begin();
  if (--nlink_ == 0) {
    // This should never be the last reference..
    dec();
  }
}

short
inode::nlink(void)
{
  // Must hold ilock if inode is accessible by multiple threads
  return nlink_;
}

void
inode::onzero(void)
{
  acquire(&lock);
  if (nlink())
    panic("iput [%p]: nlink %u\n", this, nlink());

  // inode is no longer used: truncate and free inode.
  if(busy || readbusy) {
    // race with iget
    panic("iput busy");
  }
  if(!valid)
    panic("iput not valid");
  
  busy = true;
  readbusy++;

  // XXX: use gc_delayed() to truncate the inode later.
  // flag it as a victim in the meantime.

  itrunc(this);

  {
    auto w = seq.write_begin();
    type = 0;
    major = 0;
    minor = 0;
    gen += 1;
  }

  release(&lock);
 
  inode* ip = this; 
  ins->remove(make_pair(dev, inum), &ip);
  the_inode_cache.add(inum);
  gc_delayed(ip);
  return;
}

// Lock the given inode.
// XXX why does ilock() read the inode from disk?
// why doesn't the iget() that allocated the inode cache entry
// read the inode from disk?
void
ilock(sref<inode> ip, int writer)
{
  if(ip == 0)
    panic("ilock");

  acquire(&ip->lock);
  if (writer) {
    while(ip->busy || ip->readbusy)
      ip->cv.sleep(&ip->lock);
    ip->busy = true;
  } else {
    while(ip->busy)
      ip->cv.sleep(&ip->lock);
  }
  ip->readbusy++;
  release(&ip->lock);

  if(!ip->valid)
    panic("ilock");
}

// Unlock the given inode.
void
iunlock(sref<inode> ip)
{
  if(ip == 0)
    panic("iunlock");
  if(!ip->readbusy && !ip->busy)
    panic("iunlock");

  acquire(&ip->lock);
  --ip->readbusy;
  ip->busy = false;
  ip->cv.wake_all();
  release(&ip->lock);
}

//PAGEBREAK!
// Inode contents
//
// The contents (data) associated with each inode is stored
// in a sequence of blocks on the disk.  The first NDIRECT blocks
// are listed in ip->addrs[].  The next NINDIRECT blocks are 
// listed in the block ip->addrs[NDIRECT].  The next NINDIRECT^2
// blocks are doubly-indirect from ip->addrs[NDIRECT+1].

// Return the disk block address of the nth block in inode ip.
// If there is no such block, bmap allocates one.
static u32
bmap(sref<inode> ip, u32 bn)
{
  scoped_gc_epoch e;

  u32* ap;
  u32 addr;

  if(bn < NDIRECT){
  retry0:
    if((addr = ip->addrs[bn]) == 0) {
      addr = balloc(ip->dev);
      if (!cmpxch(&ip->addrs[bn], (u32)0, addr)) {
        bfree(ip->dev, addr);
        goto retry0;
      }
    }
    return addr;
  }
  bn -= NDIRECT;

  if (bn < NINDIRECT) {
  retry1:
    if (ip->iaddrs == nullptr) {
      if((addr = ip->addrs[NDIRECT]) == 0) {
        addr = balloc(ip->dev);
        if (!cmpxch(&ip->addrs[NDIRECT], (u32)0, addr)) {
          bfree(ip->dev, addr);
          goto retry1;
        }
      }

      volatile u32* iaddrs = (u32*)kmalloc(IADDRSSZ, "iaddrs");
      sref<buf> bp = buf::get(ip->dev, addr);
      auto copy = bp->read();
      memmove((void*)iaddrs, copy->data, IADDRSSZ);

      if (!cmpxch(&ip->iaddrs, (volatile u32*)nullptr, iaddrs)) {
        bfree(ip->dev, addr);
        kmfree((void*)iaddrs, IADDRSSZ);
        goto retry1;
      }
    }

  retry2:
    if ((addr = ip->iaddrs[bn]) == 0) {
      addr = balloc(ip->dev);
      if (!__sync_bool_compare_and_swap(&ip->iaddrs[bn], (u32)0, addr)) {
        bfree(ip->dev, addr);
        goto retry2;
      }
    }

    return addr;
  }
  bn -= NINDIRECT;

  if (bn >= NINDIRECT * NINDIRECT)
    panic("bmap: %d out of range", bn);

  // Doubly-indirect blocks are currently "slower" because we do not
  // cache an equivalent of ip->iaddrs.

retry3:
  if (ip->addrs[NDIRECT+1] == 0) {
    addr = balloc(ip->dev);
    if (!cmpxch(&ip->addrs[NDIRECT+1], (u32)0, addr)) {
      bfree(ip->dev, addr);
      goto retry3;
    }
  }

  sref<buf> wb = buf::get(ip->dev, ip->addrs[NDIRECT+1]);

  for (;;) {
    auto copy = wb->read();
    ap = (u32*)copy->data;
    if (ap[bn / NINDIRECT] == 0) {
      auto locked = wb->write();
      ap = (u32*)locked->data;
      if (ap[bn / NINDIRECT] == 0)
        ap[bn / NINDIRECT] = balloc(ip->dev);
      continue;
    }
    addr = ap[bn / NINDIRECT];
    break;
  }

  wb = buf::get(ip->dev, addr);

  for (;;) {
    auto copy = wb->read();
    ap = (u32*)copy->data;
    if (ap[bn % NINDIRECT] == 0) {
      auto locked = wb->write();
      ap = (u32*)locked->data;
      if (ap[bn % NINDIRECT] == 0)
        ap[bn % NINDIRECT] = balloc(ip->dev);
      continue;
    }
    addr = ap[bn % NINDIRECT];
    break;
  }

  return addr;
}

// Truncate inode (discard contents).
// Only called after the last dirent referring
// to this inode has been erased on disk.
class diskblock : public rcu_freed {
 private:
  int _dev;
  u64 _block;

 public:
  diskblock(int dev, u64 block)
    : rcu_freed("diskblock", this, sizeof(*this)),
      _dev(dev), _block(block) {}
  virtual void do_gc() override {
    scoped_gc_epoch e;
    bfree(_dev, _block);
    delete this;
  }

  NEW_DELETE_OPS(diskblock)
};

void
itrunc(struct inode *ip)
{
  scoped_gc_epoch e;

  // XXX how to serialize itrunc w.r.t. concurrent itrunc or expansion?
  // Could lock disk blocks (buf's), or could lock the inode?

  auto w = ip->seq.write_begin();

  for(int i = 0; i < NDIRECT; i++){
    if(ip->addrs[i]){
      diskblock *db = new diskblock(ip->dev, ip->addrs[i]);
      gc_delayed(db);
      ip->addrs[i] = 0;
    }
  }
  
  if(ip->addrs[NDIRECT]){
    sref<buf> bp = buf::get(ip->dev, ip->addrs[NDIRECT]);
    auto copy = bp->read();
    u32* a = (u32*)copy->data;
    for(int i = 0; i < NINDIRECT; i++){
      if(a[i]) {
        diskblock *db = new diskblock(ip->dev, a[i]);
        gc_delayed(db);
      }
    }

    diskblock *db = new diskblock(ip->dev, ip->addrs[NDIRECT]);
    gc_delayed(db);
    ip->addrs[NDIRECT] = 0;
  }

  if(ip->addrs[NDIRECT+1]){
    sref<buf> bp1 = buf::get(ip->dev, ip->addrs[NDIRECT+1]);
    auto copy1 = bp1->read();
    u32* a1 = (u32*)copy1->data;
    for(int i = 0; i < NINDIRECT; i++){
      if(!a1[i])
        continue;

      sref<buf> bp2 = buf::get(ip->dev, a1[i]);
      auto copy2 = bp2->read();
      u32* a2 = (u32*)copy2->data;
      for(int j = 0; j < NINDIRECT; j++){
        if(!a2[j])
          continue;

        diskblock* db2 = new diskblock(ip->dev, a2[j]);
        gc_delayed(db2);
      }

      diskblock* db1 = new diskblock(ip->dev, a1[i]);
      gc_delayed(db1);
    }

    diskblock *db = new diskblock(ip->dev, ip->addrs[NDIRECT+1]);
    gc_delayed(db);
    ip->addrs[NDIRECT+1] = 0;
  }

  ip->size = 0;
}

//PAGEBREAK!
// Read data from inode.
int
readi(sref<inode> ip, char *dst, u32 off, u32 n)
{
  scoped_gc_epoch e;

  u32 tot, m;
  sref<buf> bp;

  if(ip->type == T_DEV)
    return -1;

  if(off > ip->size || off + n < off)
    return -1;
  if(off + n > ip->size)
    n = ip->size - off;

  for(tot=0; tot<n; tot+=m, off+=m, dst+=m){
    try {
      bp = buf::get(ip->dev, bmap(ip, off/BSIZE));
    } catch (out_of_blocks& e) {
      // Read operations should never cause out-of-blocks conditions
      panic("readi: out of blocks");
    }
    m = min(n - tot, BSIZE - off%BSIZE);

    auto copy = bp->read();
    memmove(dst, copy->data + off%BSIZE, m);
  }
  return n;
}

// PAGEBREAK!
// Write data to inode.
int
writei(sref<inode> ip, const char *src, u32 off, u32 n)
{
  scoped_gc_epoch e;

  int tot, m;
  sref<buf> bp;

  if(ip->type == T_DEV)
    return -1;

  if(off > ip->size || off + n < off)
    return -1;
  if(off + n > MAXFILE*BSIZE)
    n = MAXFILE*BSIZE - off;

  for(tot=0; tot<n; tot+=m, off+=m, src+=m){
    try {
      bp = buf::get(ip->dev, bmap(ip, off/BSIZE));
    } catch (out_of_blocks& e) {
      console.println("writei: out of blocks");
      // If we haven't written anything, return an error
      if (tot == 0)
        tot = -1;
      break;
    }
    m = min(n - tot, BSIZE - off%BSIZE);
    auto locked = bp->write();
    memmove(locked->data + off%BSIZE, src, m);
  }

  if(tot > 0 && off > ip->size){
    auto w = ip->seq.write_begin();
    ip->size = off;
  }
  return tot;
}

//PAGEBREAK!
// Directories

int
namecmp(const char *s, const char *t)
{
  return strncmp(s, t, DIRSIZ);
}

u64
namehash(const strbuf<DIRSIZ> &n)
{
  u64 h = 0;
  for (int i = 0; i < DIRSIZ && n.buf_[i]; i++) {
    u64 c = n.buf_[i];
    // Lifted from dcache.h in Linux v3.3
    h = (h + (c << 4) + (c >> 4)) * 11;
    // XXX(sbw) this doesn't seem to do well with the names
    // in dirbench (the low-order bits get clumped).
    // h = ((h << 8) ^ c) % 0xdeadbeef;
  }
  return h;
}

void
dir_init(sref<inode> dp)
{
  scoped_gc_epoch e;

  if (dp->dir)
    return;
  if (dp->type != T_DIR)
    panic("dir_init not DIR");

  auto dir = new dirns();
  for (u32 off = 0; off < dp->size; off += BSIZE) {
    sref<buf> bp;
    try {
      bp = buf::get(dp->dev, bmap(dp, off / BSIZE));
    } catch (out_of_blocks& e) {
      // Read operations should never cause out-of-blocks conditions
      panic("dir_init: out of blocks");
    }
    auto copy = bp->read();
    for (const struct dirent *de = (const struct dirent *) copy->data;
	 de < (const struct dirent *) (copy->data + BSIZE);
	 de++) {
      if (de->inum)
        dir->insert(strbuf<DIRSIZ>(de->name), de->inum);
    }
  }

  if (!cmpxch(&dp->dir, (decltype(dir)) 0, dir)) {
    // XXX free all the dirents
    delete dir;
  }
}

void
dir_flush(sref<inode> dp)
{
  // assume already locked

  if (!dp->dir)
    return;

  u32 off = 0;
  dp->dir.load()->enumerate([dp, &off](const strbuf<DIRSIZ> &name, const u32 &inum)->bool{
      struct dirent de;
      strncpy(de.name, name.buf_, DIRSIZ);
      de.inum = inum;
      if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
        panic("dir_flush_cb");
      off += sizeof(de);
      return false;
    });
  if (dp->size != off) {
    auto w = dp->seq.write_begin();
    dp->size = off;
  }
}

// Look for a directory entry in a directory.
sref<inode>
dirlookup(sref<inode> dp, char *name)
{
  dir_init(dp);

  u32 inum = dp->dir.load()->lookup(strbuf<DIRSIZ>(name));

  if (inum == 0)
    return sref<inode>();
  return iget(dp->dev, inum);
}

// Write a new directory entry (name, inum) into the directory dp.
int
dirlink(sref<inode> dp, const char *name, u32 inum)
{
  dir_init(dp);

  //cprintf("dirlink: %x (%d): %s -> %d\n", dp, dp->inum, name, inum);
  if (!dp->dir.load()->insert(strbuf<DIRSIZ>(name), inum))
    return -1;

  return 0;
}

// Paths

// Copy the next path element from path into name.
// Update the pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// 
// If copied into name, return 1.
// If no name to remove, return 0.
// If the name is longer than DIRSIZ, return -1;
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
//
static int
skipelem(const char **rpath, char *name)
{
  const char *path = *rpath;
  const char *s;
  int len;

  while(*path == '/')
    path++;
  if(*path == 0)
    return 0;
  s = path;
  while(*path != '/' && *path != 0)
    path++;
  len = path - s;
  if(len > DIRSIZ)
    return -1;
  else {
    memmove(name, s, len);
    if (len < DIRSIZ)
      name[len] = 0;
  }
  while(*path == '/')
    path++;
  *rpath = path;
  return 1;
}

// Look up and return the inode for a path name.
// If parent != 0, return the inode for the parent and copy the final
// path element into name, which must have room for DIRSIZ bytes.
static sref<inode>
namex(sref<inode> cwd, const char *path, int nameiparent, char *name)
{
  // Assumes caller is holding a gc_epoch

  sref<inode> ip;
  sref<inode> next;
  int r;

  if(*path == '/')
    ip = the_root;
  else
    ip = cwd;

  while((r = skipelem(&path, name)) == 1){
    // XXX Doing this here requires some annoying reasoning about all
    // of the callers of namei/nameiparent.  Also, since the abstract
    // scope is implicit, it might be wrong (or non-existent) and
    // documenting the abstract object sets of each scope becomes
    // difficult and probably unmaintainable.  We have to compute this
    // information here because it's the only place that's canonical.
    // Maybe this should return the set of inodes traversed and let
    // the caller declare the variables?  Would it help for the caller
    // to pass in an abstract scope?
    mtreadavar("inode:%x.%x", ip->dev, ip->inum);
    if(ip->type == 0)
      panic("namex");
    if(ip->type != T_DIR)
      return sref<inode>();
    if(nameiparent && *path == '\0'){
      // Stop one level early.
      return ip;
    }

    if((next = dirlookup(ip, name)) == 0)
      return sref<inode>();
    ip = next;
  }

  if(r == -1 || nameiparent)
    return sref<inode>();

  return ip;
}

sref<inode>
namei(sref<inode> cwd, const char *path)
{
  // Assumes caller is holding a gc_epoch
  char name[DIRSIZ];
  return namex(cwd, path, 0, name);
}

sref<inode>
nameiparent(sref<inode> cwd, const char *path, char *name)
{
  // Assumes caller is holding a gc_epoch
  return namex(cwd, path, 1, name);
}
