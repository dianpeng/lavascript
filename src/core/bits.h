#ifndef CORE_BITS_H_
#define CORE_BITS_H_

#include <cstdint>

namespace lavascript {
namespace core {

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

template< typename T , std::size_t N > struct OnMask {
  static const T value = static_cast<T>(1)<<static_cast<T>(N);
};

template< typename T , std::size_t N > struct OffMask {
  static const T value = ~OnMask<T,N>::value;
};

} // namespace

template< std::size_t N >
inline std::uint64_t Set( std::uint64_t value ) {
  return value | detail::OnMask<std::uint64_t,N>::value;
}

template< std::size_t N >
inline std::uint64_t Unset( std::uint64_t value ) {
  return value & detail::OffMask<std::uint64_t,N>::value;
}

template< std::size_t N >
inline std::uint32_t Set( std::uint32_t value ) {
  return value | detail::OnMask<std::uint32_t,N>::value;
}

template< std::size_t N >
inline std::uint32_t Unset( std::uint32_t value ) {
  return value & detail::OffMask<std::uint32_t,N>::value;
}

template< std::size_t N >
inline std::uint16_t Set( std::uint16_t value ) {
  return value | detail::OnMask<std::uint16_t,N>::value;
}

template< std::size_t N >
inline std::uint16_t Unset( std::uint16_t value ) {
  return value & detail::OffMask<std::uint16_t,N>::value;
}

template< std::size_t N >
inline std::uint8_t Set( std::uint8_t value ) {
  return value | detail::OnMask<std::uint8_t,N>::value;
}

template< std::size_t N >
inline std::uint8_t Unset( std::uint8_t value ) {
  return value & detail::OffMask<std::uint8_t,N>::value;
}

} // namespace core
} // namespace lavascript

#endif // CORE_BITS_H_
