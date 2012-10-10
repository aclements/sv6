#pragma once

#include <cstddef>

// Return ceil(log2(x)).
static inline std::size_t
ceil_log2(std::size_t x)
{
  auto bits = sizeof(long long) * 8 - __builtin_clzll(x);
  if (x == (std::size_t)1 << (bits - 1))
    return bits - 1;
  return bits;
}

// Round up to the nearest power of 2
static inline std::size_t
round_up_to_pow2(std::size_t x)
{
  auto bits = sizeof(long long) * 8 - __builtin_clzll(x);
  if (x == (std::size_t)1 << (bits - 1))
    return x;
  return (std::size_t)1 << bits;
}
