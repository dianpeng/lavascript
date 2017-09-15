#ifndef COMMON_H_
#define COMMON_H_
#include "config.h"

#define LAVA_DISALLOW_COPY_AND_ASSIGN(X) \
  void operator = (const X&); \
  X(const X&)


#endif // COMMON_H_
