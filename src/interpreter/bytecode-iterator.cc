#include "bytecode-iterator.h"

namespace lavascript {
namespace interpreter {

void BytecodeIterator::Decode() {
  std::uint32_t raw = code_buffer_[cursor_];
  opcode_ = static_cast<Bytecode>( raw & 0xff );
  type_ = GetBytecodeType(opcode_);
  switch(type_) {
    case TYPE_B:
      a1_8_ = static_cast<std::uint8_t>((raw & 0x0000ff00) >>8);
      a2_16_= static_cast<std::uint16_t>((raw >>16));
      offset_ = 1;
      break;
    case TYPE_C:
      a1_16_= static_cast<std::uint16_t>((raw & 0x00ffff00)>>8);
      a2_8_ = static_cast<std::uint8_t> ((raw >> 24));
      offset_ = 1;
      break;
    case TYPE_D:
      a1_8_ = static_cast<std::uint8_t> ((raw>>8));
      a2_8_ = static_cast<std::uint8_t> ((raw>>16));
      a3_8_ = static_cast<std::uint8_t> ((raw>>24));
      offset_ = 1;
      break;
    case TYPE_E:
      a1_8_ = static_cast<std::uint8_t> ((raw>>8));
      a2_8_ = static_cast<std::uint8_t> ((raw>>16));
      offset_ = 1;
      break;
    case TYPE_F:
      a1_8_ = static_cast<std::uint8_t> ((raw>>8));
      offset_ = 1;
      break;
    case TYPE_G:
      a1_16_= static_cast<std::uint16_t> ((raw>>8));
      offset_ = 1;
      break;
    case TYPE_H:
      a1_8_ = static_cast<std::uint8_t> ((raw>>8));
      a2_8_ = static_cast<std::uint8_t> ((raw>>16));
      a3_8_ = static_cast<std::uint8_t> ((raw>>24));
      a4_   = code_buffer_[cursor_+1];
      offset_ = 2;
      break;
    default:
      offset_ = 1;
      break;
  }
}

} // namespace interpreter
} // namespace lavascript
