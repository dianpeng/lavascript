#ifndef COMMON_H_
#define COMMON_H_
#include "config.h"

#define LAVA_DISALLOW_COPY_AND_ASSIGN(X) \
  void operator = (const X&) = delete;   \
  X(const X&) = delete;

#ifdef __GNUG__
#define LAVA_ALWAYS_INLINE inline __attribute__((always_inline))
#define LAVA_NOT_INLINE    __attribute__((noinline))
#else
#error "compiler not support currently!!"
#endif // __GNUG__

#endif // COMMON_H_
