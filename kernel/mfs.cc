#include "types.h"
#include "kernel.hh"
#include "mfs.hh"

u64 root_inum;

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

  while (*path == '/')
    path++;
  if (*path == 0)
    return 0;
  s = path;
  while (*path != '/' && *path != 0)
    path++;
  len = path - s;
  if (len > DIRSIZ)
    return -1;
  else {
    memmove(name, s, len);
    if (len < DIRSIZ)
      name[len] = 0;
  }
  while (*path == '/')
    path++;
  *rpath = path;
  return 1;
}

// Look up and return the mnode for a path name.  If nameiparent is true,
// return the mnode for the parent and copy the final path element into name.
static sref<mnode>
namex(sref<mnode> cwd, const char* path, bool nameiparent, strbuf<DIRSIZ>* name)
{
  sref<mnode> m;

  if (*path == '/')
    m = mnode::get(root_inum);
  else
    m = cwd;

  int r;
  while ((r = skipelem(&path, name->buf_)) == 1) {
    if (m->type() != mnode::types::dir)
      return sref<mnode>();

    if (nameiparent && *path == '\0') {
      // Stop one level early.
      return m;
    }

    sref<mnode> next = m->as_dir()->lookup(*name);
    if (!next)
      return sref<mnode>();

    m = next;
  }

  if (r == -1 || nameiparent)
    return sref<mnode>();

  return m;
}

sref<mnode>
namei(sref<mnode> cwd, const char* path)
{
  strbuf<DIRSIZ> buf;
  return namex(cwd, path, false, &buf);
}

sref<mnode>
nameiparent(sref<mnode> cwd, const char* path, strbuf<DIRSIZ>* buf)
{
  return namex(cwd, path, true, buf);
}

s64
readi(sref<mnode> m, char* buf, u64 start, u64 nbytes)
{
  if (m->type() != mnode::types::file)
    return -1;

  u64 end = start + nbytes;
  u64 msize = *m->as_file()->read_size();
  if (end > msize)
    end = msize;

  u64 off = 0;
  while (start + off < end) {
    u64 pos = start + off;
    u64 pgbase = PGROUNDDOWN(pos);
    u64 pgoff = pos - pgbase;
    u64 pgend = end - pgbase;
    if (pgend > PGSIZE)
      pgend = PGSIZE;

    auto pi = m->as_file()->get_page(pgbase / PGSIZE);
    if (!pi)
      break;

    memmove(buf + off, (const char*) pi->va() + pgoff, pgend - pgoff);
    off += (pgend - pgoff);
  }

  return off;
}

s64
writei(sref<mnode> m, const char* buf, u64 start, u64 nbytes,
       mfile::resizer* parentresize)
{
  if (m->type() != mnode::types::file)
    return -1;

  u64 end = start + nbytes;
  u64 off = 0;
  while (start + off < end) {
    u64 pos = start + off;
    u64 pgbase = PGROUNDDOWN(pos);
    u64 pgoff = pos - pgbase;
    u64 pgend = end - pgbase;
    if (pgend > PGSIZE)
      pgend = PGSIZE;

    mfile::resizer *resize = parentresize;
    mfile::resizer scoped_resize;
    u64 msize = resize ? resize->read_size() : *m->as_file()->read_size();
    if (pos + pgend - pgoff > msize) {
      if (resize == nullptr) {
        scoped_resize = m->as_file()->write_size();
        resize = &scoped_resize;
      }
      msize = resize->read_size();
    }

    sref<page_info> pi;
    if (msize > pgbase) {
      /* File already has the page we are about to update */
      pi = m->as_file()->get_page(pgbase / PGSIZE);
      if (!pi)
        break;

      memmove((char*) pi->va() + pgoff, buf + off, pgend - pgoff);
      if (resize && *resize)
        resize->resize_nogrow(pos + pgend - pgoff);
    } else {
      /* File does not yet have the page we are about to update */
      assert(resize && *resize);

      char* p = zalloc("file page");
      if (!p)
        break;

      memmove(p + pgoff, buf + off, pgend - pgoff);
      pi = sref<page_info>::transfer(new (page_info::of(p)) page_info());
      resize->resize_append(pos + pgend - pgoff, pi);
    }

    off += (pgend - pgoff);
  }

  return off ?: -1;
}
