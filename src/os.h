#ifndef OS_H_
#define OS_H_

#include <cstdint>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

namespace lavascript {

class OS {
 public:
  static inline std::int64_t GetPid();
  static inline std::uint64_t NowInMicroSeconds();

  // Get the memory page size
  static std::size_t GetPageSize();

  // The following 2 functions are specifically used for allocate pages for
  // machine code. The underlying API will try its best to get memory from
  // lower 2GB memory address which is better for our assembler.
  static void* CreateCodePage( std::size_t size , std::size_t* adjusted_size );

  static void  FreeCodePage  ( void* , std::size_t size );
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

#endif // OS_H_
