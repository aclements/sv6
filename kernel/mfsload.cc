#include "types.h"
#include "kernel.hh"
#include "fs.h"
#include "file.hh"
#include "mnode.hh"
#include "linearhash.hh"
#include "mfs.hh"

static linearhash<u64, sref<mnode>> *inum_to_mnode;

static sref<mnode> load_inum(u64 inum);

static void
load_dir(sref<inode> i, sref<mnode> m)
{
  dirent de;
  for (size_t pos = 0; pos < i->size; pos += sizeof(de)) {
    assert(sizeof(de) == readi(i, (char*) &de, pos, sizeof(de)));
    if (!de.inum)
      continue;

    sref<mnode> mf = load_inum(de.inum);
    strbuf<DIRSIZ> name(de.name);
    if (name == ".")
      continue;

    mlinkref ilink(mf);
    ilink.acquire();
    m->as_dir()->insert(name, &ilink);
  }
}

static void
load_file(sref<inode> i, sref<mnode> m)
{
  for (size_t pos = 0; pos < i->size; pos += PGSIZE) {
    char* p = zalloc("load_file");
    assert(p);

    auto pi = sref<page_info>::transfer(new (page_info::of(p)) page_info());

    size_t nbytes = i->size - pos;
    if (nbytes > PGSIZE)
      nbytes = PGSIZE;

    assert(nbytes == readi(i, p, pos, nbytes));
    auto resize = m->as_file()->write_size();
    resize.resize_append(pos + nbytes, pi);
  }
}

static sref<mnode>
mnode_alloc(u64 inum, u8 mtype)
{
  sref<mnode> m = mnode::alloc(mtype);
  inum_to_mnode->insert(inum, m);
  return m;
}

static sref<mnode>
load_inum(u64 inum)
{
  sref<mnode> m;
  if (inum_to_mnode->lookup(inum, &m))
    return m;

  sref<inode> i = iget(1, inum);
  switch (i->type.load()) {
  case T_DIR:
    m = mnode_alloc(inum, mnode::types::dir);
    load_dir(i, m);
    break;

  case T_FILE:
    m = mnode_alloc(inum, mnode::types::file);
    load_file(i, m);
    break;

  default:
    cprintf("unhandled inode type %d\n", i->type.load());
  }

  return m;
}

void
mfsload()
{
  inum_to_mnode = new linearhash<u64, sref<mnode>>(4099);
  root_inum = load_inum(1)->inum_;
  /* the root inode gets an extra reference because of its own ".." */
  delete inum_to_mnode;
}
