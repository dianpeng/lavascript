#ifndef INTERPRETER_FRAME_H_
#define INTERPRETER_FRAME_H_
#include <string>
#include <cstdint>
#include <type_traits>

#include "src/trace.h"
#include "src/objects.h"

namespace lavascript {
namespace interpreter{

// -----------------------------------------------------------------------
// An interpreter frame. This frame sits on top of our evaluation
// stack.
//
// Based on the arrangements, STK pointer points at the *start*
// of the evaluation stack. And *before* this STK pointer we will
// have interpreter frame object sits on top of the evaluation
// stack.
//
//
// LuaJIT has such an elegant fram structure which makes its call in the
// interpreter *extreamly* fast. Our design has some difference with LuaJIT
// which makes us not able to achieve such simplicity. But I try my best to
// avoid overhead.
//
// 1) We don't support var arg , also if a argument is not passed with
//    value it won't initilaize with null but directly yield an error.
//    This save us needs to *store* the #argument.
//
// [BASE           (16bits)][PC  (48bits)]
// [Flag(8bits)][...(8bits)][CLS (48bits)]
//
// We can get the *previous* stack frame by doing CUR_STK - BASE*8
//
//
// The reason why such structure is presented is because this is easier to
// write assembly code
// -----------------------------------------------------------------------
struct IFrame {
  std::uint64_t field1;
  std::uint64_t field2;
};

static_assert( std::is_standard_layout<IFrame>::value );
static_assert( sizeof(IFrame) == 16 );

// Reserved slots/register for *holding* this IFrame structure while we are
// in the *call*
static const std::size_t kReserveCallStack = 16;
static const std::size_t kReserveCallStackSlot = kReserveCallStack / sizeof(Value);

struct IFrameLayout {
  static const std::uint32_t kField1Offset = offsetof(IFrame,field1);
  static const std::uint32_t kField2Offset = offsetof(IFrame,field2);
};

// ------------------------------------------------------------------------
// Walk the interpreter's frame back to the top. Mostly used to generate
// runtime error message while doing interpretation
void InterpreterStackWalk( const struct IFrame& tos , std::string* buffer );

} // namespace interpreter
} // namespace lavascript

#endif // INTERPRETER_FRAME_H_
