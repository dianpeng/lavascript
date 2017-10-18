#include "src/interpreter/bytecode-interpreter.h"
#include "src/trace.h"

int main() {
  lavascript::InitTrace("-");
  std::shared_ptr<lavascript::interpreter::AssemblyInterpreter>
    interp( lavascript::interpreter::AssemblyInterpreter::Generate() );
  lava_verify(interp);
  {
    lavascript::DumpWriter writer(NULL);
    interp->Dump(&writer);
  }
  return 0;
}
