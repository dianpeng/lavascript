#include "src/interpreter/bytecode-interpreter.h"
#include "src/trace.h"

int main() {
  lavascript::InitTrace("-");
  lavascript::interpreter::Interpreter interp;
  lavascript::interpreter::GenerateInterpreter(&interp);
  return 0;
}
