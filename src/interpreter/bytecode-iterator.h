#ifndef BYTECODE_ITERATOR_H_
#define BYTECODE_ITERATOR_H_
#include "bytecode.h"

#include "src/trace.h"
#include "src/util.h"

/** ---------------------------------------------------------------------
 *
 * Bytecode iterator. Used to decode bytecode in a
 * bytecode stream. This is mainly used for dumpping
 * and debugging purpose. The main interpreter will
 * not use this iterator but directly write assembly
 * to decode.
 *
 * ----------------------------------------------------------------------*/

namespace lavascript {
class DumpWriter;

namespace interpreter {

class BytecodeIterator {
 public:
  inline BytecodeIterator( const std::uint32_t* , std::size_t );

 public:
  bool HasNext() const { return cursor_ < size_; }
  inline bool Next();

  // Get the opcode from current cursor
  inline Bytecode opcode() const;
  inline const char* opcode_name() const;
  inline BytecodeType type() const;
  inline std::size_t offset() const { return offset_; }
  inline void GetOperand( std::uint8_t* , std::uint8_t* , std::uint8_t* , std::uint32_t* );
  inline void GetOperand( std::uint8_t* , std::uint8_t* , std::uint8_t* );
  inline void GetOperand( std::uint8_t* , std::uint8_t* );
  inline void GetOperand( std::uint8_t* );
  inline void GetOperand( std::uint16_t* );
  inline void GetOperand( std::uint16_t* , std::uint8_t* );
  inline void GetOperand( std::uint8_t* , std::uint16_t* );

  // Get current code position pointer
  const std::uint32_t* code_buffer() const { return code_buffer_; }
  const std::uint32_t* pc() const { return code_buffer_ + cursor_; }
  std::size_t cursor() const { return cursor_; }
  void BranchTo( const std::uint32_t offset ) {
    cursor_ = offset;
    if(HasNext()) Decode();
  }

 private:
  // Decode the stuff from current cursor's pointed position
  void Decode();

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
  std::uint32_t a4_;
};

/* ---------------------------------------------
 *
 * Inline Function Definitions
 *
 * -------------------------------------------*/

inline BytecodeIterator::BytecodeIterator( const std::uint32_t* code_buffer ,
                                           std::size_t size ):
  code_buffer_(code_buffer),
  size_       (size),
  cursor_     (0),
  offset_     (0),
  type_       (TYPE_X),
  opcode_     (),
  a3_8_       (),
  a4_         ()
{
  if(HasNext()) {
    Decode();
  }
}

inline bool BytecodeIterator::Next() {
  lava_debug(NORMAL,
      lava_verify( HasNext() );
      );
  cursor_ += offset_;
  if(HasNext()) {
    Decode();
  }
  return HasNext();
}

inline Bytecode BytecodeIterator::opcode() const {
  lava_debug(NORMAL,lava_verify(HasNext()););
  return opcode_;
}

inline const char* BytecodeIterator::opcode_name() const {
  return GetBytecodeName( opcode() );
}

inline BytecodeType BytecodeIterator::type() const {
  lava_debug(NORMAL,lava_verify(HasNext()););
  return type_;
}

inline void BytecodeIterator::GetOperand( std::uint8_t* a1 , std::uint8_t* a2 ,
                                                             std::uint8_t* a3 ) {
  lava_debug(NORMAL,
      lava_verify(HasNext());
      lava_verify(type_ == TYPE_D);
    );
  *a1 = a1_8_;
  *a2 = a2_8_;
  *a3 = a3_8_;
}

inline void BytecodeIterator::GetOperand( std::uint8_t* a1, std::uint8_t* a2,
                                                            std::uint8_t* a3,
                                                            std::uint32_t* a4 ) {
  lava_debug(NORMAL,
      lava_verify(HasNext());
      lava_verify(type_ == TYPE_H);
      );
  *a1 = a1_8_;
  *a2 = a2_8_;
  *a3 = a3_8_;
  *a4 = a4_;
}

inline void BytecodeIterator::GetOperand( std::uint8_t* a1 , std::uint8_t* a2 ) {
  lava_debug(NORMAL,
      lava_verify(HasNext());
      lava_verify(type_ == TYPE_E);
    );
  *a1 = a1_8_;
  *a2 = a2_8_;
}

inline void BytecodeIterator::GetOperand( std::uint8_t* a1 ) {
  lava_debug(NORMAL,
      lava_verify(HasNext());
      lava_verify(type_ == TYPE_F);
    );
  *a1 = a1_8_;
}

inline void BytecodeIterator::GetOperand( std::uint16_t* a1 ) {
  lava_debug(NORMAL,
      lava_verify(HasNext());
      lava_verify(type_ == TYPE_G);
    );
  *a1 = a1_16_;
}

inline void BytecodeIterator::GetOperand( std::uint16_t* a1 , std::uint8_t* a2 ) {
  lava_debug(NORMAL,
      lava_verify(HasNext());
      lava_verify(type_ == TYPE_C);
    );
  *a1 = a1_16_;
  *a2 = a2_8_;
}

inline void BytecodeIterator::GetOperand( std::uint8_t* a1 , std::uint16_t* a2 ) {
  lava_debug(NORMAL,
      lava_verify(HasNext());
      lava_verify(type_ == TYPE_B);
    );
  *a1 = a1_8_;
  *a2 = a2_16_;
}

} // namespace interpreter
} // namespace lavascript


#endif // BYTECODE_ITERATOR_H_
