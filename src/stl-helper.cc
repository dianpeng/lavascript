#include "stl-helper.h"

namespace lavascript {

void BitSetReset( DynamicBitSet* set , bool value ) {
  for( std::size_t i = 0 ; i < set->size() ; ++i ) {
    set->at(i) = value;
  }
}

} // namespace lavascript
