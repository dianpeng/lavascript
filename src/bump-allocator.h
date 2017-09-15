#ifndef BUMP_ALLOCATOR_H_
#define BUMP_ALLOCATOR_H_

#include <cstddef>
#include <cstdint>

#include "common.h"
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

  ~BumpAllocator();

  // Grab memory from BumpAllocator
  void* Grab( std::size_t );
  template< typename T > T* Grab() { return Grab(sizeof(T)); }

 public:
  std::size_t maximum_size() const { return maximum_size_; }
  void set_maximum_size( std::size_t sz ) { maximum_size_ = sz; }
  std::size_t current_capacity() const { return current_capacity_; }
  std::size_t total_bytes() const { return total_bytes_; }
  HeapAllocator* allocator() const { return allocator_; }

 private:
  void RefillPool( std::size_t );

  struct Segment {
    Segment* next;
  };

  Segment* segment_;                       // First segment list
  void* pool_;                             // Starting position of the current pool
  std::size_t current_capacity_;           // Current capacity
  std::size_t used_;                       // Used size for the current pool
  std::size_t maximum_size_;               // Maximum size of BumpAllocator
  std::size_t total_bytes_ ;               // How many bytes has been allocated , include the Segment header
  HeapAllocator* allocator_;               // Allocator for allocating the underlying memory

  LAVA_DISALLOW_COPY_AND_ASSIGN(BumpAllocator);
};

inline BumpAllocator::BumpAllocator( std::size_t init_capacity ,
                                     std::size_t maximum_size  ,
                                     HeapAllocator* allocator ):
  segment_(NULL),
  pool_   (NULL),
  current_capacity_( init_capacity ),
  used_   (0),
  maximum_size_    ( maximum_size  ),
  allocator_       ( allocator )
{
  RefillPool(init_capacity);
}

} // namespace lavascript

#endif // BUMP_ALLOCATOR_H_
