#ifndef ZONE_STACK_H_
#define ZONE_STACK_H_
#include "vector.h"

namespace lavascript {
namespace zone {

// Stack , just a wrapper around the Vector to provide API to make it
// as a stack
template< typename T > class Stack {
 public:
  const T& Top() const { return vec_.Last(); }
  T& Top() { return vec_.Last(); }
  void Push( Zone* zone , const T& val ) { vec_.PushBack(zone,val); }
  void Pop () { vec_.PopBack(); }
  bool empty() const { return vec_.empty(); }

 public:
  const Vector<T>& vec() const { return vec_; }

 private:
  Vector<T> vec_;
};

} // namespace zone
} // namespace lavascript

#endif // ZONE_STACK_H_
