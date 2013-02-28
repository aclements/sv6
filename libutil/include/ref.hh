#pragma once

#include <cstdint>

template <class T, typename = void>
class sref {
private:
  constexpr explicit sref(T* p) noexcept : ptr_(p) { }

public:
  constexpr sref() noexcept : ptr_(nullptr) { }

  sref(const sref &o) : ptr_(o.ptr_) {
    if (ptr_)
      ptr_->inc();
  }

  sref(sref &&o) noexcept : ptr_(o.ptr_)
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

  sref& operator=(sref&& o) {
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

  // Create a new reference to a pointer.  This *does* increment the
  // reference count and is primarily meant for transferring between
  // manual pointer-based reference counting and automatic counting.
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

  // init_nonzero(p) can be called only if it is guaranteed that
  // there is an existing reference to p.
  bool init_nonzero(T* p) {
    if (ptr_)
      return false;
    p->inc();
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

class referenced {
public:
  // Start with 1 reference
  referenced() { ref_.v = 0; }

  // The number of valid references is:
  //   ref_.invalid ? 0 : ref_.count+1;

  inline bool valid() const {
    return ref_.invalid == 0;
  }

  inline void inc() {
    // If references is 0 (i.e. ref_.count is 0xffffffff) a 32-bit 
    // increment will increases ref_.count to 0, but ref_.invalid
    // will remain unchanged.
    asm volatile("lock; incl %0" : "+m" (ref_.count) :: "memory", "cc");
  }

  inline bool tryinc() {
    inc();
    return valid();
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
