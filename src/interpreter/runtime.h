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
  // ---------------------------------------------------------
  // current interpreted frame information
  // ---------------------------------------------------------
  Closure**            cur_cls;          // current closure, if not called by closure , then it is NULL
  Value*               cur_stk;          // current frame's start of stack
  const std::uint32_t* cur_pc;           // current frame's start of PC

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
  Context* context;
  Value ret;
  std::string* error;
  Interpreter* interp;      // Which interpreter is used to interpreting it

  Value* stack_begin;       // Start of the stack
  Value* stack_end  ;       // End of the stack
  std::uint32_t stack_size() const { return stack_begin ? stack_end - stack_begin : 0; }
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
  compiler::hotcount_t loop_hot_count[ compiler::kHotCountArraySize ];
  compiler::hotcount_t call_hot_count[ compiler::kHotCountArraySize ];


  // Whether we enable JIT compilation or not. This is useful for debugging purpose
  bool jit_enable;

 public:
  inline Runtime( Context* context , Value* init_stack ,
                                     std::uint32_t init_stack_size,
                                     std::uint32_t max_stack_size ,
                                     std::uint32_t max_call_size );
};

static_assert( std::is_standard_layout<Runtime>::value );

inline Runtime::Runtime( Context* context , Value* init_stack ,
                                            std::uint32_t init_stack_size ,
                                            std::uint32_t max_stack_size  ,
                                            std::uint32_t max_call_size ):
  cur_cls  (NULL),
  cur_stk  (NULL),
  cur_pc   (NULL),

  script   (NULL),
  global   (NULL),
  context  (NULL),
  ret      (),
  error    (NULL),
  interp   (NULL),

  stack_begin(init_stack),
  stack_end  (init_stack + init_stack_size),
  call_size(0),

  max_stack_size(max_stack_size),
  max_call_size (max_call_size),

  cjob          (NULL),
  loop_hot_count(),
  call_hot_count(),
  jit_enable    (true)
{}

struct RuntimeLayout {
  static const std::uint32_t kCurClsOffset   = offsetof(Runtime,cur_cls);
  static const std::uint32_t kCurStackOffset = offsetof(Runtime,cur_stk);
  static const std::uint32_t kCurPCOffset    = offsetof(Runtime,cur_pc );

  static const std::uint32_t kScriptOffset   = offsetof(Runtime,script);
  static const std::uint32_t kGlobalOffset   = offsetof(Runtime,global);
  static const std::uint32_t kContextOffset  = offsetof(Runtime,context);
  static const std::uint32_t kRetOffset      = offsetof(Runtime,ret);
  static const std::uint32_t kErrorOffset    = offsetof(Runtime,error);
  static const std::uint32_t kInterpOffset   = offsetof(Runtime,interp);

  static const std::uint32_t kStackBeginOffset = offsetof(Runtime,stack_begin);
  static const std::uint32_t kStackEndOffset   = offsetof(Runtime,stack_end);

  static const std::uint32_t kMaxStackSizeOffset = offsetof(Runtime,max_stack_size);
  static const std::uint32_t kMaxCallSizeOffset  = offsetof(Runtime,max_call_size);

  static const std::uint32_t kCompilerJobOffset  = offsetof(Runtime,cjob);
  static const std::uint32_t kLoopHotCountOffset = offsetof(Runtime,loop_hot_count);
  static const std::uint32_t kCallHotCountOffset = offsetof(Runtime,call_hot_count);
};

} // namespace interpreter
} // namespace lavascript

#endif // RUNTIME_H_
