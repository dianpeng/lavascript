#ifndef OS_LINUX_H_
#define OS_LINUX_H_

#include <cstdint>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

namespace lavascript {

class OS {
 public:
  static inline std::int64_t GetPid();
  static inline std::uint64_t NowInMicroSeconds();
};

inline std::int64_t OS::GetPid() {
  return static_cast<std::int64_t>(getpid());
}

inline std::uint64_t OS::NowInMicroSeconds() {
  struct timespec tv;
  clock_gettime(CLOCK_MONOTONIC,&tv);
  return tv.tv_sec*1000000 + tv.tv_nsec/1000;
}

} // namespace lavascript

#endif // OS_LINUX_H_
