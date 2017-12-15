#ifndef CBASE_DEOPTIMIZE_H_
#define CBASE_DEOPTIMIZE_H_

/**
 * Deoptimization of CBase compiler.
 *
 * In CBase we support 3 different types of compilation.
 *
 * 1. General Compliation.
 *
 *    Cannot support deoptimization and it is called from native to native
 *
 * 2. OSR Compilation
 *
 *    Jump into the function via OSR entry and it will be deoptimized once
 *    it finishes the function. This function *doesn't* have normal main
 *
 * 3. Specialized Compilation
 * 
 *    Compile a method based on the past profile. This function will be
 *    deopimized when its profile assumption doesn't hold.
 *
 * The 2 and 3 types compliation will only be triggerd from interpreter since
 * deopimization will fallback to interpreter not to native code. The type 1
 * compilation is triggerd inside of the a compliation job of type 2 and 3 one
 * we decide not to inline a certain function. Since the caller is native function,
 * we don't support deoptimization.
 *
 */

namespace lavascript {
namespace cbase      {

/**
 * The deopimization is performed via a C++ function called inside of generated
 * assembly code. The assembly code will tell you which information we needs to
 * use to generate the interpreter frame from the native frame.
 *
 * These information are gathered inside of the compiler and store along with the
 * function itself.
 *
 * In general, what we need to do for deopimization includes:
 * 1) restore value in interpreter's stack
 * 2) restore value in upvalue stack
 * 3) restore value in global table ( if global variable sink is not enabled )
 * 4) restore call frame for interpreter due to the inline
 *
 */

} // namespace lavascript
} // namespace cbase

#endif // CBASE_DEOPTIMIZATION_H_
