#ifndef STL_HELPER_H_
#define STL_HELPER_H_
#include <vector>
#include <algorithm>

namespace lavascript {

// Use DynamicBitSet to represent the dynamic bitset and underly implementation should
// use real bit set to make memory efficient.
typedef std::vector<bool> DynamicBitSet;

inline void BitSetUnion( DynamicBitSet* lhs , const DynamicBitSet& rhs ) {
  DynamicBitSet temp;
  std::set_union( lhs->begin() , lhs->end() , rhs.begin() , rhs.end() ,
                                                            std::back_inserter(temp) );
  lhs->swap(temp);
}

inline void BitSetIntersection( DynamicBitSet* lhs , const DynamicBitSet& rhs ) {
  DynamicBitSet temp;
  std::set_intersection( lhs->begin() , lhs->end() , rhs.begin() ,
                                                     rhs.end() ,
                                                     std::back_inserter(temp) );
  lhs->swap(temp);
}

inline void BitSetDifference( DynamicBitSet* lhs , const DynamicBitSet& rhs ) {
  DynamicBitSet temp;
  std::set_difference( lhs->begin() , lhs->end() , rhs.begin() , rhs.end() ,
                                                                 std::back_inserter(temp) );
  lhs->swap(temp);
}

void BitSetReset( DynamicBitSet* set , bool value = false );

} // namespace lavascript

#endif // STL_HELPER_H_
