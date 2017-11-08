#include "interpreter-runtime.h"

namespace lavascript {

LAVA_DEFINE_INT32(Interpreter,init_stack_size,"initial evaluations stack size for interpreter",1024);
LAVA_DEFINE_INT32(Interpreter,max_stack_size,"maximum evaluation stack size for interpreter",1024*60);
LAVA_DEFINE_INT32(Interpreter,max_call_size ,"maximum recursive call size for interpreter",1024*20);

} // namespace lavascript
