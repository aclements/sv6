#pragma once

template<class T>
u64
hash(const T& v);

template<>
inline u64
hash(const u64& v)
{
  return v;
}
