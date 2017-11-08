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
  Prototype** cur_proto;        // current prototype
  Closure**   cur_cls;          // current closure
  Value*      cur_stk;          // current frame's start of stack
  std::uint32_t* cur_pc;        // current frame's start of PC

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

  Value* stack;
  std::uint32_t stack_size;
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
  cur_proto(NULL),
  cur_cls  (NULL),
  cur_stk  (NULL),
  cur_pc   (NULL),

  script   (NULL),
  global   (NULL),
  context  (NULL),
  ret      (),
  error    (NULL),

  stack    (init_stack),
  stack_size( init_stack_size ),
  call_size(0),

  max_stack_size(max_stack_size),
  max_call_size (max_call_size)
{}

struct RuntimeLayout {
  static const std::uint32_t kCurProtoOffset = offsetof(Runtime,cur_proto);
  static const std::uint32_t kCurClsOffset   = offsetof(Runtime,cur_cls);
  static const std::uint32_t kCurStackOffset = offsetof(Runtime,cur_stk);
  static const std::uint32_t kCurPCOffset    = offsetof(Runtime,cur_pc );
  static const std::uint32_t kRetOffset      = offsetof(Runtime,ret);
};

} // namespace interpreter
} // namespace lavascript

#endif // INTERPRETER_RUNTIME_H_
