#pragma once

#include "cpputil.hh"
#include "fs.h"

template<class T>
u64
hash(const T& v);

template<>
inline u64
hash(const u64& v)
{
  return v;
}

template<>
inline u64
hash(const strbuf<DIRSIZ>& v)
{
  u64 h = 0;
  for (int i = 0; i < DIRSIZ && v.buf_[i]; i++) {
    u64 c = v.buf_[i];
    // Lifted from dcache.h in Linux v3.3
    h = (h + (c << 4) + (c >> 4)) * 11;
  }
  return h;
}
