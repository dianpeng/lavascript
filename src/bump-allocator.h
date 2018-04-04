#ifndef BUMP_ALLOCATOR_H_
#define BUMP_ALLOCATOR_H_

#include <cstddef>
#include <cstdint>

#include "common.h"
#include "util.h"
#include "heap-allocator.h"

namespace lavascript {

/**
 * Simple BumpAllocator , bumps the pointer when do allocation.
 * It doesn't support free operation and all memory will be freed
 * when BumpAllocator is destructed
 */

class BumpAllocator {
 public:
  inline BumpAllocator( std::size_t init_capacity ,
                        std::size_t maximum_size  ,
                        HeapAllocator* allocator = NULL );

  ~BumpAllocator() { Clear(); }

  // Grab memory from BumpAllocator
  void* Grab( std::size_t );
  void* Grab( std::size_t sz , std::size_t alignment ) {
    return Grab( Align(sz,alignment) );
  }
  template< typename T > T* Grab() { return static_cast<T*>(Grab(sizeof(T))); }

 public:
  std::size_t size() const { return size_; }
  std::size_t maximum_size() const { return maximum_size_; }
  void set_maximum_size( std::size_t sz ) { maximum_size_ = sz; }
  std::size_t segment_size() const { return segment_size_; }
  std::size_t current_capacity() const { return current_capacity_; }
  std::size_t total_bytes() const { return total_bytes_; }
  HeapAllocator* allocator() const { return allocator_; }

 public:
  void Reset();

 private:
  void Clear();
  void RefillPool( std::size_t );

  struct Segment {
    Segment* next;
  };

  Segment* segment_;                       // First segment list
  void* pool_;                             // Starting position of the current pool
  std::size_t init_capacity_;              // Initialized capacity
  std::size_t size_;                       // How many bytes has been allocated by Grab function call
  std::size_t current_capacity_;           // Current capacity
  std::size_t used_;                       // Used size for the current pool
  std::size_t segment_size_;               // Size of all the segment
  std::size_t maximum_size_;               // Maximum size of BumpAllocator
  std::size_t total_bytes_ ;               // How many bytes has been allocated , include the Segment header
  HeapAllocator* allocator_;               // Allocator for allocating the underlying memory

  LAVA_DISALLOW_COPY_AND_ASSIGN(BumpAllocator);
};

inline BumpAllocator::BumpAllocator( std::size_t init_capacity ,
                                     std::size_t maximum_size  ,
                                     HeapAllocator* allocator ):
  segment_         (NULL),
  pool_            (NULL),
  init_capacity_   (init_capacity),
  size_            (0),
  current_capacity_(init_capacity),
  used_            (0),
  segment_size_    (0),
  maximum_size_    (maximum_size),
  total_bytes_     (0),
  allocator_       (allocator)
{
  if(init_capacity) RefillPool(init_capacity);
}

} // namespace lavascript

#endif // BUMP_ALLOCATOR_H_
