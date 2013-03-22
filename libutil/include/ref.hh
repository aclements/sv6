#pragma once

#include <cstdint>
#include <type_traits>
#include <utility>

template <class T, typename = void>
class sref {
private:
  constexpr explicit sref(T* p) noexcept : ptr_(p) { }

  // Allow access to ptr_ from up-conversions
  template<typename, typename>
  friend class sref;

public:
  constexpr sref() noexcept : ptr_(nullptr) { }

  // Yes, we need to duplicate the standard and is_convertible
  // constructors and assignment methods, even though the
  // is_convertible ones logically suffice.  If we don't write the
  // standard methods, C++ will helpfully give us default
  // implementations for them.

  sref(const sref &o) : ptr_(o.ptr_) {
    if (ptr_)
      ptr_->inc();
  }

  template<typename U, typename = typename
           std::enable_if<std::is_convertible<U*, T*>::value>::type>
  sref(const sref<U> &o) : ptr_(o.ptr_) {
    if (ptr_)
      ptr_->inc();
  }

  sref(sref &&o) noexcept : ptr_(o.ptr_)
  {
    o.ptr_ = nullptr;
  }

  template<typename U, typename = typename
           std::enable_if<std::is_convertible<U*, T*>::value>::type>
  sref(sref<U> &&o) noexcept : ptr_(o.ptr_)
  {
    o.ptr_ = nullptr;
  }

  ~sref() {
    if (ptr_)
      ptr_->dec();
  }

  sref& operator=(const sref& o) {
    T *optr = o.ptr_;
    if (optr != ptr_) {
      if (optr)
        optr->inc();
      if (ptr_)
        ptr_->dec();
      ptr_ = optr;
    }
    return *this;
  }

  template<typename U, typename = typename
           std::enable_if<std::is_convertible<U*, T*>::value>::type>
  sref& operator=(const sref<U>& o) {
    T *optr = o.ptr_;
    if (optr != ptr_) {
      if (optr)
        optr->inc();
      if (ptr_)
        ptr_->dec();
      ptr_ = optr;
    }
    return *this;
  }

  sref& operator=(sref&& o) {
    if (ptr_)
      ptr_->dec();
    ptr_ = o.ptr_;
    o.ptr_ = nullptr;
    return *this;
  }

  template<typename U, typename = typename
           std::enable_if<std::is_convertible<U*, T*>::value>::type>
  sref& operator=(sref<U>&& o) {
    if (ptr_)
      ptr_->dec();
    ptr_ = o.ptr_;
    o.ptr_ = nullptr;
    return *this;
  }

  // Transfer ownership of a pointer to this sref.  This *does not*
  // increment the reference count and is primarily meant for getting
  // srefs for freshly allocated objects that have come with an
  // implicit reference count of 1.
  static constexpr sref transfer(T* p) {
    return sref(p);
  }

  // Transfer ownership of this sref to a pointer, clearing this sref.
  // This *does not* decrement the reference count and is meant for
  // transitioning an automatic reference to a manual one.
  T* transfer_to_ptr() {
    T* res = ptr_;
    ptr_ = nullptr;
    return res;
  }

  // Create a new reference to a pointer.  This *does* increment the
  // reference count and is primarily meant for transferring between
  // manual pointer-based reference counting and automatic counting.
  // The caller must ensure that it is valid to increment the
  // reference count (for example, the count is not currently zero).
  static sref newref(T* p) {
    if (p)
      p->inc();
    return sref(p);
  }

  void reset()
  {
    if (ptr_) {
      ptr_->dec();
      ptr_ = nullptr;
    }
  }

  bool init(T* p) {
    if (ptr_ || !p->tryinc())
      return false;
    ptr_ = p;
    return true;
  }

  bool operator==(const sref<T>& pr) const { return ptr_ == pr.ptr_; }
  bool operator!=(const sref<T>& pr) const { return ptr_ != pr.ptr_; }
  bool operator==(T* p) const { return ptr_ == p; }
  bool operator!=(T* p) const { return ptr_ != p; }

  explicit operator bool() const noexcept { return !!ptr_; }

  T * operator->() const noexcept { return ptr_; }
  T & operator*() const noexcept { return *ptr_; }
  T * get() const noexcept { return ptr_; }

private:
  T *ptr_;
};

template<typename T, typename... Args>
sref<T> make_sref(Args&&... args)
{
  return sref<T>::transfer(new T(std::forward<Args>(args)...));
}

class referenced {
public:
  // Start with 1 reference by default
  referenced(uint64_t refcount = 1) { ref_.v = refcount - 1; }

  // The number of valid references is:
  //   ref_.invalid ? 0 : ref_.count+1;

  inline void inc() {
    asm volatile("lock; incq %0" : "+m" (ref_.v) :: "memory", "cc");
  }

  // Attempt to increment the reference count, failing if the count is
  // currently zero.  If there is ever a tryinc from zero, the count
  // will remain zero forever after.
  inline bool tryinc() {
    // If references is 0 (i.e. ref_.count is 0xffffffff) a 32-bit 
    // increment will increases ref_.count to 0, but ref_.invalid
    // will remain unchanged.
    asm volatile("lock; incl %0" : "+m" (ref_.count) :: "memory", "cc");
    return ref_.invalid == 0;
  }

  inline void dec() {
    unsigned char c;
    // If references is 1 (i.e. ref_.v is 0), a 64-bit decrement will
    // underflow ref_.invalid to 0xffffffff (and ref_.count to 0xffffffff).
    asm volatile("lock; decq %0; sets %1" : "+m" (ref_.v), "=qm" (c) 
                 :: "memory", "cc");
    if (c)
      onzero();
  }

  uint64_t get_consistent() const {
    return ref_.invalid ? 0 : (ref_.count + 1);
  }

protected:
  virtual ~referenced() { }
  virtual void onzero() { delete this; }

private:
  mutable union {
    volatile uint64_t v;
    struct {
      volatile uint32_t count;
      volatile uint32_t invalid;
    };
  } ref_;
};
