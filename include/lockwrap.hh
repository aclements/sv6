#pragma once

#include "seqlock.hh"

template<typename T>
class ptr_wrap
{
public:
  ptr_wrap(T* ptr) : ptr_(ptr) {}

  T* operator->() const {
    return ptr_;
  }

  T& operator*() const {
    return *ptr_;
  }

private:
  T* ptr_;
};

template<typename T>
class seq_reader
{
public:
  seq_reader(const T* v, const seqcount<u32>* seq) {
    for (;;) {
      auto r = seq->read_begin();
      state_ = *v;
      if (!r.need_retry())
        return;
    }
  }

  const T* operator->() const {
    return &state_;
  }

  const T& operator*() const {
    return state_;
  }

private:
  T state_;
};

class seq_writer
{
public:
  seq_writer(seqcount<u32>* seq) : w_(seq->write_begin()) {}
  seq_writer() {}

private:
  seqcount<u32>::writer w_;
};
