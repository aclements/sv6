// Helpers for declaring type-safe enum bit masks

#pragma once

#include <type_traits>

#define ENUM_BITSET_OPS(classname)                              \
  inline constexpr classname                                    \
  operator&(classname x, classname y)                           \
  {                                                             \
    typedef std::underlying_type<classname>::type int_type;     \
    return static_cast<classname>(                              \
      static_cast<int_type>(x) & static_cast<int_type>(y));     \
  }                                                             \
                                                                \
  inline constexpr classname                                    \
  operator|(classname x, classname y)                           \
  {                                                             \
    typedef std::underlying_type<classname>::type int_type;     \
    return static_cast<classname>(                              \
      static_cast<int_type>(x) | static_cast<int_type>(y));     \
  }                                                             \
                                                                \
  inline constexpr classname                                    \
  operator^(classname x, classname y)                           \
  {                                                             \
    typedef std::underlying_type<classname>::type int_type;     \
    return static_cast<classname>(                              \
      static_cast<int_type>(x) ^ static_cast<int_type>(y));     \
  }                                                             \
                                                                \
  inline constexpr classname                                    \
  operator~(classname x)                                        \
  {                                                             \
    typedef std::underlying_type<classname>::type int_type;     \
    return static_cast<classname>(~static_cast<int_type>(x));   \
  }                                                             \
                                                                \
  inline classname&                                             \
  operator&=(classname& x, classname y)                         \
  {                                                             \
    x = x & y; return x;                                        \
  }                                                             \
                                                                \
  inline classname&                                             \
  operator|=(classname& x, classname y)                         \
  {                                                             \
    x = x | y; return x;                                        \
  }                                                             \
                                                                \
  inline classname&                                             \
  operator^=(classname& x, classname y)                         \
  {                                                             \
    x = x ^ y; return x;                                        \
  }                                                             \
                                                                \
  static_assert(true, "semicolon required")
