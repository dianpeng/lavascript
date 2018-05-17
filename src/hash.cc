#include "hash.h"
#include "bits.h"

namespace lavascript {

namespace {
// https://gist.github.com/badboy/6267743
template <typename T>
inline size_t IntegerHash(T v) {
  switch (sizeof(T)) {
    case 4: {
      // "32 bit Mix Functions"
      v = ~v + (v << 15);  // v = (v << 15) - v - 1;
      v = v ^ (v >> 12);
      v = v + (v << 2);
      v = v ^ (v >> 4);
      v = v * 2057;  // v = (v + (v << 3)) + (v << 11);
      v = v ^ (v >> 16);
      return static_cast<size_t>(v);
    }
    case 8: {
      switch (sizeof(size_t)) {
        case 4: {
          // "64 bit to 32 bit Hash Functions"
          v = ~v + (v << 18);  // v = (v << 18) - v - 1;
          v = v ^ (v >> 31);
          v = v * 21;  // v = (v + (v << 2)) + (v << 4);
          v = v ^ (v >> 11);
          v = v + (v << 6);
          v = v ^ (v >> 22);
          return static_cast<size_t>(v);
        }
        case 8: {
          // "64 bit Mix Functions"
          v = ~v + (v << 21);  // v = (v << 21) - v - 1;
          v = v ^ (v >> 24);
          v = (v + (v << 3)) + (v << 8);  // v * 265
          v = v ^ (v >> 14);
          v = (v + (v << 2)) + (v << 4);  // v * 21
          v = v ^ (v >> 28);
          v = v + (v << 31);
          return static_cast<size_t>(v);
        }
      }
    }
  }
  lava_die(); return 0;
}

template< typename T >
inline T StringHash(const void* data , std::size_t length) {
  static const std::uint32_t kHashSeed = 177771;
  T ret = kHashSeed;
  size_t i = 0;
  for( i = 0 ; i <length ; ++i ) {
    ret = (ret ^ ((ret<<5) + (ret>>2))) +
      static_cast<T>(static_cast<const char*>(data)[i]);
  }
  return ret;
}

} // namespace


std::uint32_t Hasher::Hash( const void* data , std::size_t length ) {
  return StringHash<std::uint32_t>(data,length);
}

std::uint32_t Hasher::Hash( std::uint32_t value ) {
  return IntegerHash(value);
}

std::uint32_t Hasher::HashCombine( std::uint32_t lhs , std::uint32_t rhs ) {
  const std::uint32_t c1 = 0xcc9e2d51;
  const std::uint32_t c2 = 0x1b873593;

  rhs *= c1;
  rhs  = bits::BRor(rhs,15);
  rhs *= c2;

  lhs ^= rhs;
  lhs = bits::BRor(lhs,13);
  lhs = lhs * 5 + 0xe6546b64;

  return lhs;
}

std::uint64_t Hasher::Hash64( const void* data , std::size_t length ) {
  return StringHash<std::uint64_t>(data,length);
}

std::uint64_t Hasher::Hash64( std::uint64_t value ) {
  return IntegerHash(value);
}

std::uint64_t Hasher::HashCombine64( std::uint64_t lhs , std::uint64_t rhs ) {
  const std::uint64_t m = 0xc6a4a7935bd1e995;
  const std::uint32_t r = 47;

  rhs *= m;
  rhs ^= rhs >> r;
  rhs *= m;

  lhs ^= rhs;
  lhs *= m;
  return lhs;
}

} // namespace lavascript
