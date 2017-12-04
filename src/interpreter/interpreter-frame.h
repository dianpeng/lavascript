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

// The IFrame is 16 bytes length what it stores is:
// 1) Pointer (32 bits) to previous IFrame ( relative offset )
// 2) Pointer (48 bits) to the field2 object, extension/closure
// 3) PC offset (16 bits)
// 4) NArg , number of argument (8 bits)
// 5) Flag , 0 indicate normal call , 1 means tail call
// -----------------------------------------------------------------------
// [Reserve (16 bits)][PC  (16bits)][PFrame pointer (32 bits)]
// [Narg     (8 bits)][Flag(8 bits)][Caller (48 bits)]

// To make it easy to be accessed via assembly code we wrap it as normal
// C structure and put all the accessor outside of the structure as normal
// functions
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

inline IFrame* IFrameGetPreviousFrame( const IFrame& iframe ) {
  static const std::uint64_t kMask = 0x0000ffffffffffffUL;
  return reinterpret_cast<IFrame*>( iframe.field1 & kMask );
}

inline std::uint16_t IFrameGetPCOffset( const IFrame& iframe ) {
  std::uint64_t val = ((iframe.field1) >> 48);
  return static_cast<std::uint16_t>(val);
}

inline std::uint16_t IFrameGetNArg( const IFrame& iframe ) {
  std::uint64_t val = ((iframe.field2) >> 56);
  return static_cast<std::uint8_t>(val);
}

inline void* IFrameGetCallerPointer( const IFrame& iframe ) {
  static const std::uint64_t kMask = 0x0000ffffffffffffUL;
  return reinterpret_cast<void*>( iframe.field2 & kMask );
}

// ------------------------------------------------------------------------
// Walk the interpreter's frame back to the top. Mostly used to generate
// runtime error message while doing interpretation
void InterpreterStackWalk( const struct IFrame& tos , std::string* buffer );

} // namespace interpreter
} // namespace lavascript

#endif // INTERPRETER_FRAME_H_
