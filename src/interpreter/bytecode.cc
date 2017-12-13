#include "bytecode.h"
#include "src/trace.h"

namespace lavascript {
namespace interpreter{

const char* GetBytecodeName( Bytecode bc ) {
  switch(bc) {

#define __(A,B,C,...) case BC_##B: return #C;
LAVASCRIPT_BYTECODE_LIST(__)
#undef __ // __

    default: lava_unreach(""); return NULL;
  }
}

BytecodeType GetBytecodeType( Bytecode bc ) {
  switch(bc) {

#define __(A,B,...) case BC_##B: return TYPE_##A;
LAVASCRIPT_BYTECODE_LIST(__)
#undef __ // __

    default: lava_unreach(""); return TYPE_X;
  }
}

bool DoesBytecodeHasFeedback( Bytecode bc ) {
#define RESULT_FB true
#define RESULT__  false

  switch(bc) {
#define __(A,B,C,D,E,F,G) case BC_##B: return RESULT_##G;
  LAVASCRIPT_BYTECODE_LIST(__)
#undef __
    default: return false;
  }

#undef RESULT_TB // RESULT_TB
#undef RESULT__  // RESULT__
}


} // namespace interpreter
} // namespace lavascript
