#ifndef X64_INTERPRETER_H_
#define X64_INTERPRETER_H_
#include "src/objects.h"

#include "intrinsic-call.h"
#include "interpreter.h"
#include "bytecode.h"

#include <memory>
#include <type_traits>

namespace lavascript {
class Context;

namespace interpreter{

class AssemblyInterpreter;
struct AssemblyInterpreterStubLayout;

// AssemblyInterpreterStub represents a interpreter function/routine that is generated
// on the fly. It should be just generated *once* and all the created context just
// use this one. It is a function so it doesn't contain *states*.
class AssemblyInterpreterStub {
 public:
  // Create an AssemblyInterpreterStub. After creation , the machine code for the
  // interpreter will be generated and also the dispatch table will be setup
  // correctly.
  //
  // Here we use a shared_ptr since we will use this assembly interpreter always
  // because we don't need to create a new assembly interpreter everytime. Sort
  // of like Singleton but just via a shared_ptr.
  static std::shared_ptr<AssemblyInterpreterStub> GetInstance();
 public:
  ~AssemblyInterpreterStub();

  // Dump the interpreter into human readable assembly into the DumpWriter
  void Dump( DumpWriter* ) const;

 private:
  Bytecode      CheckBytecodeRoutine ( void* pc ) const;
  int           CheckHelperRoutine   ( void* pc ) const;
  IntrinsicCall CheckIntrinsicCall   ( void* pc ) const;

  // Generate the dispatch interp
  bool GenerateDispatchInterp();

  // Generate the dispatch profile handler, *MUST* be called after GenerateDispatchInterp
  bool GenerateDispatchProfile();

  // Call it after instantiate the object
  bool Init();

 private:
  AssemblyInterpreterStub();

  // The following table are *not* modified and should not modified. The instance object
  // contains a mutable dispatch table instance and those are the tables that *should* be
  // handled into the main assembly routine for interpretation

  // dispatch table for each bytecode in interpretation mode
  void* dispatch_interp_[ SIZE_OF_BYTECODE ];

  // dispatch table for each bytecode in recording mode
  void* dispatch_profile_[ SIZE_OF_BYTECODE ];

  // dispatch table for each bytecode in jitting mode
  void* dispatch_jit_   [ SIZE_OF_BYTECODE ];

  // all builtin intrinsic call function's entrance address. these builtin functions are *not*
  // suitable for normal C++ call since they don't obey normal ABI but assume all the value are
  // on the stack
  void* ic_entry_       [ SIZE_OF_INTRINSIC_CALL ];

  // internal helper routine's entry
  std::vector<void*> interp_helper_;

  // main assembly interpreter routine. this entry may not be a valid C++ function
  // we can use inline assembly to jmp to this routine
  void* interp_entry_;

  struct CodeBuffer {
    void* entry;
    std::size_t code_size;
    std::size_t buffer_size;

    void Set( void* e , std::size_t cs , std::size_t bs ) {
      entry = e;
      code_size = cs;
      buffer_size= bs;
    }

    CodeBuffer():
      entry      (NULL),
      code_size  (0),
      buffer_size(0)
    {}

    void FreeIfNeeded();
  };

  // buffer to hold dispatch_interp_ handler
  CodeBuffer interp_code_buffer_;

  // buffer to hold dispatch_profile_ handler
  CodeBuffer profile_code_buffer_;

  friend struct AssemblyInterpreterStubLayout;
  friend class AssemblyInterpreter;

  LAVA_DISALLOW_COPY_AND_ASSIGN(AssemblyInterpreterStub)
};

static_assert( std::is_standard_layout<AssemblyInterpreterStub>::value );

struct AssemblyInterpreterStubLayout {
  static const std::uint32_t kDispatchInterpOffset = offsetof(AssemblyInterpreterStub,dispatch_interp_);
  static const std::uint32_t kDispatchRecordOffset = offsetof(AssemblyInterpreterStub,dispatch_profile_);
  static const std::uint32_t kDispatchJitOffset    = offsetof(AssemblyInterpreterStub,dispatch_jit_   );
  static const std::uint32_t kInterpEntryOffset    = offsetof(AssemblyInterpreterStub,interp_entry_   );
  static const std::uint32_t kIntrinsicEntry       = offsetof(AssemblyInterpreterStub,ic_entry_);
};

// Concret class implementation for Interpreter interface
class AssemblyInterpreter : public Interpreter {
 public:
  AssemblyInterpreter();

 public:
  virtual bool Run( Context* , const Handle<Script>& , const Handle<Object>& ,
                                                       Value*,
                                                       std::string* );

  virtual bool Run( Context* , const Handle<Closure>&, const Handle<Object>& ,
                                                       Value*,
                                                       std::string* ) {
    return false;
  }

  virtual ~AssemblyInterpreter() {}

 public:
  const void* dispatch_interp() const { return dispatch_interp_; }
  const void* dispatch_profile()const { return dispatch_profile_;}

 private:
  void* dispatch_interp_ [SIZE_OF_BYTECODE];
  void* dispatch_profile_[SIZE_OF_BYTECODE];
  void* dispatch_jit_    [SIZE_OF_BYTECODE];
  void**ic_entry_;
  void* interp_entry_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(AssemblyInterpreter)
};

} // namespace interpreter
} // namespace lavascript


#endif // X64_INTERPRETER_H_
