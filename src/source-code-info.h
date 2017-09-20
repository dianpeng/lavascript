#ifndef SOURCE_CODE_INFO_H_
#define SOURCE_CODE_INFO_H_
#include <cstdint>

namespace lavascript {

struct SourceCodeInfo {
  std::size_t start;
  std::size_t end;
  SourceCodeInfo():start(0),end(0) {}
  SourceCodeInfo( std::size_t s , std::size_t e ):start(s),end(e){}
};

} // namespace lavascript

#endif // SOURCE_CODE_INFO_H_
