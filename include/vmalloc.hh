#pragma once

#include <typeinfo>
#include <memory>

// vmalloc is a virtual memory-mapped allocator.  It is less efficient
// than kalloc/kmalloc, but can satisfy arbitrarily large allocations
// and surrounds allocations with unmapped guard memory for improved
// safety.

void *vmalloc_raw(size_t bytes, size_t guard, const char *name);
void vmalloc_free(void *ptr);

// Managed single-owner pointer to vmalloc'd memory
template<class T>
using vmalloc_ptr = std::unique_ptr<T, decltype(&vmalloc_free)>;

// vmalloc for single objects
template<class T, class... Args, typename = typename
         std::enable_if<!std::is_array<T>::value>::type>
vmalloc_ptr<T> vmalloc(Args&& ...args)
{
  void *res = vmalloc_raw(sizeof(T), PGSIZE, typeid(T).name());
  return vmalloc_ptr<T>(new (res) T(std::forward<Args>(args)...), vmalloc_free);
}

// vmalloc for arrays
template<class T, class... Args, typename = typename
         std::enable_if<std::is_array<T>::value && std::extent<T>::value == 0>::type>
vmalloc_ptr<T> vmalloc(size_t bound, Args&& ...args)
{
  typedef typename std::remove_extent<T>::type base;
  void *res = vmalloc_raw(bound * sizeof(base), PGSIZE, typeid(T).name());
  return vmalloc_ptr<T>(new (res) base[bound](), vmalloc_free);
}
