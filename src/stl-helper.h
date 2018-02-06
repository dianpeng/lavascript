#ifndef STL_HELPER_H_
#define STL_HELPER_H_
#include <vector>
#include <algorithm>

namespace lavascript {

// Use DynamicBitSet to represent the dynamic bitset and underly implementation should
// use real bit set to make memory efficient.
typedef std::vector<bool> DynamicBitSet;

void BitSetReset( DynamicBitSet* set , bool value = false );

} // namespace lavascript

#endif // STL_HELPER_H_
