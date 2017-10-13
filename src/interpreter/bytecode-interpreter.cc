#include "bytecode-interpreter.h"
#include "dep/dynasm/dasm_proto.h"
#include "dep/dynasm/dasm_x86.h"

namespace lavascript {
namespace interpreter{
namespace {

// A frame object that is used to record the function's runtime
// information
struct Frame {
  void* caller;
  std::int32_t offset;
};

|.arch x64
|.actionlist actions

/* ---------------------------------------------------------------
 * summary of register usage
 * --------------------------------------------------------------*/
|.define SBOX rsi
|.define STACK rdx
|.define DISP r15

/* ---------------------------------------------------------------
 * dispatch table
 * --------------------------------------------------------------*/

// rdi ---> hold *Sandbox* object
// rsi ---> hold *starting* point of the stack object

void GenerateOneBytecode( Bytecode bc ) {
  switch(bc) {
  }
}


} // namespace
} // namespace lavascript
} // namespace interpreter
