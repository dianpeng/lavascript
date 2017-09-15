#ifndef FREE_LIST_H_
#define FREE_LIST_H_
#include <cstdint>

#include "heap-allocator.h"
#include "common.h"
#include "trace.h"

namespace lavascript {

template< typename T > class FreeList {
 public:
  static_assert( sizeof(T) >= sizeof(std::uintptr_t) );

  FreeList( size_t current , size_t maximum , HeapAllocator* allocator );
  ~FreeList();

  // These 2 API will *NOT* construct the object or destruct the object
  // it is user's responsibility to call placement new to construct it
  // and call destructor manually to dispose them if the type is not a POD
  T* Grab();
  void Drop( T* );

 public:
  size_t size() const { return size_; }
  size_t capacity() const { return capacity_; }

 private:
  void Reserve( size_t count );

  struct Segment {
    Segment* next;              // Next segment
  };

  struct FreeNode {
    FreeNode* next;             // Next pointer points to FreeNode
  };

  FreeNode* next_;              // Next available free node inside of FreeList
  Segment* chunk_;              // Head of the chunk pool
  size_t size_;                 // Size of *allocated* FreeNode in FreeList
  size_t capacity_;             // All the *existed* FreeNode in FreeList
  size_t maximum_ ;             // Maximum upper bound for FreeList growing
  HeapAllocator* allocator_;    // HeapAllocator

  LAVA_DISALLOW_COPY_AND_ASSIGN(FreeList);
};

template< typename T > void FreeList<T>::Reserve( size_t count ) {
  void* ptr = Malloc( allocator_ , sizeof(T) * count + sizeof(Segment) );

  Segment* f= reinterpret_cast<Segment*>(ptr);
  f->next = chunk_;
  chunk_ = f;

  char* cur = static_cast<char*>(ptr) + sizeof(Segment);
  char* start = cur;

  // Linked the free-node into a free-list
  for( size_t i = 0 ; i < count - 1 ; ++i ) {
    FreeNode* node = reinterpret_cast<FreeNode*>(cur);
    node->next     = reinterpret_cast<FreeNode*>(cur + sizeof(T));
    cur = reinterpret_cast<char*>(node->next);
  }
  (reinterpret_cast<FreeNode*>(cur))->next = next_;
  next_ = reinterpret_cast<FreeNode*>(start);
  capacity_ = count;
}

template< typename T > FreeList<T>::FreeList( size_t current , size_t maximum ,
    HeapAllocator* allocator ):
  next_(NULL),
  chunk_(NULL),
  size_(0),
  capacity_(0),
  maximum_ (maximum),
  allocator_ (allocator)
{
  lava_verify( current != 0 && current <= maximum );
  Reserve(current);
}

template< typename T > FreeList<T>::~FreeList() {
  while(chunk_) {
    Segment* n = chunk_->next;
    Free(allocator_,chunk_);
    chunk_ = n;
  }
}

template< typename T > T* FreeList<T>::Grab() {
  if(!next_) {
    size_t cap = capacity_ * 2;
    cap = cap > maximum_ ? maximum_ : cap;
    Reserve(cap);
  }
  FreeNode* ret = next_;

#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(ret);
#endif // LAVASCRIPT_CHECK_OBJECTS

  next_ = next_->next;
  ++size_;
  return reinterpret_cast<T*>(ret);
}

template< typename T > void FreeList<T>::Drop( T* ptr ) {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(size_ >0);
#endif // LAVASCRIPT_CHECK_OBJECTS

  FreeNode* n = reinterpret_cast<FreeNode*>(ptr);
  n->next = next_;
  next_ = n;
  --size_;
}


} // namespace lavascript

#endif // FREE_LIST_H_
