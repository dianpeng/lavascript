#ifndef X64_ASSEMBLER_H_
#define X64_ASSEMBLER_H_
#include <cstdint>
#include <src/trace.h>
#include <src/bits.h>
#include <src/util.h>

/**
 * A x64 assembler. Notes it only supports x64 not x86.
 *
 * Initial design is using DynASM from LuaJIT . But now I find it is
 * easy to tailor code from V8 so I decide to just have a real assembler
 * inside of lavascript.
 *
 * The following code is heavily influenced by V8's assembler-x86.h/.cc
 * file.
 */

namespace lavascript {
namespace x64 {

} // namespace x64
} // namespace lavascript

#endif // X64_LAVASCRIPT_H_
