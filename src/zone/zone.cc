#include "zone.h"
#include <cstdlib>

namespace lavascript {
namespace zone {

void Zone::Grow( size_t require ) {
  size_t new_capacity = capacity_ * 2;
  if( new_capacity > maximum_ ) new_capacity = maximum_;
  if( !new_capacity ) new_capacity = minimum_;
  if( new_capacity < require ) new_capacity = require;
  void* new_chunk = ::malloc(new_capacity + sizeof(void*));
  ((Chunk*)(new_chunk))->next = first_chunk_;
  first_chunk_ = (Chunk*)(new_chunk);
  pool_ = (void*)(((char*)(new_chunk)) + sizeof(void*));
  left_ = new_capacity;
  capacity_ = new_capacity;
}

Zone::Zone():
  first_chunk_(NULL),
  capacity_   (0),
  pool_       (NULL),
  left_       (0),
  minimum_    (kMinimum),
  maximum_    (kMaximum)
{ Grow(0); }

Zone::Zone( size_t minimum , size_t maximum ):
  first_chunk_(NULL),
  capacity_   (0),
  pool_       (NULL),
  left_       (0),
  minimum_    (minimum),
  maximum_    (maximum)
{ Grow(0); }

Zone::~Zone()
{
  while(first_chunk_) {
    Chunk* n = first_chunk_->next;
    ::free(first_chunk_);
    first_chunk_ = n;
  }
}

} // namespace zone
} // namespace lavascript
