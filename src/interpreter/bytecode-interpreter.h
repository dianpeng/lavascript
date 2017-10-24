#ifndef BYTECODE_INTERPRETER_H_
#define BYTECODE_INTERPRETER_H_
#include "src/objects.h"
#include "bytecode.h"

#include <memory>
#include <type_traits>

namespace lavascript {
class Context;
namespace interpreter{

struct AssemblyInterpreterLayout;

// AssemblyInterpreter represents a interpreter function/routine that is generated
// on the fly. It should be just generated *once* and all the created context just
// use this one. It is a function so it doesn't contain *states*.
class AssemblyInterpreter {
 public:
  // Create an AssemblyInterpreter. After creation , the machine code for the
  // interpreter will be generated and also the dispatch table will be setup
  // correctly.
  //
  // Here we use a shared_ptr since we will use this assembly interpreter always
  // because we don't need to create a new assembly interpreter everytime. Sort
  // of like Singleton but just via a shared_ptr.
  static std::shared_ptr<AssemblyInterpreter> Generate();
 public:
  ~AssemblyInterpreter();

  struct InstanceLayout;

  // Instance maintains a stateful dispatch table due to the fact that we need this
  // state for *recording/tracing* and jitting purpose. The entry/function for interpreter
  // doesn't have state but it requires certain state to properly execute.
  class Instance {
   public:
    Instance( const std::shared_ptr<AssemblyInterpreter>& interp );

    // Execute the *script* starting from its *main* function with *environment*
    bool Run( Context* context , const Handle<Script>& , const Handle<Object>& ,
                                                         std::string* , Value* );

   private:
    // Purposely duplicate these 3 fields to ease the pain of access them
    // inside of assembly code.
    void* dispatch_interp_ [ SIZE_OF_BYTECODE ];
    void* dispatch_record_ [ SIZE_OF_BYTECODE ];
    void* dispatch_jit_    [ SIZE_OF_BYTECODE ];

    std::shared_ptr<AssemblyInterpreter> interp_;

    friend struct InstanceLayout;
    LAVA_DISALLOW_COPY_AND_ASSIGN(Instance);
  };

 public:

  // Dump the interpreter into human readable assembly into the DumpWriter
  void Dump( DumpWriter* ) const;

 private:
  Bytecode CheckBytecodeRoutine( void* pc ) const;
  int      CheckHelperRoutine  ( void* pc ) const;

 private:
  AssemblyInterpreter();

  // The following table are *not* modified and should not modified. The instance object
  // contains a mutable dispatch table instance and those are the tables that *should* be
  // handled into the main assembly routine for interpretation

  // dispatch table for each bytecode in interpretation mode
  void* dispatch_interp_[ SIZE_OF_BYTECODE ];

  // dispatch table for each bytecode in recording mode
  void* dispatch_record_[ SIZE_OF_BYTECODE ];

  // dispatch table for each bytecode in jitting mode
  void* dispatch_jit_   [ SIZE_OF_BYTECODE ];

  // internal helper routine's entry
  std::vector<void*> interp_helper_;

  // main assembly interpreter routine. this entry may not be a valid C++ function
  // we can use inline assembly to jmp to this routine
  void* interp_entry_;

  // code buffer
  void* code_buffer_;

  // code size for this buffer
  std::size_t code_size_;

  // the actual buffer size , this number will be aligned with page size
  std::size_t buffer_size_;

  friend class Instance;
  friend struct AssemblyInterpreterLayout;
  LAVA_DISALLOW_COPY_AND_ASSIGN(AssemblyInterpreter);
};

static_assert( std::is_standard_layout<AssemblyInterpreter>::value );

struct AssemblyInterpreterLayout {
  static const std::uint32_t kDispatchInterpOffset = offsetof(AssemblyInterpreter,dispatch_interp_);
  static const std::uint32_t kDispatchRecordOffset = offsetof(AssemblyInterpreter,dispatch_record_);
  static const std::uint32_t kDispatchJitOffset    = offsetof(AssemblyInterpreter,dispatch_jit_   );
  static const std::uint32_t kInterpEntryOffset    = offsetof(AssemblyInterpreter,interp_entry_   );
  static const std::size_t   kCodeSizeOffset       = offsetof(AssemblyInterpreter,code_size_      );
  static const std::size_t   kBufferSizeOffset     = offsetof(AssemblyInterpreter,buffer_size_    );
};

struct AssemblyInterpreter::InstanceLayout {
  static const std::uint32_t kDispatchInterpOffset = offsetof(AssemblyInterpreter::Instance,dispatch_interp_);
  static const std::uint32_t kDispatchRecordOffset = offsetof(AssemblyInterpreter::Instance,dispatch_record_);
  static const std::uint32_t kDispatchJitOffset    = offsetof(AssemblyInterpreter::Instance,dispatch_jit_   );
};

} // namespace interpreter
} // namespace lavascript


#endif // BYTECODE_INTERPRETER_H_
