#ifndef FREE_LIST_H_
#define FREE_LIST_H_
#include <cstdint>

#include "heap-allocator.h"
#include "common.h"
#include "trace.h"

namespace lavascript {

// A normal free list and it won't reclaim any memory during its lifetime.
// This is crucial for us since we need to mark the memory in certain states
// when we return the memory back. Since any memory used by freelist when
// it's free will only use first 8 bytes (a pointer), we can safely mark any
// states even if the memory is in free list.
// This is helpful for GC to avoid false positive , otherwise we need to reset
// certain field in stack whenever a function is called in interpreter.
template< typename T > class FreeList {
 public:
  static_assert( sizeof(T) >= sizeof(std::uintptr_t) );

  FreeList( std::size_t current , std::size_t maximum , HeapAllocator* allocator );
  ~FreeList();

  // These 2 API will *NOT* construct the object or destruct the object
  // it is user's responsibility to call placement new to construct it
  // and call destructor manually to dispose them if the type is not a POD
  T* Grab();
  void Drop( T* );

 public:
  std::size_t size() const { return size_; }
  std::size_t chunk_size() const { return chunk_size_; }
  std::size_t capacity() const { return capacity_; }

 private:
  void Reserve( std::size_t count );

  struct Segment {
    Segment* next;              // Next segment
  };

  struct FreeNode {
    FreeNode* next;             // Next pointer points to FreeNode
  };

  FreeNode* next_;              // Next available free node inside of FreeList
  Segment* chunk_;              // Head of the chunk pool
  std::size_t size_;            // Size of *allocated* FreeNode in FreeList
  std::size_t chunk_size_;      // Chunk number
  std::size_t capacity_;        // All the *existed* FreeNode in FreeList
  std::size_t maximum_ ;        // Maximum upper bound for FreeList growing
  HeapAllocator* allocator_;    // HeapAllocator

  LAVA_DISALLOW_COPY_AND_ASSIGN(FreeList);
};

template< typename T > void FreeList<T>::Reserve( std::size_t count ) {
  void* ptr = Malloc( allocator_ , sizeof(T) * count + sizeof(Segment) );

  Segment* f= reinterpret_cast<Segment*>(ptr);
  f->next = chunk_;
  chunk_ = f;

  char* cur = static_cast<char*>(ptr) + sizeof(Segment);
  char* start = cur;

  // Linked the free-node into a free-list
  for( std::size_t i = 0 ; i < count - 1 ; ++i ) {
    FreeNode* node = reinterpret_cast<FreeNode*>(cur);
    node->next     = reinterpret_cast<FreeNode*>(cur + sizeof(T));
    cur = reinterpret_cast<char*>(node->next);
  }
  (reinterpret_cast<FreeNode*>(cur))->next = next_;
  next_ = reinterpret_cast<FreeNode*>(start);
  capacity_ = count;
  ++chunk_size_;
}

template< typename T > FreeList<T>::FreeList( std::size_t current , std::size_t maximum ,
    HeapAllocator* allocator ):
  next_(NULL),
  chunk_(NULL),
  size_(0),
  chunk_size_(0),
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
    std::size_t cap = capacity_ * 2;
    cap = cap > maximum_ ? maximum_ : cap;
    Reserve(cap);
  }
  FreeNode* ret = next_;

  lava_debug(NORMAL,lava_verify(ret););

  next_ = next_->next;
  ++size_;
  return reinterpret_cast<T*>(ret);
}

template< typename T > void FreeList<T>::Drop( T* ptr ) {
  lava_debug(NORMAL,lava_verify(size_>0););

  FreeNode* n = reinterpret_cast<FreeNode*>(ptr);
  n->next = next_;
  next_ = n;
  --size_;
}


} // namespace lavascript

#endif // FREE_LIST_H_
