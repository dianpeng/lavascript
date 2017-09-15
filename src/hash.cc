#include "hash.h"

namespace lavascript {

std::uint32_t Hasher::Hash( const void* data , std::size_t length ) {
  std::uint32_t ret = 1777771;
  size_t i = 0;
  for( i = 0 ; i <length ; ++i ) {
    ret = (ret ^ ((ret<<5) + (ret>>2))) + static_cast<std::uint32_t>(
        static_cast<const char*>(data)[i]);
  }
  return ret;
}

} // namespace lavascript
