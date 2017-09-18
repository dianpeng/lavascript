#include "bump-allocator.h"
#include "trace.h"

namespace lavascript {

void BumpAllocator::RefillPool( std::size_t size ) {
  const size_t total = size + sizeof(Segment);
  void* ptr = Malloc(allocator_,total);

  Segment* segment = reinterpret_cast<Segment*>(ptr);
  segment->next = segment_;
  pool_ = reinterpret_cast<void*>(
      static_cast<char*>(ptr) + sizeof(Segment));

  current_capacity_ = size;
  used_ = 0;
  total_bytes_ += total;
  ++segment_size_;
}

void* BumpAllocator::Grab( std::size_t size ) {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(size);
#endif // LAVASCRIPT_CHECK_OBJECTS

  if( used_ + size > current_capacity_ ) {
    size_t new_cap = current_capacity_ * 2;
    if(new_cap > maximum_size_) new_cap = maximum_size_;
    if(new_cap < size ) new_cap = size;
    RefillPool(new_cap);
  }

#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify( current_capacity_ - used_ >= size );
#endif // LAVASCRIPT_CHECK_OBJECTS

  void* ret = pool_;
  pool_ = reinterpret_cast<void*>(static_cast<char*>(pool_) + size);
  used_ += size;
  ++size_;
  return ret;
}

BumpAllocator::~BumpAllocator() {
  while(segment_) {
    Segment* n = segment_->next;
    Free(allocator_,n);
    segment_ = n;
  }
}

} // namespace lavascript
