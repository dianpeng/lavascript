#ifndef BYTECODE_ITERATOR_H_
#define BYTECODE_ITERATOR_H_
#include "bytecode.h"

#include "src/trace.h"
#include "src/util.h"
#include "src/tagged-ptr.h"

/** ---------------------------------------------------------------------
 *
 * Bytecode iterator. Used to decode bytecode inside of a bytecode stream.
 * The interpreter doesn't use it. It is mainly used in compiler's backend
 *
 * Currently it doesn't support reverse iteration due to the variable length
 * bytecode's nature. It may be extended to reversely iterate the bytecode
 * stream.
 *
 * ----------------------------------------------------------------------*/

namespace lavascript {
class DumpWriter;

namespace interpreter {

// General function to decode one bytecode from the input stream and gives out
// all information needed to step on
template< typename T1 , typename T2, typename T3, typename T4 >
void DecodeBytecode( const std::uint32_t* address ,
                     Bytecode* bc,
                     BytecodeType* type,
                     T1* a1,
                     T2* a2,
                     T3* a3,
                     T4* a4,
                     std::size_t* offset );


// Bytecode location encode a single bytecode's *address* within its
// bytecode stream and it also encodes how long this bytecode will be
// since we have bytecode can be encoded in 1 or 2 dwords. Internally
// it will use most efficient way to encode everything into a qword
// by using tagged pointer. It is used in the backend's compiler for
// storing each IR node's corresponding Bytecode information in a none
// decoded format. Later on we can do a real decoding on the fly to
// retrieve its information.
class BytecodeLocation {
 public:
  enum {
    ONE_BYTE = 0,
    TWO_BYTE
  };

  BytecodeLocation( const std::uint32_t* address , int type ):
    ptr_(address,type)
  {}

 public:
  const std::uint32_t* address() const { return ptr_.ptr(); }
  bool IsOneByte() const { return ptr_.state() == ONE_BYTE; }
  bool IsTwoByte() const { return ptr_.state() == TWO_BYTE; }

 public:
  // Do a decoding for this *single* bytecode
  void Decode( Bytecode* bc , std::uint32_t* a1 ,
                              std::uint32_t* a2 ,
                              std::uint32_t* a3 ,
                              std::uint32_t* a4 ) {
    BytecodeType type;
    std::size_t offset;
    DecodeBytecode(ptr_.ptr(),bc,&type,a1,a2,a3,a4,&offset);
    lava_debug(NORMAL,lava_verify(offset == (IsOneByte() ? 1:2)););
  }

 private:
  TaggedPtr<const std::uint32_t> ptr_;
};

static_assert( sizeof(BytecodeLocation) == sizeof(void*) );

class BytecodeIterator {
 public:
  // initialize the bytecode iterator using a bytecode stream
  inline BytecodeIterator( const std::uint32_t* , std::size_t );

 public:
  bool HasNext() const { return cursor_ < size_; }
  inline bool Move();

  // Get the opcode from current cursor
  inline Bytecode opcode() const;
  inline const char* opcode_name() const;
  inline BytecodeType type() const;
  inline const std::uint32_t* pc() const { return code_buffer_ + cursor_; }

  BytecodeLocation bytecode_location() const {
    return BytecodeLocation( pc(),offset() == 1 ? BytecodeLocation::ONE_BYTE :
                                                  BytecodeLocation::TWO_BYTE );
  }

  std::size_t offset() const { return offset_; }
  const BytecodeUsage& usage() const { return GetBytecodeUsage(opcode()); }

  inline void GetOperand( std::uint8_t* , std::uint8_t* , std::uint8_t* , std::uint32_t* );
  inline void GetOperand( std::uint8_t* , std::uint8_t* , std::uint8_t* );
  inline void GetOperand( std::uint8_t* , std::uint8_t* );
  inline void GetOperand( std::uint8_t* );
  inline void GetOperand( std::uint16_t* );
  inline void GetOperand( std::uint16_t* , std::uint8_t* );
  inline void GetOperand( std::uint8_t* , std::uint16_t* );

  void GetOperandByIndex( int index , std::uint32_t*);

 private:
  void Decode() {
    DecodeBytecode(code_buffer_+cursor_,&opcode_,&type_,&a1_,&a2_,&a3_,&a4_,&offset_);
  }

  const std::uint32_t* code_buffer_;
  std::size_t size_;
  std::size_t cursor_;
  std::size_t offset_;
  BytecodeType type_;
  Bytecode opcode_;

  std::uint32_t a1_;
  std::uint32_t a2_;
  std::uint32_t a3_;
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
  a1_         (),
  a2_         (),
  a3_         (),
  a4_         ()
{
  if(HasNext()) {
    Decode();
  }
}

inline bool BytecodeIterator::Move() {
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
  *a1 = static_cast<std::uint8_t>(a1_);
  *a2 = static_cast<std::uint8_t>(a2_);
  *a3 = static_cast<std::uint8_t>(a3_);
}

inline void BytecodeIterator::GetOperand( std::uint8_t* a1, std::uint8_t* a2,
                                                            std::uint8_t* a3,
                                                            std::uint32_t* a4 ) {
  lava_debug(NORMAL,
      lava_verify(HasNext());
      lava_verify(type_ == TYPE_H);
    );
  *a1 = static_cast<std::uint8_t>(a1_);
  *a2 = static_cast<std::uint8_t>(a2_);
  *a3 = static_cast<std::uint8_t>(a3_);
  *a4 = a4_;
}

inline void BytecodeIterator::GetOperand( std::uint8_t* a1 , std::uint8_t* a2 ) {
  lava_debug(NORMAL,
      lava_verify(HasNext());
      lava_verify(type_ == TYPE_E);
    );
  *a1 = static_cast<std::uint8_t>(a1_);
  *a2 = static_cast<std::uint8_t>(a2_);
}

inline void BytecodeIterator::GetOperand( std::uint8_t* a1 ) {
  lava_debug(NORMAL,
      lava_verify(HasNext());
      lava_verify(type_ == TYPE_F);
    );
  *a1 = static_cast<std::uint8_t>(a1_);
}

inline void BytecodeIterator::GetOperand( std::uint16_t* a1 ) {
  lava_debug(NORMAL,
      lava_verify(HasNext());
      lava_verify(type_ == TYPE_G);
    );
  *a1 = static_cast<std::uint16_t>(a1_);
}

inline void BytecodeIterator::GetOperand( std::uint16_t* a1 , std::uint8_t* a2 ) {
  lava_debug(NORMAL,
      lava_verify(HasNext());
      lava_verify(type_ == TYPE_C);
    );
  *a1 = static_cast<std::uint16_t>(a1_);
  *a2 = static_cast<std::uint8_t>(a2_);
}

inline void BytecodeIterator::GetOperand( std::uint8_t* a1 , std::uint16_t* a2 ) {
  lava_debug(NORMAL,
      lava_verify(HasNext());
      lava_verify(type_ == TYPE_B);
    );
  *a1 = static_cast<std::uint8_t>(a1_);
  *a2 = static_cast<std::uint16_t>(a2_);
}

template< typename T1 , typename T2, typename T3, typename T4 >
void DecodeBytecode( const std::uint32_t* address , Bytecode* bc, BytecodeType* type,
                                                                  T1* a1,
                                                                  T2* a2,
                                                                  T3* a3,
                                                                  T4* a4,
                                                                  std::size_t* offset ) {

  std::uint32_t raw = *address;

  *bc = static_cast<Bytecode>( raw & 0xff );
  *type = GetBytecodeType(*bc);

  switch(*type) {
    case TYPE_B:
      *a1 = static_cast<T1>((raw & 0x0000ff00) >>8);
      *a2 = static_cast<T2>((raw >>16));
      *offset = 1;
      break;
    case TYPE_C:
      *a1 = static_cast<T1>((raw & 0x00ffff00)>>8);
      *a2 = static_cast<T2>((raw >> 24));
      *offset = 1;
      break;
    case TYPE_D:
      *a1 = static_cast<T1> ((raw>>8));
      *a2 = static_cast<T2> ((raw>>16));
      *a3 = static_cast<T3> ((raw>>24));
      *offset = 1;
      break;
    case TYPE_E:
      *a1 = static_cast<T1> ((raw>>8));
      *a2 = static_cast<T2> ((raw>>16));
      *offset = 1;
      break;
    case TYPE_F:
      *a1 = static_cast<T1> ((raw>>8));
      *offset = 1;
      break;
    case TYPE_G:
      *a1 = static_cast<T1> ((raw>>8));
      *offset = 1;
      break;
    case TYPE_H:
      *a1 = static_cast<T1> ((raw>>8));
      *a2 = static_cast<T2> ((raw>>16));
      *a3 = static_cast<T3> ((raw>>24));
      *a4 = address[1];
      *offset = 2;
      break;
    default:
      *offset = 1;
      break;
  }
}

} // namespace interpreter
} // namespace lavascript


#endif // BYTECODE_ITERATOR_H_
