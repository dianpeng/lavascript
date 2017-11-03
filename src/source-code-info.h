#ifndef SOURCE_CODE_INFO_H_
#define SOURCE_CODE_INFO_H_
#include <cstdint>

namespace lavascript {

struct SourceCodeInfo {
  std::uint32_t start;
  std::uint32_t end;

  SourceCodeInfo():start(0),end(0){}

  SourceCodeInfo( std::size_t s , std::size_t e ):
    start(static_cast<std::uint32_t>(s)),
    end  (static_cast<std::uint32_t>(e))
  {}

  SourceCodeInfo( std::uint32_t s , std::uint32_t e ):
    start(s),
    end  (e)
  {}
};

static_assert( sizeof(SourceCodeInfo) == 8 );

} // namespace lavascript

#endif // SOURCE_CODE_INFO_H_
