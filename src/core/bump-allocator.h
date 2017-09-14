#ifndef BUMP_ALLOCATOR_H_
#define BUMP_ALLOCATOR_H_

#include <cstdint>

namespace lavascript {
namespace core {

/**
 * Simple BumpAllocator , bumps the pointer when do allocation.
 * It doesn't support free operation and all memory will be freed
 * when BumpAllocator is destructed
 */

class BumpAllocator {
 public:
  BumpAllocator( std::size_t init_capacity ,
                 std::size_t maximum_size  ):
    segment_(NULL),
    pool_   (NULL),
    current_capacity_( init_capacity ),
    used_   (0),
    maximum_size_    ( maximum_size  )
  {
    RefillPool(init_capacity);
  }

  // Grab memory from BumpAllocator
  void* Grab( std::size_t );

 private:
  void RefillPool( std::size_t );

 private:
  struct Segment {
    Segment* next;
  };

  Segment* segment_;                       // First segment list
  void* pool_;                             // Starting position of the current pool
  std::size_t current_capacity_;           // Current capacity
  std::size_t used_;                       // Used size for the current pool
  std::size_t maximum_size_;               // Maximum size of BumpAllocator
};


} // namespace core
} // namespace lavascript

#endif // BUMP_ALLOCATOR_H_
