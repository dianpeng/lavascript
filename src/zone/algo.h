#ifndef ZONE_ALGO_H_
#define ZONE_ALGO_H_
#include "src/trace.h"

#include <functional>

/**
 * I should use STL with customized allocator really, but I have already gone this far and it
 * is not easy to go back to STL with all the customized container. Anyway, I will have to
 * replicate some STL algorithm here to ease the pain for using all the zone based container.
 */

namespace lavascript {
namespace zone       {

// A iterator adpater to limit how many steps this iterator should have
template< typename ITR > class CountedIterator {
 public:
  typedef typename ITR::ValueType ValueType;
  CountedIterator( const ITR& itr , std::size_t limit ) : itr_(itr), limit_(limit) {}
  const ValueType& value() const { return itr_.value(); }
  bool  HasNext         () const { return limit_ == 0 || itr_.HasNext(); }
  bool  Move            () const {
    lava_debug(NORMAL,lava_verify(HasNext()););
    if(--limit_ == 0) return false;
    return itr_.Move();
  }
 private:
  ITR itr_;
  mutable std::size_t limit_;
};


} // namespace zone
} // namespace lavascript

#endif // ZONE_ALGO_H_
