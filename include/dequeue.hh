#pragma once

#include "ilist.hh"
#include "kalloc.hh"

#include <cstddef>
#include <initializer_list>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <new>

/// A statically allocated vector of at most N T's.
template<class T, typename Allocator = kalloc_allocator<T>>
class dequeue
{
public:
  class iterator;

private:
  static constexpr size_t NENTRIES = 508;

  struct page {
    ilink<page> link;
    size_t begin_index;
    size_t end_index;
    T* entries[NENTRIES];
  };
  static_assert(sizeof(page) == PGSIZE);

  typename Allocator::template rebind<page>::other alloc_;

  std::size_t size_;
  ilist<page, &page::link> pages_;

public:
  class iterator {
    typename ilist<page, &page::link>::iterator page;
    size_t entry;

    friend class dequeue;
    iterator(): entry(NENTRIES) {}

  public:
    bool operator==(const iterator& o) const noexcept {
      return o.page == page && o.entry == entry;
    }
    bool operator!=(const iterator& o) const noexcept {
      return o.page != page || o.entry != entry;
    }
    iterator &operator++() noexcept {
      entry++;
      if (entry == page->end_index) {
        entry = 0;
        page++;
      }
      return *this;
    }
    iterator operator++(int) noexcept {
      iterator cur = *this;
      ++(*this);
      return cur;
    }
    iterator &operator--() noexcept {
      if (entry == 0) {
        page--;
        entry = page->end_index;
      }
      entry--;
      return *this;
    }
    iterator operator--(int) noexcept {
      iterator cur = *this;
      --(*this);
      return cur;
    }
    T* operator*() const noexcept {
      return page->entries[entry];
    }
    T* operator->() const noexcept {
      return page->entries[entry];
    }
  };

  typedef T* value_type;
  typedef value_type& reference;
  typedef const value_type& const_reference;
  // typedef T* iterator;
  // typedef const T* const_iterator;
  typedef std::size_t size_type;
  // typedef std::size_t difference_type;

  constexpr dequeue() : size_(0), pages_() {}

  template<class InputIterator>
  dequeue(InputIterator first, InputIterator last) : size_(0) {
    for (; first != last; ++first)
      push_back(*first);
  }

  dequeue(std::initializer_list<T> elts) : size_(0) {
    for (auto it = elts.begin(), last = elts.end(); it != last; ++it)
      push_back(*it);
  }

  dequeue(dequeue&& other) : size_(other.size_), pages_(std::move(other.pages_)) {
    other.size = 0;
  }

  dequeue& operator=(dequeue&& other) {
    size_ = other.size_;
    pages_ = std::move(other.pages_);
    other.size_ = 0;
    return *this;
  }

  ~dequeue() {
    clear();
  }

  size_type size() const noexcept {
    return size_;
  }

  bool empty() const noexcept {
    return size_ == 0;
  }

  iterator begin() {
    if(empty())
      return end();

    iterator it;
    it.page = pages_.begin();
    it.entry = it.page->begin_index;
    return it;
  }

  iterator end() {
    iterator it;
    it.page = pages_.end();
    it.entry = 0;
    return it;
  }

  T* front()
  {
    return *begin();
  }

  T* back() const
  {
    return *(--end());
  }

  void push_front(T* x)
  {
    size_++;
    if (!pages_.empty()) {
      page& p = pages_.front();
      if (p.begin_index > 0) {
        p.entries[--p.begin_index] = x;
        return;
      }
    }

    auto p = alloc_.allocate(1);
    p->begin_index = NENTRIES - 1;
    p->end_index = p->begin_index + 1;
    p->entries[p->begin_index] = x;
    pages_.push_front(p);
  }

  void push_back(T* x)
  {
    size_++;
    if (!pages_.empty()) {
      page& p = pages_.back();
      if (p.end_index < NENTRIES) {
        p.entries[p.end_index++] = x;
        return;
      }
    }

    auto p = alloc_.allocate(1);
    p->begin_index = 0;
    p->end_index = p->begin_index + 1;
    p->entries[p->begin_index] = x;
    pages_.push_front(p);
  }

  T* pop_front()
  {
    if (empty())
      return nullptr;

    iterator it = begin();
    erase(it);
    return *it;
  }

  T* pop_back()
  {
    if (empty())
      return nullptr;

    iterator it = --end();
    erase(it);
    return *it;
  }

  void erase(iterator it) {
    assert(!empty());

    size_--;
    if (it.entry == it.page->begin_index) {
      it.page->begin_index++;
    } else if(it.entry == it.page->end_index) {
      it.page->end_index--;
    } else {
      memmove(&it.page->entries[it.entry],
              &it.page->entries[it.entry+1],
              (it.page->end_index - it.entry - 1) * sizeof(T*));
      it.page->end_index--;
    }

    if (it.page->begin_index == it.page->end_index) {
      page* ptr = &*it.page;
      pages_.erase(it.page);
      alloc_.deallocate(ptr, 1);
    }
  }

  void clear() noexcept {
    for (auto p : pages_)
      alloc_.deallocate(&p, 1);

    pages_.clear();
  }

  iterator iterator_to(T* t) {
    auto it = begin();
    while (*it != t && it != end())
      it++;
    return it;
  }
};
