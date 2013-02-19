#pragma once

#include "kernel.hh"

#include <string.h>
#include <type_traits>
#include <utility>
#include <new>

using std::pair;
using std::make_pair;

template<int N>
class strbuf {
 public:
  char buf_[N];

  strbuf() {
    if (N > 0)
      buf_[0] = '\0';
  }

  strbuf(const char *s) {
    strncpy(buf_, s, N);
  }

  bool operator==(const strbuf<N> &other) const {
    return !strncmp(buf_, other.buf_, N);
  }

  bool operator!=(const strbuf<N> &other) const {
    return !operator==(other);
  }

  bool operator<(const strbuf<N> &other) const {
    return strncmp(buf_, other.buf_, N) < 0;
  }
};

namespace std {
  struct ostream { int next_width; };
  extern ostream cout;

  static inline
  ostream& operator<<(ostream &s, const char *str) {
    if (!str)
      str = "(null)";

    int len = strlen(str);
    cprintf("%s", str);
    while (len < s.next_width) {
      cprintf(" ");
      len++;
    }
    s.next_width = 0;
    return s;
  }

  static inline
  ostream& operator<<(ostream &s, u32 v) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", v);
    return s << buf;
  }

  static inline
  ostream& operator<<(ostream &s, u64 v) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%ld", v);
    return s << buf;
  }

  static inline
  ostream& operator<<(ostream &s, ostream& (*xform)(ostream&)) {
    return xform(s);
  }

  static inline ostream& endl(ostream &s) { s << "\n"; return s; }
  static inline ostream& left(ostream &s) { return s; }

  struct ssetw { int _n; };
  static inline ssetw setw(int n) { return { n }; }

  static inline
  ostream& operator<<(ostream &s, const ssetw &sw) {
    s.next_width = sw._n;
    return s;
  }
}

/* C++ runtime */
/* Ref: http://sourcery.mentor.com/public/cxx-abi/abi.html */
extern "C" void __cxa_pure_virtual(void);
extern "C" int  __cxa_guard_acquire(s64 *guard_object);
extern "C" void __cxa_guard_release(s64 *guard_object);
extern "C" void __cxa_guard_abort(s64 *guard_object);
extern "C" int  __cxa_atexit(void (*f)(void *), void *p, void *d);
extern void *__dso_handle;

#define NEW_DELETE_OPS(classname)                                   \
  static void* operator new(unsigned long nbytes,                   \
                            const std::nothrow_t&) noexcept {       \
    assert(nbytes == sizeof(classname));                            \
    return kmalloc(sizeof(classname), #classname);                  \
  }                                                                 \
                                                                    \
  static void* operator new(unsigned long nbytes) {                 \
    void *p = classname::operator new(nbytes, std::nothrow);        \
    if (p == nullptr)                                               \
      throw_bad_alloc();                                            \
    return p;                                                       \
  }                                                                 \
                                                                    \
  static void* operator new(unsigned long nbytes, classname *buf) { \
    assert(nbytes == sizeof(classname));                            \
    return buf;                                                     \
  }                                                                 \
                                                                    \
  static void operator delete(void *p,                              \
                              const std::nothrow_t&) noexcept {     \
    kmfree(p, sizeof(classname));                                   \
  }                                                                 \
                                                                    \
  static void operator delete(void *p) {                            \
    classname::operator delete(p, std::nothrow);                    \
  }

template<class T>
class scoped_cleanup_obj {
 private:
  T handler_;
  bool active_;

 public:
  scoped_cleanup_obj(const T& h) : handler_(h), active_(true) {};
  ~scoped_cleanup_obj() { if (active_) handler_(); }
  void dismiss() { active_ = false; }

  void operator=(const scoped_cleanup_obj&) = delete;
  scoped_cleanup_obj(const scoped_cleanup_obj&) = delete;
  scoped_cleanup_obj(scoped_cleanup_obj&& other) :
    handler_(other.handler_), active_(other.active_) { other.dismiss(); }
};

template<class T>
scoped_cleanup_obj<T>
scoped_cleanup(const T& h)
{
  return scoped_cleanup_obj<T>(h);
}

static void inline
throw_bad_alloc()
{
#if EXCEPTIONS
  throw std::bad_alloc();
#else
  panic("bad alloc");
#endif
}
