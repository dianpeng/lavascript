#ifndef IFRAME_H_
#define IFRAME_H_
#include <string>
#include <cstdint>
#include <type_traits>

#include "src/trace.h"

namespace lavascript {
class Closure;
class Extension;
class CompilationJob;

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
// [BASE           (16bits)][PC  (48bits)]  Field1
// [Flag(8bits)][Narg8bits)][CLS (48bits)]  Field2
//
// We can get the *previous* stack frame by doing CUR_STK - BASE*8
//
//
// NOTES: narg value is *not* set unless it is a call into a Extension object
//        which interpreter will not handle by itself. If thats the cases the
//        narg will be set to *how many arguments are passed by the caller*.
//        If the frame is generated inside of the interpreter, the Narg is not
//        used since you could always tell the argument by looking at the closure
//
// The reason why such structure is presented is because this is easier to
// write assembly code
// -----------------------------------------------------------------------
struct IFrame {
  CompilationJob** cjob; // Pointer points CompilationJob
  std::uint64_t field1;  // First  8 bytes
  std::uint64_t field2;  // Second 8 bytes

 public:
  inline void SetUpAsExtension( std::uint16_t base , const std::uint32_t* pc ,
                                                     bool tcall ,
                                                     std::uint8_t narg ,
                                                     Extension** );

  inline void set_pc( const std::uint32_t* pc );

  inline std::uint16_t base() const;
  inline const std::uint32_t* pc() const;
  inline Closure** closure() const;
  inline Extension** extension() const;
  inline bool tcall() const;
  inline std::uint8_t narg() const;

  enum { CLOSURE_CALL = 0 , EXTENSION_CALL = 1 };
  inline int call_type() const;
};

static_assert( std::is_standard_layout<IFrame>::value );
static_assert( sizeof(IFrame) == 24 );

struct IFrameLayout {
  static const std::uint32_t kField1Offset = offsetof(IFrame,field1);
  static const std::uint32_t kField2Offset = offsetof(IFrame,field2);
};

inline void IFrame::SetUpAsExtension( std::uint16_t base , const std::uint32_t* pc ,
                                                           bool tcall ,
                                                           std::uint8_t narg ,
                                                           Extension** cls ) {
  std::uint64_t temp = static_cast<std::uint64_t>(base) << 48;
  field1 = temp | reinterpret_cast<std::uint64_t>(pc);
  field2 = (static_cast<std::uint64_t>(tcall) << 56) |
           (bits::BitOn<std::uint64_t,57,58>::value) |
           (static_cast<std::uint64_t>(narg)  << 48) |
           (reinterpret_cast<std::uint64_t>(cls));
}

inline void IFrame::set_pc( const std::uint32_t* pc ) {
  std::uint64_t temp = field1 & (bits::BitOn<std::uint64_t,48,64>::value);
  field1 = temp | reinterpret_cast<std::uint64_t>(pc);
}

inline std::uint16_t IFrame::base() const {
  std::uint64_t t = field1;
  return static_cast<std::uint16_t>(t >>48);
}

inline const std::uint32_t* IFrame::pc() const {
  return reinterpret_cast<const std::uint32_t*>(
      bits::BitOn<std::uint64_t,0,48>::value & field1);
}

inline Closure** IFrame::closure() const {
  lava_debug(NORMAL,lava_verify(call_type() == CLOSURE_CALL););
  return reinterpret_cast<Closure**>(
      bits::BitOn<std::uint64_t,0,48>::value & field2);
}

inline Extension** IFrame::extension() const {
  lava_debug(NORMAL,lava_verify(call_type() == EXTENSION_CALL););
  return reinterpret_cast<Extension**>(
      bits::BitOn<std::uint64_t,0,48>::value & field2);
}

inline bool IFrame::tcall() const {
  std::uint64_t temp = field2 & bits::BitOn<std::uint64_t,56,57>::value;
  return static_cast<bool>((temp>>56));
}

inline int IFrame::call_type() const {
  std::uint64_t temp = field2 & bits::BitOn<std::uint64_t,57,58>::value;
  return static_cast<int>(temp>>57);
}

inline std::uint8_t IFrame::narg() const {
  std::uint64_t temp = field2 & bits::BitOn<std::uint64_t,48,56>::value;
  return static_cast<std::uint8_t>(temp >> 48);
}

// ------------------------------------------------------------------------
// Walk the interpreter's frame back to the top. Mostly used to generate
// runtime error message while doing interpretation
void InterpreterStackWalk( const struct IFrame& tos , std::string* buffer );

} // namespace interpreter
} // namespace lavascript

#endif // IFRAME_H_
