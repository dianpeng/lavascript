#include "bump-allocator.h"
#include "trace.h"

namespace lavascript {

void BumpAllocator::RefillPool( std::size_t size ) {
  const size_t total = size + sizeof(Segment);
  void* ptr = Malloc(allocator_,total);

  Segment* segment = reinterpret_cast<Segment*>(ptr);
  segment->next = segment_;
  segment_ = segment;

  pool_ = reinterpret_cast<void*>(
      static_cast<char*>(ptr) + sizeof(Segment));

  current_capacity_ = size;
  used_ = 0;
  total_bytes_ += total;
  ++segment_size_;
}

void* BumpAllocator::Grab( std::size_t size ) {
  lava_debug(NORMAL,lava_verify(size););

  if( used_ + size > current_capacity_ ) {
    size_t new_cap = current_capacity_ * 2;
    if(new_cap > maximum_size_) new_cap = maximum_size_;
    if(new_cap < size ) new_cap = size;
    RefillPool(new_cap);
  }

  lava_debug(NORMAL,
      lava_verify( current_capacity_ - used_ >= size );
      );

  void* ret = pool_;
  pool_ = reinterpret_cast<void*>(static_cast<char*>(pool_) + size);
  used_ += size;
  size_ += size;
  return ret;
}

void BumpAllocator::Clear() {
  while(segment_) {
    Segment* n = segment_->next;
    Free(allocator_,segment_);
    segment_ = n;
  }
}

void BumpAllocator::Reset() {
  Clear();

  pool_ = NULL;
  size_ = 0;
  current_capacity_ = 0;
  used_ = 0;
  segment_size_ = 0;

  RefillPool(init_capacity_);
}

} // namespace lavascript
