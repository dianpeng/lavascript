#ifndef INTERPRETER_RUNTIME_H_
#define INTERPRETER_RUNTIME_H_

#include "src/config.h"
#include "src/objects.h"

#include "interpreter-frame.h"

namespace lavascript {

LAVA_DECLARE_INT32(Interpreter,init_stack_size);
LAVA_DECLARE_INT32(Interpreter,max_stack_size);
LAVA_DECLARE_INT32(Interpreter,max_call_size);

namespace interpreter{

struct Runtime {
  // current interpreted frame information ----------------------
  Closure**            cur_cls;          // current closure
  Value*               cur_stk;          // current frame's start of stack
  const std::uint32_t* cur_pc;           // current frame's start of PC

  Prototype* cur_proto() const { return (*cur_cls)->prototype().ptr(); }
  Handle<Prototype> cur_proto_handle() const { return (*cur_cls)->prototype(); }

  // get current interpreted frame object
  IFrame* cur_frame() const {
    return reinterpret_cast<IFrame*>(
      reinterpret_cast<char*>(cur_stk) - sizeof(IFrame));
  }

  // global interpretation information --------------------------
  Script** script;
  Object** global;
  Context* context;
  Value ret;
  std::string* error;

  Value* stack_begin;       // Start of the stack
  Value* stack_end  ;       // End of the stack
  std::uint32_t stack_size() const { return stack_begin ? stack_end - stack_begin : 0; }
  std::uint32_t call_size ; // how many function call is on going

  // runtime threshold/constraints ------------------------------
  std::uint32_t max_stack_size;
  std::uint32_t max_call_size;

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

  stack_begin(init_stack),
  stack_end  (init_stack + init_stack_size),
  call_size(0),

  max_stack_size(max_stack_size),
  max_call_size (max_call_size)
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

  static const std::uint32_t kStackBeginOffset = offsetof(Runtime,stack_begin);
  static const std::uint32_t kStackEndOffset   = offsetof(Runtime,stack_end);

  static const std::uint32_t kMaxStackSizeOffset = offsetof(Runtime,max_stack_size);
  static const std::uint32_t kMaxCallSizeOffset  = offsetof(Runtime,max_call_size);
};

} // namespace interpreter
} // namespace lavascript

#endif // INTERPRETER_RUNTIME_H_
