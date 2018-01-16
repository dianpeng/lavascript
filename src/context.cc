#include "context.h"

namespace lavascript {

void Context::PopCurrentRuntime() {
  lava_debug(NORMAL,lava_verify(runtime_););
  runtime_ = runtime_->previous;
}

} // namespace lavascript
