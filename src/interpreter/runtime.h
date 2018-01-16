#ifndef RUNTIME_H_
#define RUNTIME_H_

#include "src/config.h"
#include "src/objects.h"

#include "iframe.h"

namespace lavascript {

LAVA_DECLARE_INT32(Interpreter,init_stack_size);
LAVA_DECLARE_INT32(Interpreter,max_stack_size);
LAVA_DECLARE_INT32(Interpreter,max_call_size);

namespace interpreter{

class Interpreter;

// This serves as a global state holder object cross interpretation and JIT compliation.
// It is kind of mess but it is easy for us to hold these core data fields in a single
// place since we could just easily and efficiently pass this object's pointer around and
// mess with it.
struct Runtime {
  // pointer points to the previous runtime object on the stack
  Runtime*             previous;

  // ---------------------------------------------------------
  // current interpreted frame information
  // ---------------------------------------------------------
  Closure**            cur_cls;          // current closure, if not called by closure , then it is NULL
  Value*               cur_stk;          // current frame's start of stack
  const std::uint32_t* cur_pc ;          // current frame's start of PC

  Prototype* cur_proto() const { return (*cur_cls)->prototype().ptr(); }
  Handle<Prototype> cur_proto_handle() const { return (*cur_cls)->prototype(); }

  // get current interpreted frame object
  IFrame* cur_frame() const {
    return reinterpret_cast<IFrame*>(
      reinterpret_cast<char*>(cur_stk) - sizeof(IFrame));
  }

  // ---------------------------------------------------------
  // global interpretation information
  // ---------------------------------------------------------
  Script** script;
  Object** global;
  Value ret;
  std::string* error;
  Interpreter* interp;      // Which interpreter is used to interpreting it
  Context* const context;   // Immutable , binded while initialized
  void** ic_entry;          // Hold intrinsic call entry point, used only by assembly interpreter

  // ---------------------------------------------------------
  // interpretation information
  // ---------------------------------------------------------
  Value* stack_test ;       // If stack pointer is larger than this value, then it means
                            // we don't have 256 register slots , needs to grow
  std::uint32_t call_size ; // how many function call is on going

  // ---------------------------------------------------------
  // interpreter threshold/constraints
  // ---------------------------------------------------------
  std::uint32_t max_stack_size;
  std::uint32_t max_call_size;

  // ---------------------------------------------------------
  // JIT
  // ---------------------------------------------------------
  CompilationJob** cjob;    // This field will be set to a CompilerJob object
                            // if a JIT is pending in states *profile*. If profile
                            // is done, this field will be set to NULL again

  // This array will be used to store the *interpreter* time hot count recording.
  // There're 3+2 == 5 instructions will trigger a hot count recording, they are
  // 1) forend1
  // 2) forend2
  // 3) fevrend
  //
  // 4) call
  // 5) tcall
  //
  // When it is triggered, its current PC will be feeded into hash function to gen key to
  // index the hot count array , once the hot count array reaches 0 it means we need to
  // trigger the JIT.
  //
  // The first 3 BC will use loop_hot_count array and the rest 2 will use call_hot_count.
  //
  //
  // The hash is ((PC >> 2) & 0xff) , basically after shifting by 2 and then the least
  // significant 8 bits since the kHotCountArraySize is 256 and must be 256.
  compiler::hotcount_t* loop_hot_count;
  compiler::hotcount_t* call_hot_count;


  // Whether we enable JIT compilation or not. This is useful for debugging purpose
  bool jit_enable;

 public:
  Runtime( Context* , const Handle<Script>& , const Handle<Closure>& closure ,
                                              const Handle<Object>&  globals ,
                                              Interpreter*           interp  ,
                                              std::string*           error   );

  Runtime( Context* , const Handle<Closure>& closure );


  ~Runtime();
};

static_assert( std::is_standard_layout<Runtime>::value );

struct RuntimeLayout {
  static const std::uint32_t kCurClsOffset   = offsetof(Runtime,cur_cls);
  static const std::uint32_t kCurStackOffset = offsetof(Runtime,cur_stk);
  static const std::uint32_t kCurPCOffset    = offsetof(Runtime,cur_pc );

  static const std::uint32_t kScriptOffset   = offsetof(Runtime,script);
  static const std::uint32_t kGlobalOffset   = offsetof(Runtime,global);
  static const std::uint32_t kRetOffset      = offsetof(Runtime,ret);
  static const std::uint32_t kErrorOffset    = offsetof(Runtime,error);
  static const std::uint32_t kInterpOffset   = offsetof(Runtime,interp);
  static const std::uint32_t kContextOffset  = offsetof(Runtime,context);
  static const std::uint32_t kICEntryOffset  = offsetof(Runtime,ic_entry);

  static const std::uint32_t kStackTestOffset = offsetof(Runtime,stack_test);

  static const std::uint32_t kMaxStackSizeOffset = offsetof(Runtime,max_stack_size);
  static const std::uint32_t kMaxCallSizeOffset  = offsetof(Runtime,max_call_size);

  static const std::uint32_t kCompilerJobOffset  = offsetof(Runtime,cjob);
  static const std::uint32_t kLoopHotCountOffset = offsetof(Runtime,loop_hot_count);
  static const std::uint32_t kCallHotCountOffset = offsetof(Runtime,call_hot_count);
};

} // namespace interpreter
} // namespace lavascript

#endif // RUNTIME_H_
