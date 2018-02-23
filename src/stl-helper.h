#ifndef STL_HELPER_H_
#define STL_HELPER_H_
#include <vector>
#include <algorithm>

namespace lavascript {

// Use DynamicBitSet to represent the dynamic bitset and underly implementation should
// use real bit set to make memory efficient.
typedef std::vector<bool> DynamicBitSet;

void BitSetReset( DynamicBitSet* set , bool value = false );

template< typename T >
typename T::iterator IteratorAt( T& container , std::size_t pos ) {
  typename T::iterator p(container.begin());
  std::advance(p,pos);
  return p;
}

template< typename T >
typename T::const_iterator IteratorAt( const T& container , std::size_t pos ) {
  typename T::cosnt_iterator p(container.begin());
  std::advance(p,pos);
  return p;
}

} // namespace lavascript

#endif // STL_HELPER_H_
