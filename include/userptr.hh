#pragma once

#include <cstddef>
#include <memory>

// XXX(Austin) Avoiding TOCTTOU bugs when dealing with user-provided
// pointers in the presence of shared memory requires copying.  A
// possibly better approach would be to force some region of the
// address space to be thread-local and unshared and to require
// system call arguments to point in to this region.  We still have to
// validate such pointers (that the pages are mapped and that their
// length doesn't extend out of the region), but we wouldn't need to
// copy them in the kernel.

// A protected pointer to user data.  These act like pointers (and
// their in-memory representation is identical), but userptr's cannot
// be dereferenced without explicit checks.
template<typename T>
class userptr
{
  T* ptr;

public:
  userptr(std::nullptr_t n) : ptr(nullptr) { }
  explicit userptr(T* p) : ptr(p) { }
  userptr() = default;
  userptr(const userptr<T> &o) = default;
  userptr& operator=(const userptr& o) = default;

  // Up-conversion
  template<typename U, typename = typename
           std::enable_if<std::is_convertible<U*, T*>::value>::type>
  userptr(const userptr<U> &o) : ptr(o.ptr) { }

  template<typename U, typename = typename
           std::enable_if<std::is_convertible<U*, T*>::value>::type>
  userptr& operator=(const userptr<U>& o)
  {
    ptr = o.ptr;
  }

  T* unsafe_get() const
  {
    return ptr;
  }

  // Note that this has to be explicit or C++ will use this as the
  // conversion to *any* integral type, first converting through bool.
  explicit operator bool() const
  {
    return ptr != nullptr;
  }

  explicit operator uptr () const
  {
    return (uptr)ptr;
  }

  userptr operator+(ptrdiff_t x) const
  {
    return userptr(unsafe_get() + x);
  }

  // Store a value to this pointer.  Returns true if successful, false
  // if the user pointer is illegal.
  bool store(const T *val) const
  {
    return !putmem(unsafe_get(), val, sizeof(T));
  }

  bool store(const T *val, std::size_t count) const
  {
    return !putmem(unsafe_get(), val, sizeof(T) * count);
  }

  // Load a value from this pointer.  Returns true if successful,
  // false if the user pointer is illegal.
  bool load(T *val) const
  {
    if (sizeof(T) == sizeof(uint64_t))
      return !fetchint64((uptr)*this, reinterpret_cast<uint64_t*>(val));
    else
      return !fetchmem(val, unsafe_get(), sizeof(T));
  }

  bool load(T *val, std::size_t count) const
  {
    return fetchmem((void*)val, unsafe_get(), sizeof(T) * count) >= 0;
  }

  // Allocate memory for T[count] and copy into it.  If the pointer is
  // out of bounds, returns nullptr.  If memory allocation fails,
  // throws bad_alloc.
  std::unique_ptr<T[]>
  load_alloc(std::size_t count) const
  {
    std::unique_ptr<T[]> res(new T[count]);
    if (!load(res.get(), count))
      return nullptr;
    return res;
  }
};

// Specialization of userptr<void>
template<>
class userptr<void>
{
  void *ptr;

public:
  userptr(std::nullptr_t n) : ptr(nullptr) { }
  explicit userptr(void* p) : ptr(p) { }
  userptr() = default;
  userptr(const userptr<void> &o) = default;
  userptr& operator=(const userptr& o) = default;

  void *unsafe_get() const
  {
    return ptr;
  }

  explicit operator bool() const
  {
    return ptr != nullptr;
  }

  explicit operator uptr () const
  {
    return (uptr)ptr;
  }

  bool store_bytes(const void *val, std::size_t bytes) const
  {
    return !putmem(unsafe_get(), val, bytes);
  }

  bool load_bytes(void *val, std::size_t bytes) const
  {
    return !fetchmem(val, unsafe_get(), bytes);
  }
};

// For userptr to be passed like a regular pointer, its representation
// must be the same as a pointer (obviously) and, furthermore, the
// AMD64 ABI requires that it have a trivial copy construct and
// trivial destructor.
static_assert(sizeof(userptr<void>) == sizeof(void*), "userptr is wrong size");
static_assert(__is_pod(userptr<void>), "userptr is not a POD");

// A protected pointer to a string from user space.
class userptr_str
{
  userptr<const char> ptr;

public:
  userptr_str(std::nullptr_t n) : ptr(nullptr) { }
  explicit userptr_str(const char* p) : ptr(p) { }
  userptr_str() = default;
  userptr_str(const userptr_str &o) = default;
  userptr_str(userptr_str &&o) = default;
  userptr_str& operator=(const userptr_str& o) = default;

  explicit operator bool() const
  {
    return (bool)ptr;
  }

  bool load(char *dst, std::size_t size)
  {
    extern int fetchstr(char* dst, const char* usrc, u64 size);
    return !fetchstr(dst, ptr.unsafe_get(), size);
  }

  // Allocate memory for this string and copy into it.  If the pointer
  // is out of bounds or the string is longer than limit returns
  // nullptr.  If memory allocation fails, throws bad_alloc.  If
  // len_out is not nullptr, it will be set to strlen of the returned
  // string.
  std::unique_ptr<char[]> load_alloc(
    std::size_t limit, std::size_t *len_out = nullptr) const;
};
