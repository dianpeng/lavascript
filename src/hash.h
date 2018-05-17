#ifndef HASH_H_
#define HASH_H_
#include <cstdint>
#include "all-static.h"

namespace lavascript {
// Hash function helper
class Hasher : public AllStatic {
 public:
  // 32 bits hashing
  static std::uint32_t Hash( const void* , std::size_t );
  static std::uint32_t Hash( std::uint32_t );
  static std::uint32_t HashCombine( std::uint32_t , std::uint32_t );

  // 64 bits hashing
  static std::uint64_t Hash64( const void* , std::size_t );
  static std::uint64_t Hash64( std::uint64_t );
  static std::uint64_t HashCombine64( std::uint64_t , std::uint64_t );
};
} // namespace lavascript

#endif // HASH_H_
