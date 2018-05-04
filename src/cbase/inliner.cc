#include "inliner.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

bool StaticInliner::ShouldInline( std::size_t depth , const Handle<Prototype>& proto ) {
  if(depth > max_inline_depth_)
    return false;
  // not accurate due to we have 2 dword instructions
  auto bccnt = proto->code_buffer_size();
  // last resort
  if(bccnt < max_inline_bytecode_per_func_) {
    total_inlined_bytecode_ += bccnt;
    if(total_inlined_bytecode_ > max_inline_bytecode_total_)
      return false;
    return true;
  }
  return false;
}

} // namespace hir
} // namespace cbase
} // namespace lavascript
