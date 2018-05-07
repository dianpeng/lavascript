#ifndef BITS_H_
#define BITS_H_
#include <cstdint>

namespace lavascript {
namespace bits {

/**
 * Some helper stuff for bits manipulation
 */
inline std::uint32_t High64( std::uint64_t value ) {
  static const std::uint64_t kMask = 0xffffffff00000000;
  return static_cast<std::uint32_t>( (value & kMask) >> 32 );
}

inline std::uint32_t Low64( std::uint64_t value ) {
  static const std::uint64_t kMask = 0x00000000ffffffff;
  return static_cast<std::uint32_t>( (value & kMask) );
}

inline std::uint16_t High32( std::uint32_t value ) {
  static const std::uint32_t kMask = 0xffff0000;
  return static_cast<std::uint16_t>((value & kMask) >> 16 );
}

inline std::uint16_t Low32( std::uint32_t value ) {
  static const std::uint32_t kMask = 0x0000ffff;
  return static_cast<std::uint16_t>((value & kMask));
}

inline std::uint8_t High16( std::uint16_t value ) {
  static const std::uint16_t kMask = 0xff00;
  return static_cast<std::uint8_t>((value & kMask));
}

inline std::uint8_t Low16 ( std::uint16_t value ) {
  static const std::uint16_t kMask = 0x00ff;
  return static_cast<std::uint8_t>((value & kMask));
}

namespace detail {

template< typename T , std::size_t Start , std::size_t End >
struct BitOnImpl {
  static const T value = BitOnImpl<T,Start+1,End>::value |
                         static_cast<T>(static_cast<T>(1) << Start);
};

template< typename T , std::size_t End >
struct BitOnImpl<T,End,End> {
  static const T value = 0;
};

} // namespace detail

/**
 * The BitOn and BitOff struct takes a Start and End index to
 * indicate where to start our Bit flag and where to End flag.
 *
 * Example:
 *
 * std::uint32_t mask = BitOn<std::uint32_t,1,3>::value;
 *
 * will generate mask with value 0b0....011
 */

template< typename T , std::size_t Start , std::size_t End >
struct BitOn {
  static const std::size_t value =
    detail::BitOnImpl<T,Start,End>::value;
};

template< typename T , std::size_t Start , std::size_t End >
struct BitOff {
  static const std::size_t value =
    ~detail::BitOnImpl<T,Start,End>::value;
};

inline std::uint16_t NextPowerOf2( std::uint16_t v ) {
  --v;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  return v + 1;
}

inline std::uint32_t NextPowerOf2( std::uint32_t v ) {
  --v;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  return v+1;
}

inline std::uint64_t NextPowerOf2( std::uint64_t v ) {
  --v;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v |= v >> 32;
  return v+1;
}

// mimic rol/ror x64 instruction since C++ doesn't have builtin operator supports
inline std::uint32_t BRol( std::uint32_t lhs , std::uint8_t rhs ) {
  return (lhs << rhs) | (lhs >> (32-rhs));
}

inline std::uint32_t BRor( std::uint32_t lhs , std::uint8_t rhs ) {
  return (lhs >> rhs) | (lhs << (32-rhs));
}

} // namespace bits
} // namespace lavascript

#endif // BITS_H_
