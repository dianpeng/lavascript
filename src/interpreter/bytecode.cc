#include "bytecode.h"
#include "src/trace.h"

namespace lavascript {
namespace interpreter{

const char* GetBytecodeName( Bytecode bc ) {
  switch(bc) {

#define __(A,B,C,D,E,F) case BC_##B: return #C;
LAVASCRIPT_BYTECODE_LIST(__)
#undef __ // __

    default: lava_unreach(""); return NULL;
  }
}

BytecodeType GetBytecodeType( Bytecode bc ) {
  switch(bc) {

#define __(A,B,C,D,E,F) case BC_##B: return TYPE_##A;
LAVASCRIPT_BYTECODE_LIST(__)
#undef __ // __

    default: lava_unreach(""); return TYPE_X;
  }
}

} // namespace interpreter
} // namespace lavascript
