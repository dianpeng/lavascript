#ifndef CBASE_ZONE_STL_H_
#define CBASE_ZONE_STL_H_

#include <limits>
#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "zone.h"

/**
 * This file contains a STL dapater of the zone allocator and also the corresponding STL
 * container. User can use the stl conatiner with zone allocator as well. The main use
 * case is when user want destruction or STL functionality. We should gradually migrate
 * to use STL but this is a really large change :(
 */

namespace lavascript {
namespace zone       {
namespace stl        {

template <typename T>
class ZoneAllocator {
 public:
  typedef T* pointer;
  typedef const T* const_pointer;
  typedef T& reference;
  typedef const T& const_reference;
  typedef T value_type;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;
  template <class O>
  struct rebind {
    typedef ZoneAllocator<O> other;
  };

  explicit ZoneAllocator(Zone* zone) throw() : zone_(zone) {}
  explicit ZoneAllocator(const ZoneAllocator& other) throw() : ZoneAllocator<T>(other.zone_) {}
  template <typename U>
  ZoneAllocator(const ZoneAllocator<U>& other) throw() : ZoneAllocator<T>(other.zone_) {}

  template <typename U> friend class ZoneAllocator;

  T*             address(T& x) const { return &x; }
  const T* address(const T& x) const { return &x; }
  T* allocate(size_t n, const void* hint = 0) {
    (void)hint;
    return static_cast<T*>(zone_->Malloc(sizeof(T)*n));
  }
  void deallocate(T* p, size_t sz) { /* noop for Zones */
    (void)p;(void)sz;
  }

  size_t max_size() const throw() { return std::numeric_limits<int>::max() / sizeof(T); }

  template <typename U, typename... Args>
  void construct(U* p, Args&&... args) {
    void* v_p = const_cast<void*>(static_cast<const void*>(p));
    ::new (v_p) U(std::forward<Args>(args)...);
  }

  template <typename U>
  void destroy(U* p) { ::lavascript::Destruct(p); }

  bool operator==(ZoneAllocator const& other) const {
    return zone_ == other.zone_;
  }
  bool operator!=(ZoneAllocator const& other) const {
    return zone_ != other.zone_;
  }

  Zone* zone() { return zone_; }
 private:
  Zone* zone_;
};

template <typename T>
class ZoneVector : public std::vector<T, ZoneAllocator<T>> {
 public:
  explicit ZoneVector(Zone* zone)
      : std::vector<T, ZoneAllocator<T>>(ZoneAllocator<T>(zone)) {}

  ZoneVector(Zone* zone , T def, std::size_t size)
      : std::vector<T, ZoneAllocator<T>>(size, def, ZoneAllocator<T>(zone)) {}

  ZoneVector(Zone* zone , std::initializer_list<T> list)
      : std::vector<T, ZoneAllocator<T>>(list, ZoneAllocator<T>(zone)) {}

  template <class InputIt>
  ZoneVector(Zone* zone,InputIt first, InputIt last)
      : std::vector<T, ZoneAllocator<T>>(first, last, ZoneAllocator<T>(zone)) {}
};

template <typename T>
class ZoneDeque : public std::deque<T, ZoneAllocator<T>> {
 public:
  explicit ZoneDeque(Zone* zone)
      : std::deque<T, ZoneAllocator<T>>(
            ZoneAllocator<T>(zone)) {}
};

template <typename T>
class ZoneLinkedList : public std::list<T, ZoneAllocator<T>> {
 public:
  explicit ZoneLinkedList(Zone* zone)
      : std::list<T, ZoneAllocator<T>>(ZoneAllocator<T>(zone)) {}
};

template <typename T>
class ZoneForwardList : public std::forward_list<T, ZoneAllocator<T>> {
 public:
  explicit ZoneForwardList(Zone* zone)
      : std::forward_list<T, ZoneAllocator<T>>(ZoneAllocator<T>(zone)) {}
};

template <typename T, typename Compare = std::less<T>>
class ZonePriorityQueue
    : public std::priority_queue<T, ZoneVector<T>, Compare> {
 public:
  explicit ZonePriorityQueue(Zone* zone)
      : std::priority_queue<T, ZoneVector<T>, Compare>(Compare(),
                                                       ZoneVector<T>(zone)) {}
};

template <typename T>
class ZoneQueue : public std::queue<T, ZoneDeque<T>> {
 public:
  explicit ZoneQueue(Zone* zone)
      : std::queue<T, ZoneDeque<T>>(ZoneDeque<T>(zone)) {}
};

template <typename T>
class ZoneStack : public std::stack<T, ZoneDeque<T>> {
 public:
  explicit ZoneStack(Zone* zone)
      : std::stack<T, ZoneDeque<T>>(ZoneDeque<T>(zone)) {}
};

template <typename K, typename Compare = std::less<K>>
class ZoneSet : public std::set<K, Compare, ZoneAllocator<K>> {
 public:
  explicit ZoneSet(Zone* zone)
      : std::set<K, Compare, ZoneAllocator<K>>(Compare(),
                                               ZoneAllocator<K>(zone)) {}
};

template <typename K, typename V, typename Compare = std::less<K>>
class ZoneMap
    : public std::map<K, V, Compare, ZoneAllocator<std::pair<const K, V>>> {
 public:
  explicit ZoneMap(Zone* zone)
      : std::map<K, V, Compare, ZoneAllocator<std::pair<const K, V>>>(
            Compare(), ZoneAllocator<std::pair<const K, V>>(zone)) {}
};

static const std::size_t kSTLDefaultBucketCount = 64;

template <typename K, typename V, typename Hash = std::hash<K>,
          typename KeyEqual = std::equal_to<K>>
class ZoneUnorderedMap
    : public std::unordered_map<K, V, Hash, KeyEqual,
                                ZoneAllocator<std::pair<const K, V>>> {
 public:
  explicit ZoneUnorderedMap(Zone* zone)
      : std::unordered_map<K, V, Hash, KeyEqual,
                           ZoneAllocator<std::pair<const K, V>>>(
            kSTLDefaultBucketCount , Hash(), KeyEqual(),
            ZoneAllocator<std::pair<const K, V>>(zone)) {}
};

template <typename K, typename Hash = std::hash<K>,
          typename KeyEqual = std::equal_to<K>>
class ZoneUnorderedSet
    : public std::unordered_set<K, Hash, KeyEqual, ZoneAllocator<K>> {
 public:
  explicit ZoneUnorderedSet(Zone* zone)
      : std::unordered_set<K, Hash, KeyEqual, ZoneAllocator<K>>(
            kSTLDefaultBucketCount , Hash(), KeyEqual(), ZoneAllocator<K>(zone)) {}
};

template <typename K, typename V, typename Compare = std::less<K>>
class ZoneMultimap
    : public std::multimap<K, V, Compare,
                           ZoneAllocator<std::pair<const K, V>>> {
 public:
  explicit ZoneMultimap(Zone* zone)
      : std::multimap<K, V, Compare, ZoneAllocator<std::pair<const K, V>>>(
            Compare(), ZoneAllocator<std::pair<const K, V>>(zone)) {}
};

} // namespace stl
} // namespace zone
} // namespace lavascript

#endif // CBASE_ZONE_STL_H_
