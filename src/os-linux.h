#ifndef OS_LINUX_H_
#define OS_LINUX_H_

#include "all-static.h"

#include <inttypes.h>
#include <sys/types.h>
#include <unistd.h>

namespace lavascript {

class OS : AllStatic {
 public:
  static int64_t GetPid() { return static_cast<int64_t>(getpid()); }
};

} // namespace lavascript

#endif // OS_LINUX_H_
