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
    case TYPE_N:
      a1_8_ = static_cast<std::uint8_t>((raw>>8) & 0xff);
      a2_8_ = static_cast<std::uint8_t>((raw>>16)& 0xff);
      a3_8_ = static_cast<std::uint8_t>((raw>>24)& 0xff);
      offset_ = 1 + Align(a1_8_,static_cast<std::uint8_t>(4)) / 4;
      break;
    default:
      offset_ = 1;
      break;
  }
}

void BytecodeIterator::GetNArg( std::vector<std::uint8_t>* output ) {
  std::size_t pos = cursor_ + 1;
  std::size_t step= 0;
  std::size_t len = Align(a3_8_,static_cast<std::uint8_t>(4));

  for( std::size_t i = 0 ; i < len ; i += 4 , ++step ) {
    std::uint32_t cd = code_buffer_[pos+step];
    output->push_back( static_cast<std::uint8_t>(cd & 0xff) );
    output->push_back( static_cast<std::uint8_t>((cd >>8) & 0xff) );
    output->push_back( static_cast<std::uint8_t>((cd >>16) & 0xff) );
    output->push_back( static_cast<std::uint8_t>((cd >>24) & 0xff) );
  }

  std::size_t left = len - a3_8_;
  switch(left) {
    case 1: output->pop_back(); break;
    case 2: output->pop_back(); output->pop_back(); break;
    case 3: output->pop_back(); output->pop_back(); output->pop_back(); break;
    default: lava_debug(NORMAL,lava_verify(len == a3_8_);); break;
  }
  lava_debug(NORMAL,lava_verify(a3_8_ == output->size()););
}

} // namespace interpreter
} // namespace lavascript
