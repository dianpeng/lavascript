#include "bytecode.h"
#include "src/trace.h"

namespace lavascript {
namespace interpreter{

namespace {

#define __(T,NAME,PNAME,A1,A2,A3,A4,FB)     \
  BytecodeUsage{BytecodeUsage::A1,          \
                BytecodeUsage::A2,          \
                BytecodeUsage::A3,          \
                BytecodeUsage::A4,          \
                TYPE_##T,                   \
                FB},

BytecodeUsage kBytecodeUsage[] = {
  LAVASCRIPT_BYTECODE_LIST(__)

  BytecodeUsage{}
};

#undef __ // __

} // namespace

const char* GetBytecodeName( Bytecode bc ) {
  switch(bc) {

#define __(A,B,C,...) case BC_##B: return #C;
LAVASCRIPT_BYTECODE_LIST(__)
#undef __ // __

    default: lava_unreach(""); return NULL;
  }
}

const char* GetBytecodeTypeName( BytecodeType type ) {
  switch(type) {
    case TYPE_B: return "b";
    case TYPE_C: return "c";
    case TYPE_D: return "d";
    case TYPE_E: return "e";
    case TYPE_F: return "f";
    case TYPE_G: return "g";
    case TYPE_H: return "h";
    default: return "x";
  }
}

const BytecodeUsage& GetBytecodeUsage( Bytecode bc ) {
  return kBytecodeUsage[static_cast<int>(bc)];
}

} // namespace interpreter
} // namespace lavascript
