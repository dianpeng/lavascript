#ifndef HASH_H_
#define HASH_H_

#include <cstdint>
#include "all-static.h"

namespace lavascript {


class Hasher : public AllStatic {
 public:
  static std::uint32_t Hash( const char* );
  static std::uint32_t Hash( const char* , size_t len );
};

} // namespace lavascript

#endif // HASH_H_
