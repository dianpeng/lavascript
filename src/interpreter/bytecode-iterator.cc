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

} // namespace interpreter
} // namespace lavascript
