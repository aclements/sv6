/**
 * Intrusive linked lists.
 *
 * An intrusive linked list uses link fields embedded directly in
 * objects as fields, much like how linked lists are often implemented
 * in C.  As a result, adding an object to an intrusive linked list
 * requires no copying or allocation and is exception-safe.  The
 * downside is that a given object can only be on one list per link
 * field in that object; however, this property is often true in
 * practice.
 *
 * Example use:
 *     struct thing {
 *       ilink<thing> link;
 *     };
 *     ilist<thing, &thing::link> list;
 *
 * Because the usual access checks apply to the member data pointer
 * template argument, if the list is declared outside of the class,
 * then either the link field must be public (as above), or the list's
 * type must be declared inside the class (e.g., as a typedef):
 *     class thing {
 *       ilink<thing> link;
 *     public:
 *       typedef ilist<thing, &thing::link> list_t;
 *     };
 *     thing::list_t list;
 * This is not necessary if the list is declared inside the class
 * (e.g., as a static field), though if this list is publicly
 * accessible, outside users will not be able to refer to its type
 * except through an in-class typedef or auto.
 */

#pragma once

#include <utility>

/**
 * @internal Given a pointer to a member and a member data pointer,
 * return a pointer to the object containing that member.
 */
template<class Container, class Member>
Container *
container_from_member(const Member *member, const Member Container::* mem_ptr)
{
  // List of compilers where this trick works from Boost.Intrusive.
#if defined(__GNUC__) || defined(__HP_aCC) || defined(__IBMCPP__) || \
  defined(__DECCXX)
  Container *c = nullptr;
  const char *c_mem = reinterpret_cast<const char*>(&(c->*mem_ptr));
  auto offset = c_mem - reinterpret_cast<const char*>(c);
#else
#error Unsupported compiler
#endif
  return (Container*)((char*)member - offset);
}

/**
 * An embedded link used for singly-linked lists.
 */
template<typename T>
struct islink
{
  T* next;
};

/**
 * Forward-iterator over singly linked lists.
 */
template<typename T, islink<T> T::* L>
struct isiterator
{
  T* elem;

  constexpr isiterator() : elem(nullptr) { }
  constexpr isiterator(T* e) : elem(e) { }

  T& operator*() const noexcept
  {
    return *elem;
  }

  T* operator->() const noexcept
  {
    return elem;
  }

  bool operator==(const isiterator &o) const noexcept
  {
    return o.elem == elem;
  }

  bool operator!=(const isiterator &o) const noexcept
  {
    return o.elem != elem;
  }

  isiterator &operator++() noexcept
  {
    elem = (elem->*L).next;
    return *this;
  }

  isiterator operator++(int) noexcept
  {
    isiterator cur = *this;
    ++(*this);
    return cur;
  }
};

/**
 * An intrusive singly-linked list.  This provides an API similar to
 * C++'s std::forward_list, but it is pointer-based instead of
 * value-based: adding an object to the list shares ownership of that
 * instance with the list, rather than copying it.  As a result, this
 * class never performs allocation and never throws exceptions.
 *
 * Being a singly-linked list, this supports only forward iteration
 * starting at the first element of the list.  Furthermore, having an
 * iterator to an element generally isn't enough to modify the list
 * around that element; most methods require an iterator to the
 * element *before* the position being modified (in contrast with the
 * standard requirement of an iterator to after the position being
 * modified).
 */
template<typename T, islink<T> T::* L>
struct islist
{
  typedef isiterator<T, L> iterator;

  islink<T> head;

  constexpr islist() : head{nullptr} { }

  islist(const islist &o) = delete;
  islist &operator=(const islist &o) = delete;

  islist(islist &&o)
  {
    *this = std::move(o);
  }

  islist &operator=(islist &&o) noexcept
  {
    head = o.head;
    o.head.next = nullptr;
    return *this;
  }

  iterator
  before_begin() noexcept
  {
    return iterator(container_from_member(&head, L));
  }

  iterator
  begin() noexcept
  {
    return iterator(head.next);
  }

  const iterator
  begin() const noexcept
  {
    return iterator(head.next);
  }

  iterator
  end() noexcept
  {
    return iterator(nullptr);
  }

  const iterator
  end() const noexcept
  {
    return iterator(nullptr);
  }

  bool
  empty() const noexcept
  {
    return head.next == nullptr;
  }

  T&
  front() noexcept
  {
    return *head.next;
  }

  const T&
  front() const noexcept
  {
    return *head.next;
  }

  void
  push_front(T *x) noexcept
  {
    (x->*L).next = head.next;
    head.next = x;
  }

  void
  pop_front() noexcept
  {
    head.next = (head.next->*L).next;
  }

  iterator
  insert_after(iterator pos, T* x) noexcept
  {
    (x->*L).next = (pos.elem->*L).next;
    (pos.elem->*L).next = x;
    return ++pos;
  }

  iterator
  erase_after(iterator pos) noexcept
  {
    (pos.elem->*L).next = ((pos.elem->*L).next->*L).next;
    return ++pos;
  }

  iterator
  erase_after(iterator pos, iterator last) noexcept
  {
    (pos.elem->*L).next = last.elem;
    return last;
  }

  void
  clear() noexcept
  {
    head.next = nullptr;
  }

  /**
   * Insert the contents of x after pos and clear x.  In general, this
   * is linear in the length of x, but if pos points to the last
   * element of this list, this is constant-time.
   */
  void
  splice_after(iterator pos, islist &&x)
  {
    if ((pos.elem->*L).next) {
      // Find the last link in x and patch it.  As a special case, if
      // pos's next is null, we can skip this entirely, making
      // splice_after O(1) when splicing at the end of the list.
      for (auto xit = x.begin(), end = x.end(); xit != end; ++xit) {
        if ((*xit.*L).next == nullptr) {
          (*xit.*L).next = (pos.elem->*L).next;
          break;
        }
      }
    }
    (pos.elem->*L).next = x.head.next;
    x.head.next = nullptr;
  }

  /**
   * Cut this list after the element pointed to by @c pos and return a
   * new list consisting of all elements starting at <code>pos +
   * 1</code>.
   */
  islist
  cut_after(iterator pos) noexcept
  {
    auto nfirst = (pos.elem->*L).next;
    (pos.elem->*L).next = nullptr;
    islist nl;
    nl.head.next = nfirst;
    return nl;
  }

  /**
   * Return an iterator pointing to elem, which must be in this list.
   */
  iterator
  iterator_to(T *elem)
  {
    return iterator(elem);
  }
};

/**
 * An intrusive single-linked list with both head and tail pointers.
 * This is similar to islist (and uses the same link and iterator
 * type), but can provides O(1) access to the last element in the
 * list.
 */
template<typename T, islink<T> T::* L>
struct isqueue : private islist<T, L>
{
  typedef typename islist<T,L>::iterator iterator;
  using islist<T,L>::head;

private:
  T* last;

public:
  constexpr isqueue() : islist<T,L>(), last(nullptr) { }

  isqueue(const isqueue &o) = delete;
  isqueue &operator=(const isqueue &o) = delete;

  isqueue(isqueue &&o)
  {
    *this = std::move(o);
  }

  isqueue &operator=(isqueue &&o) noexcept
  {
    head = o.head;
    last = o.last;
    o.head.next = nullptr;
    o.last = nullptr;
    return *this;
  }

  using islist<T,L>::before_begin;

  iterator
  before_end() noexcept
  {
    if (empty())
      return before_begin();
    return iterator_to(last);
  }

  using islist<T,L>::begin;
  using islist<T,L>::end;
  using islist<T,L>::empty;
  using islist<T,L>::front;

  T&
  back() noexcept
  {
    return *last;
  }

  void
  push_front(T *x) noexcept
  {
    if (empty())
      last = x;
    islist<T, L>::push_front(x);
  }

  void
  push_back(T *x) noexcept
  {
    insert_after(before_end(), x);
  }

  void
  pop_front() noexcept
  {
    islist<T, L>::pop_front();
    if (empty())
      last = nullptr;
  }

  iterator
  insert_after(iterator pos, T* x) noexcept
  {
    if (!(pos.elem->*L).next)
      last = x;
    return islist<T, L>::insert_after(pos, x);
  }

  iterator
  erase_after(iterator pos) noexcept
  {
    auto it = islist<T, L>::erase_after(pos);
    if (empty())
      last = nullptr;
    else if (!(pos.elem->*L).next)
      last = pos.elem;
    return it;
  }

  iterator
  erase_after(iterator pos, iterator last) noexcept
  {
    auto it = islist<T, L>::erase_after(pos, last);
    if (empty())
      last = nullptr;
    else if (!(pos.elem->*L).next)
      last = pos.elem;
    return it;
  }

  void
  clear() noexcept
  {
    islist<T, L>::clear();
    last = nullptr;
  }

  isqueue
  cut_after(iterator pos) noexcept
  {
    isqueue nq;
    nq.last = last;
    auto nl = islist<T, L>::cut_after(pos);
    nq.head = nl.head;
    if (empty())
      last = nullptr;
    else
      last = pos.elem;
    return nq;
  }

  using islist<T, L>::iterator_to;
};

/**
 * An embedded link used for doubly-linked lists.
 */
template<typename T>
struct ilink
{
  T *prev, *next;
};

/**
 * Iterator over double-linked lists.
 */
template<typename T, ilink<T> T::* L>
struct iiterator
{
  T* elem;

  constexpr iiterator(T* e) : elem(e) { }
  constexpr iiterator() : elem(nullptr) { }

  T& operator*() const noexcept
  {
    return *elem;
  }

  T* operator->() const noexcept
  {
    return elem;
  }

  bool operator==(const iiterator &o) const noexcept
  {
    return o.elem == elem;
  }

  bool operator!=(const iiterator &o) const noexcept
  {
    return o.elem != elem;
  }

  iiterator &operator++() noexcept
  {
    elem = (elem->*L).next;
    return *this;
  }

  iiterator operator++(int) noexcept
  {
    iiterator cur = *this;
    ++(*this);
    return cur;
  }

  iiterator &operator--() noexcept
  {
    elem = (elem->*L).prev;
    return *this;
  }

  iiterator operator--(int) noexcept
  {
    iiterator cur = *this;
    --(*this);
    return cur;
  }
};

/**
 * An intrusive doubly-linked list.  This provides an API similar to
 * C++'s std::list, but it is pointer-based instead of value-based:
 * adding an object to the list shares ownership of that instance with
 * the list, rather than copying it.  As a result, this class never
 * performs allocation and never throws exceptions.
 *
 * Being a doubly-linked list, this supports bi-directional
 * iteration.  This list tracks both the head and the tail of the
 * list, so it's possible to start from either end.
 */
template<typename T, ilink<T> T::* L>
struct ilist
{
  typedef iiterator<T, L> iterator;

  ilink<T> head;

  ilist()
  {
    clear();
  }

  ilist(const ilist &o) = delete;
  ilist &operator=(const ilist &o) = delete;

  ilist(ilist &&o)
  {
    *this = std::move(o);
  }

  ilist &operator=(ilist &&o) noexcept
  {
    if(o.empty()) {
      clear();
      return *this;
    }

    // Fix up first and last elements to point to this list.  It's
    // important to do this on o.head *before* we copy it to head: if
    // the other list is empty, o.head will point to itself so our
    // update will update o.head.
    (o.head.next->*L).prev = (o.head.prev->*L).next =
      container_from_member(&head, L);
    head = o.head;
    // Reset other list
    o.clear();
    return *this;
  }

  iterator
  begin() const noexcept
  {
    return iterator(head.next);
  }

  iterator
  end() const noexcept
  {
    return iterator(container_from_member(&head, L));
  }

  bool
  empty() const noexcept
  {
    return head.next == container_from_member(&head, L);
  }

  T&
  front() noexcept
  {
    return *head.next;
  }

  const T&
  front() const noexcept
  {
    return *head.next;
  }

  T&
  back() noexcept
  {
    return *head.prev;
  }

  const T&
  back() const noexcept
  {
    return *head.prev;
  }

  void
  push_front(T *x) noexcept
  {
    insert(begin(), x);
  }

  void
  pop_front() noexcept
  {
    erase(begin());
  }

  void
  push_back(T *x) noexcept
  {
    insert(end(), x);
  }

  void
  pop_back() noexcept
  {
    erase(iterator(head.prev));
  }

  iterator
  insert(iterator pos, T* x) noexcept
  {
    (x->*L).next = pos.elem;
    (x->*L).prev = (pos.elem->*L).prev;
    ((pos.elem->*L).prev->*L).next = x;
    (pos.elem->*L).prev = x;
    return ++pos;
  }

  iterator
  erase(iterator pos) noexcept
  {
    ((pos.elem->*L).next->*L).prev = (pos.elem->*L).prev;
    ((pos.elem->*L).prev->*L).next = (pos.elem->*L).next;
    return ++pos;
  }

  iterator
  erase(iterator pos, iterator last) noexcept
  {
    ((pos.elem->*L).prev->*L).next = last.elem;
    (last.elem->*L).prev = (pos.elem->*L).prev;
    return last;
  }

  void
  clear() noexcept
  {
    head.prev = head.next = container_from_member(&head, L);
  }

  /**
   * Return an iterator pointing to elem, which must be in this list.
   */
  static iterator
  iterator_to(T *elem)
  {
    return iterator(elem);
  }
};
