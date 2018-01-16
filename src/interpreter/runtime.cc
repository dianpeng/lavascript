#include "runtime.h"
#include "src/context.h"

namespace lavascript {

LAVA_DEFINE_INT32(Interpreter,init_stack_size,"initial evaluations stack size for interpreter",40960);
LAVA_DEFINE_INT32(Interpreter,max_stack_size ,"maximum evaluation stack size for interpreter" ,1024*60);
LAVA_DEFINE_INT32(Interpreter,max_call_size  ,"maximum recursive call size for interpreter"   ,1024*20);

namespace interpreter {

Runtime::Runtime( Context* context , const Handle<Script>&  script  ,
                                     const Handle<Closure>& closure ,
                                     const Handle<Object>&  globals ,
                                     Interpreter*           inp,
                                     std::string*           e ):
  previous      (NULL),
  cur_cls       (closure.ref()),
  cur_stk       (context->gc()->interp_stack_start()),
  cur_pc        (NULL),

  script        (script.ref()),
  global        (globals.ref()),
  ret           (),
  error         (e),
  interp        (inp),
  context       (context),
  ic_entry      (NULL),

  stack_test    (context->gc()->interp_stack_test ()),
  call_size     (0),

  max_call_size (LAVA_OPTION(Interpreter,max_call_size)),

  cjob          (NULL),
  loop_hot_count(context->hotcount_data()->loop_hot_count),
  call_hot_count(context->hotcount_data()->call_hot_count),
  jit_enable    (true)
{
  // for this version of consturctor, the current context should not existed
  // or should be *NULL*
  lava_debug(NORMAL,lava_verify(context->runtime() == NULL););
  context->PushCurrentRuntime(this);
}

Runtime::Runtime( Context* context , const Handle<Closure>& cls ):
  previous      (NULL),
  cur_cls       (NULL),
  cur_stk       (NULL),
  cur_pc        (NULL),

  script        (NULL),
  global        (NULL),
  ret           (),
  error         (NULL),
  interp        (NULL),
  context       (context),
  ic_entry      (NULL),

  stack_test    (context->gc()->interp_stack_test ()),
  call_size     (0),

  max_call_size (LAVA_OPTION(Interpreter,max_call_size)),

  cjob          (NULL),
  loop_hot_count(NULL),
  call_hot_count(NULL),
  jit_enable    () {

  // for this version of consturctor, the current context should not existed
  // or should be *NULL*
  lava_debug(NORMAL,lava_verify(context->runtime()););

  auto prev = context->runtime();

  // current execution field
  previous = prev;
  cur_cls  = cls.ref();

  /**
   * NOTES: the cur_stk and cur_pc is left untouched since the
   *        interpreter should set it to correct value
   */

  // global shared field
  script   = prev->script;
  global   = prev->global;
  context  = prev->context;
  error    = prev->error;
  interp   = prev->interp;
  ic_entry = prev->ic_entry;

  // jit related
  cjob           = prev->cjob;
  loop_hot_count = prev->loop_hot_count;
  call_hot_count = prev->call_hot_count;
  jit_enable     = prev->jit_enable;

  context->PushCurrentRuntime(this);
}

Runtime::~Runtime() {
  context->PopCurrentRuntime();
}

} // namespace interpreter
} // namespace lavascript
