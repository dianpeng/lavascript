#ifndef BYTECODE_ITERATOR_H_
#define BYTECODE_ITERATOR_H_
#include "bytecode.h"

#include "src/trace.h"
#include "src/util.h"
#include "src/tagged-ptr.h"

#include <functional>

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

// Decode a bytecode from the input address and forms a human readable
// string representation for this bytecode
std::string GetBytecodeRepresentation( const std::uint32_t* address );

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
  BytecodeLocation( const std::uint32_t* address , int type ): ptr_(address,type) {}
  BytecodeLocation(): ptr_(NULL,0) {}
 public:
  const std::uint32_t* address() const { return ptr_.ptr(); }
  bool IsOneByte() const { return ptr_.state() == ONE_BYTE; }
  bool IsTwoByte() const { return ptr_.state() == TWO_BYTE; }

 public:
  // Do a decoding for this *single* bytecode
  inline void     Decode( Bytecode* ,std::uint32_t* , std::uint32_t* , std::uint32_t* , std::uint32_t* ) const;
  inline Bytecode opcode() const;
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
  BytecodeLocation bytecode_location() const {
    return BytecodeLocation( pc(),offset() == 1 ? BytecodeLocation::ONE_BYTE :
                                                  BytecodeLocation::TWO_BYTE );
  }
  std::size_t offset() const { return offset_; }
  const BytecodeUsage& usage() const { return GetBytecodeUsage(opcode()); }
  // get operand genernally, the caller does type mappings impossible to fail.
  inline void FetchOperand( std::uint32_t*, std::uint32_t*, std::uint32_t*, std::uint32_t* );
 public:
  // get operand based on the type mappings, fail with assertion
  inline void GetOperand( std::uint8_t* , std::uint8_t* , std::uint8_t* , std::uint32_t* );
  inline void GetOperand( std::uint8_t* , std::uint8_t* , std::uint8_t* );
  inline void GetOperand( std::uint8_t* , std::uint8_t* );
  inline void GetOperand( std::uint8_t* );
  inline void GetOperand( std::uint16_t* );
  inline void GetOperand( std::uint16_t* , std::uint8_t* );
  inline void GetOperand( std::uint8_t* , std::uint16_t* );
  // get the operand based on the operand index
  void GetOperandByIndex( int index , std::uint32_t*);
  // Get current code position pointer
  const std::uint32_t* code_buffer() const { return code_buffer_; }
  const std::uint32_t* end() const { return code_buffer_ + cursor_; }
  const std::uint32_t* pc() const { return code_buffer_ + cursor_; }
  std::size_t cursor() const { return cursor_; }

  void BranchTo( const std::uint32_t offset ) {
    cursor_ = offset;
    if(HasNext()) Decode();
  }

  void BranchTo( const std::uint32_t* pc ) {
    std::size_t offset = (pc - code_buffer_);
    cursor_ = offset;
    if(HasNext()) Decode();
  }

  const std::uint32_t* OffsetAt( std::uint32_t offset ) {
    return code_buffer_ + offset;
  }

 public:
  // Skip to bytecode if the input predicate returns true, otherwise stop at that
  // point. If no further bytecode can be consumed and predicate haven't failed
  // once this funtion will return false ; otherwise return true
  bool SkipTo( const std::function<bool(BytecodeIterator*)>& );

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
// Do a decoding for this *single* bytecode
inline void BytecodeLocation::Decode( Bytecode* bc , std::uint32_t* a1 ,
    std::uint32_t* a2 ,
    std::uint32_t* a3 ,
    std::uint32_t* a4 ) const {
  BytecodeType type;
  std::size_t offset;
  DecodeBytecode(ptr_.ptr(),bc,&type,a1,a2,a3,a4,&offset);
  lava_debug(NORMAL,lava_verify(offset == (IsOneByte() ? 1:2)););
}

inline Bytecode BytecodeLocation::opcode() const {
  Bytecode bc;
  std::uint32_t a1,a2,a3,a4;
  Decode(&bc,&a1,&a2,&a3,&a4);
  return bc;
}

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
  if(HasNext()) Decode();
}

inline bool BytecodeIterator::Move() {
  lava_debug(NORMAL, lava_verify( HasNext() ););
  cursor_ += offset_;
  if(HasNext()) Decode();
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

inline void BytecodeIterator::FetchOperand( std::uint32_t* a1 , std::uint32_t* a2 ,
                                                                std::uint32_t* a3 ,
                                                                std::uint32_t* a4 ) {
  lava_debug(NORMAL,lava_verify(HasNext()););
  *a1 = a1_;
  *a2 = a2_;
  *a3 = a3_;
  *a4 = a4_;
}

inline void BytecodeIterator::GetOperand( std::uint8_t* a1 , std::uint8_t* a2 ,
                                                             std::uint8_t* a3 ) {
  lava_debug(NORMAL, lava_verify(HasNext()); lava_verify(type_ == TYPE_D););
  *a1 = static_cast<std::uint8_t>(a1_);
  *a2 = static_cast<std::uint8_t>(a2_);
  *a3 = static_cast<std::uint8_t>(a3_);
}

inline void BytecodeIterator::GetOperand( std::uint8_t* a1, std::uint8_t* a2,
                                                            std::uint8_t* a3,
                                                            std::uint32_t* a4 ) {
  lava_debug(NORMAL, lava_verify(HasNext()); lava_verify(type_ == TYPE_H););
  *a1 = static_cast<std::uint8_t>(a1_);
  *a2 = static_cast<std::uint8_t>(a2_);
  *a3 = static_cast<std::uint8_t>(a3_);
  *a4 = a4_;
}

inline void BytecodeIterator::GetOperand( std::uint8_t* a1 , std::uint8_t* a2 ) {
  lava_debug(NORMAL, lava_verify(HasNext()); lava_verify(type_ == TYPE_E););
  *a1 = static_cast<std::uint8_t>(a1_);
  *a2 = static_cast<std::uint8_t>(a2_);
}

inline void BytecodeIterator::GetOperand( std::uint8_t* a1 ) {
  lava_debug(NORMAL, lava_verify(HasNext()); lava_verify(type_ == TYPE_F););
  *a1 = static_cast<std::uint8_t>(a1_);
}

inline void BytecodeIterator::GetOperand( std::uint16_t* a1 ) {
  lava_debug(NORMAL, lava_verify(HasNext()); lava_verify(type_ == TYPE_G););
  *a1 = static_cast<std::uint16_t>(a1_);
}

inline void BytecodeIterator::GetOperand( std::uint16_t* a1 , std::uint8_t* a2 ) {
  lava_debug(NORMAL, lava_verify(HasNext()); lava_verify(type_ == TYPE_C););
  *a1 = static_cast<std::uint16_t>(a1_);
  *a2 = static_cast<std::uint8_t>(a2_);
}

inline void BytecodeIterator::GetOperand( std::uint8_t* a1 , std::uint16_t* a2 ) {
  lava_debug(NORMAL, lava_verify(HasNext()); lava_verify(type_ == TYPE_B););
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
      *a2 = static_cast<T2>((raw >>16) & 0xffff);
      *offset = 1;
      break;
    case TYPE_C:
      *a1 = static_cast<T1>((raw & 0x00ffff00)>>8);
      *a2 = static_cast<T2>((raw >> 24)& 0xff);
      *offset = 1;
      break;
    case TYPE_D:
      *a1 = static_cast<T1> ((raw>>8 ) & 0xff);
      *a2 = static_cast<T2> ((raw>>16) & 0xff);
      *a3 = static_cast<T3> ((raw>>24) & 0xff);
      *offset = 1;
      break;
    case TYPE_E:
      *a1 = static_cast<T1> ((raw>>8 ) & 0xff);
      *a2 = static_cast<T2> ((raw>>16) & 0xff);
      *offset = 1;
      break;
    case TYPE_F:
      *a1 = static_cast<T1> ((raw>>8 ) & 0xff);
      *offset = 1;
      break;
    case TYPE_G:
      *a1 = static_cast<T1> ((raw>>8 ) & 0xffff);
      *offset = 1;
      break;
    case TYPE_H:
      *a1 = static_cast<T1> ((raw>>8 ) & 0xff);
      *a2 = static_cast<T2> ((raw>>16) & 0xff);
      *a3 = static_cast<T3> ((raw>>24) & 0xff);
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
