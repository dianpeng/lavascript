#ifndef OS_LINUX_H_
#define OS_LINUX_H_
#include "all-static.h"
#include <cstdint>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

namespace lavascript {

class OS : AllStatic {
 public:
  static std::int64_t GetPid() { return static_cast<std::int64_t>(getpid()); }
  static inline std::uint64_t NowInMicroSeconds();
};

inline std::uint64_t OS::NowInMicroSeconds() {
  struct timespec tv;
  clock_gettime(CLOCK_MONOTONIC,&tv);
  return tv.tv_sec*1000000 + tv.tv_nsec/1000;
}

} // namespace lavascript

#endif // OS_LINUX_H_
