#ifndef BYTECODE_INTERPRETER_H_
#define BYTECODE_INTERPRETER_H_
#include "src/objects.h"
#include "bytecode.h"

namespace lavascript {
class Context;
namespace interpreter{
class Interpreter;

// I write it as C struct to make assembly code easier to access each field
// inside of the *sandbox* object
struct Sandbox {
  // context related information
  Script** script;
  Object** global;
  String** error;
  Value ret;

  // stack
  std::uintptr_t* stack;           // Stack of interpreter
  std::uint32_t stack_size;        // Size of the stack

  Context* context;                // Context pointer

  // interpreter
  Interpreter* interp;
};

// We can use dynasm's syntax sugar on struct but to maintain consistency
// we still use this Layout structure to work around C++ private member
struct SandboxLayout {
  static const std::uint32_t kScriptOffset = offsetof(Sandbox,script);
  static const std::uint32_t kGlobalOffset = offsetof(Sandbox,global);
  static const std::uint32_t kErrorOffset  = offsetof(Sandbox,error);
  static const std::uint32_t kRetOffset    = offsetof(Sandbox,ret);
  static const std::uint32_t kStackOffset  = offsetof(Sandbox,stack);
  static const std::uint32_t kStackSizeOffset    = offsetof(Sandbox,stack_size);
  static const std::uint32_t kContextOffset= offsetof(Sandbox,context);
  static const std::uint32_t kInterpOffset = offsetof(Sandbox,interp);
};

// Function that generates the interpreter , call it during the initialization
typedef bool (*Interpret)( Sandbox* , Script* );

// Interpreter context for doing the interpretation
struct Interpreter {
  void* dispatch_interp[ SIZE_OF_BYTECODE ];
  void* dispatch_record[ SIZE_OF_BYTECODE ];
  void* dispatch_jit   [ SIZE_OF_BYTECODE ];

  // Entry of the interpreter , which is generated on the fly.
  //
  // TODO::
  // This piece of memory is *purposely* leaked for now , we will
  // have a better way to guard it and free it.
  Interpret entry;

  // how long does the code actually occupied
  std::size_t code_size;

  // buffer size , aligned with page size of OS
  std::size_t buffer_size;
};


// Generate interpreter for interpreting purpose
void GenerateInterpreter( Interpreter* );

} // namespace interpreter
} // namespace lavascript


#endif // BYTECODE_INTERPRETER_H_
