#ifndef JIT_PROFILE_DATA_H_
#define JIT_PROFILE_DATA_H_

#include "src/config.h"

namespace lavascript {
namespace compiler   {

struct JITHotCountData {
  hotcount_t loop_hot_count[kHotCountArraySize];
  hotcount_t call_hot_count[kHotCountArraySize];
};

} // namespace compiler
} // namespace lavascript

#endif // JIT_PROFILE_DATA_H_
