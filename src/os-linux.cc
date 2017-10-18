#include "os-linux.h"
#include "util.h"
#include "trace.h"

#include <unistd.h>
#include <sys/mman.h>

namespace lavascript {

std::size_t OS::GetPageSize() {
  return static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
}

void* OS::CreateCodePage( std::size_t size , std::size_t* adjusted_size ) {
  const std::size_t page_size = GetPageSize();
  std::size_t nsize = Align(size,page_size);
  *adjusted_size = nsize;

  static const int kFlag = MAP_ANONYMOUS | MAP_PRIVATE | MAP_32BIT;
  static const int kProtection = PROT_EXEC | PROT_READ | PROT_WRITE;

  return mmap(NULL,nsize,kProtection,kFlag,-1,0);
}

void OS::FreeCodePage( void* ptr , std::size_t size ) {
  lava_verify( Align(size,GetPageSize()) == size );
  lava_verify( munmap(ptr,size) );
}

} // namespace lavascript
