#ifndef BYTECODE_ITERATOR_H_
#define BYTECODE_ITERATOR_H_
#include "bytecode.h"

#include <src/util.h>

/** ----------------------------------------------
 * Bytecode iterator. Used to decode bytecode in a
 * bytecode stream. This is mainly used for dumpping
 * and debugging purpose. The main interpreter will
 * not use this iterator but directly write assembly
 * to decode.
 * -----------------------------------------------*/

namespace lavascript {
namespace interpreter {

class BytecodeIterator {
 public:
  BytecodeIterator( const std::uint32_t* , std::size_t );
  BytecodeIterator();
  BytecodeIterator( const BytecodeIterator& );
  BytecodeIterator& operator = ( const BytecodeIterator& );

 public:
  bool HasNext() const { return cursor_ < size_; }
  inline bool Next();

  // Get the opcode from current cursor
  inline Bytecode opcode() const;
  inline const char* opcode_name() const;
  BytecodeType type() const { return type_; }

  inline void Decode( std::uint8_t* , std::uint8_t* , std::uint8_t* );
  inline void Decode( std::uint8_t* , std::uint8_t* );
  inline void Decode( std::uint8_t* );
  inline void Decode( std::uint16_t* );
  inline void Decode( std::uint16_t* , std::uint8_t* );
  inline void Decode( std::uint8_t* , std::uint16_t* );
 private:
  // Decode the stuff from current cursor's pointed position
  void Decode();

 private:
  const std::uint32_t* code_buffer_;
  std::size_t size_;
  std::size_t cursor_;
  std::size_t offset_;
  BytecodeType type_;
  Bytecode opcode_;
  union {
    std::uint8_t a1_8_;
    std::uint16_t a1_16_;
  };
  union {
    std::uint8_t a2_8_;
    std::uint16_t a2_16_;
  };
  std::uint8_t a3_8_;
};

inline Bytecode BytecodeIterator::opcode() const {
  lava_debug(NORMAL,lava_verify(HasNext()););
  return static_cast<Bytecode>(code_buffer_[cursor_] & 0xff);
}

} // namespace interpreter
} // namespace lavascript


#endif // BYTECODE_ITERATOR_H_
