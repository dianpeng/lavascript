#ifndef BYTECODE_INTERPRETER_H_
#define BYTECODE_INTERPRETER_H_
#include "src/objects.h"
#include "bytecode.h"

namespace lavascript {
class Context;
namespace interpreter{

// I write it as C struct to make assembly code easier to access each field
// inside of the *sandbox* object
struct Sandbox {
  // context related information
  Script** script;
  Object** global;
  String** error;

  // stack
  std::uintptr_t* stack;           // Stack of interpreter
  std::uint32_t stack_size;        // Size of the stack

  Context* context;                // Context pointer
};

// Interpreter context for doing the interpretation
struct Interpreter {
  void* dispatch_interp[ SIZE_OF_BYTECODE ];
  void* dispatch_record[ SIZE_OF_BYTECODE ];
  void* dispatch_jit   [ SIZE_OF_BYTECODE ];
};

// Function that generates the interpreter , call it during the initialization
typedef int (*Interpret)( void* , void* , void* );

// Generate interpreter for interpreting purpose
Interpret GenerateInterpreter();

} // namespace interpreter
} // namespace lavascript

#endif // BYTECODE_INTERPRETER_H_
