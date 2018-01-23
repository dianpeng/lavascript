#include "bytecode-iterator.h"

namespace lavascript {
namespace interpreter {

void BytecodeIterator::GetOperandByIndex( int index , std::uint32_t* output ) {
  lava_debug(NORMAL,lava_verify(index >= 0 && index < 4););
  switch(type_) {
    case TYPE_B:
      lava_debug(NORMAL,lava_verify(index == 0 || index == 1););
      if(index == 0)
        *output = (a1_);
      else
        *output = (a2_);
      break;
    case TYPE_C:
      lava_debug(NORMAL,lava_verify(index == 0 || index == 1 ););
      if(index == 0)
        *output = (a1_);
      else
        *output = (a2_);
      break;
    case TYPE_D:
      lava_debug(NORMAL,lava_verify(index == 0 || index == 1 || index == 2););
      if(index == 0)
        *output = (a1_);
      else if(index == 1)
        *output = (a2_);
      else
        *output = (a3_);
      break;
    case TYPE_E:
      lava_debug(NORMAL,lava_verify(index == 0 || index == 1););
      if(index == 0)
        *output = (a1_);
      else
        *output = (a2_);
      break;
    case TYPE_F:
      lava_debug(NORMAL,lava_verify(index == 0););
      *output = (a1_);
      break;
    case TYPE_H:
      if(index == 0)
        *output = (a1_);
      else if(index == 1)
        *output = (a2_);
      else if(index == 2)
        *output = (a3_);
      else if(index == 3)
        *output = (a4_);
      else
        lava_die();

      break;
    default:
      lava_die();
      break;
  }
}

bool BytecodeIterator::SkipTo( const std::function<bool(BytecodeIterator*)>& predicate ) {
  for( ; HasNext() ; Move() ) {
    if(!predicate(this)) return true;
  }
  return false;
}

std::string GetBytecodeRepresentation( const std::uint32_t* address ) {
  Bytecode bc;
  BytecodeType bt;
  std::size_t offset;
  std::uint32_t a1,a2,a3,a4;
  DecodeBytecode(address,&bc,&bt,&a1,&a2,&a3,&a4,&offset);
  switch(bt) {
    case TYPE_B: case TYPE_C: case TYPE_E:
      return Format("%x. %s(%d,%d)/%d",GetBytecodeName(bc), a1, a2, offset);
    case TYPE_D:
      return Format("%x. %s(%d,%d,%d)/%d", GetBytecodeName(bc),a1,a2,offset);
    case TYPE_F:
      return Format("%x. %s(%d)/%d", GetBytecodeName(bc),a1,offset);
    default:
      lava_debug(NORMAL,lava_verify(bt == TYPE_H););
      return Format("%x. %s(%d,%d,%d,%d)/%d", GetBytecodeName(bc),a1,a2,a3,a4,offset);
  }
}

} // namespace interpreter
} // namespace lavascript
