#ifndef INTRINSIC_CALL_H_
#define INTRINSIC_CALL_H_
#include "src/config.h"
#include "src/builtins.h"

/**
 * Intrinsic call are function call that has special meaning to Compiler/Interpreter.
 * It is treated as a special bytecode and during JIT compilation phase these intrinsic
 * will be translated into related special IR node and then lower to simple machine
 * instruction if applicable
 */

namespace lavascript {
namespace interpreter{

/**
 * All the builtin function call will be mapped to intrinsic function call
 *
 * List of Builtins
 */
enum IntrinsicCall {
#define __(A,B,...) INTRINSIC_CALL_##B,

  LAVASCRIPT_BUILTIN_FUNCTIONS(__)

#undef __ // __

  SIZE_OF_INTRINSIC_CALL
};

// We cannot have more than 256 intrinsic call due to the limitation of bytecode
static_assert( SIZE_OF_INTRINSIC_CALL <= kMaxIntrinsicCall );

// map a intrinsic call's function name to the IntrinsicCall index, if no such
// intrinsic call, then return SIZE_OF_INTRINSIC_CALL
IntrinsicCall MapIntrinsicCallIndex( const char* );

// get the argument count of intrinsic function call
std::uint8_t  GetIntrinsicCallArgumentSize( IntrinsicCall );

// get the intrinsic function call's name based on the intrinsic call index
const char* GetIntrinsicCallName  ( IntrinsicCall );

// get the intrinsic function call's error message
const char* GetIntrinsicCallErrorMessage( IntrinsicCall );

} // namespace interpreter
} // namespace lavascript


#endif // INTRINSIC_CALL_H_
