#ifndef BYTECODE_BUILDER_H_
#define BYTECODE_BUILDER_H_

#include "bytecode.h"

#include "src/source-code-info.h"
#include "src/util.h"
#include "src/trace.h"
#include "src/objects.h"
#include "src/interpreter/upvalue.h"
#include "src/interpreter/bytecode-iterator.h"

#include <vector>

namespace lavascript {
namespace interpreter {

/**
 * Bytecode builder.
 *
 * Bytecode builder wraps all the logic for building a bytecode sequences
 * with its needed constant table. After we building the bytecode sequences,
 * the result will be *moved* from the external heap into our managed heap.
 *
 * Currently the *move* is simply implemented as a copy
 */

class BytecodeBuilder {
 public:
  static const std::size_t kInitialCodeBufferSize = 1024;
  static const std::size_t kMaxCodeLength = 65536;
  static const std::size_t kMaxLiteralSize= 65536;
  static const std::size_t kMaxUpValueSize= 65536;
  static void DecodeUpValue( std::uint32_t code , std::uint16_t* index ,
                                                  UpValueState* state ) {
    *index = static_cast<std::uint16_t>(code & 0x0000ffff);
    *state = static_cast<UpValueState>( code >> 16 );
  }
 public:
  inline BytecodeBuilder();

  inline bool AddUpValue( UpValueState , std::uint16_t , std::uint16_t* );
  inline std::int32_t Add( std::int32_t );
  inline std::int32_t Add( double );
  std::int32_t Add( const ::lavascript::zone::String& , GC* );

  std::size_t int_table_size() const { return int_table_.size(); }
  std::size_t real_table_size() const { return real_table_.size(); }
  std::size_t string_table_size() const { return string_table_.size(); }
  std::size_t upvalue_size() const { return upvalue_slot_.size(); }
  std::size_t code_buffer_size() const { return code_buffer_.size(); }
  std::size_t debug_info_size() const { return debug_info_.size(); }

  std::uint16_t CodePosition() const {
    return static_cast<uint16_t>(code_buffer_.size());
  }

  const SourceCodeInfo& IndexSourceCodeInfo( std::size_t index ) const {
    return debug_info_[index];
  }

 public:
  // Use it as your own cautious, mainly for testing purpose
  BytecodeIterator GetIterator() const {
    return BytecodeIterator(AsBuffer(&code_buffer_,0),code_buffer_.size());
  }

 public:

  class Label {
   public:
    inline Label( BytecodeBuilder* , std::size_t , BytecodeType );
    inline Label();
    bool IsOk() const { return builder_ != NULL; }
    operator bool () const { return IsOk(); }
    inline void Patch( std::uint16_t );
   private:
    BytecodeType type_;
    std::size_t index_;
    BytecodeBuilder* builder_;
  };

  inline bool EmitB( const SourceCodeInfo& , Bytecode , std::uint8_t , std::uint16_t );
  inline bool EmitC( const SourceCodeInfo& , Bytecode , std::uint16_t, std::uint8_t  );
  inline bool EmitD( const SourceCodeInfo& , Bytecode , std::uint8_t , std::uint8_t , std::uint8_t );
  inline bool EmitE( const SourceCodeInfo& , Bytecode , std::uint8_t , std::uint8_t );
  inline bool EmitF( const SourceCodeInfo& , Bytecode , std::uint8_t );
  inline bool EmitG( const SourceCodeInfo& , Bytecode , std::uint16_t );
  inline bool EmitX( const SourceCodeInfo& , Bytecode );
  bool EmitN( const SourceCodeInfo& , Bytecode , std::uint8_t , std::uint8_t ,
                                                                std::uint8_t ,
                                                                const std::vector<std::uint8_t>& nargs );

  template< int BC , int TP , bool A1 = false , bool A2 = false , bool A3 = false >
  inline Label EmitAt( const SourceCodeInfo& , std::uint32_t a1 = 0 ,
                                               std::uint32_t a2 = 0 ,
                                               std::uint32_t a3 = 0 );

 public:
  /** ---------------------------------------------------
   * Bytecode emittion
   * ---------------------------------------------------*/

#define IMPLB(INSTR,C)                                                     \
  bool C(const SourceCodeInfo& si , std::uint8_t a1 , std::uint16_t a2 ) { \
    return EmitB(si,INSTR,a1,a2);                                          \
  }

#define IMPLC(INSTR,C)                                                     \
  bool C(const SourceCodeInfo& si , std::uint16_t a1, std::uint8_t a2 )  { \
    return EmitC(si,INSTR,a1,a2);                                          \
  }

#define IMPLD(INSTR,C)                                                 \
  bool C(const SourceCodeInfo& si , std::uint8_t a1, std::uint8_t a2,  \
      std::uint8_t a3 ) {                                              \
    return EmitD(si,INSTR,a1,a2,a3);                                   \
  }

#define IMPLE(INSTR,C)                                                    \
  bool C(const SourceCodeInfo& si , std::uint8_t a1 , std::uint8_t a2 ) { \
    return EmitE(si,INSTR,a1,a2);                                         \
  }

#define IMPLF(INSTR,C)                                                 \
  bool C(const SourceCodeInfo& si , std::uint8_t a1 ) {                \
    return EmitF(si,INSTR,a1);                                         \
  }

#define IMPLG(INSTR,C)                                                 \
  bool C(const SourceCodeInfo& si , std::uint16_t a1 ) {               \
    return EmitG(si,INSTR,a1);                                         \
  }

#define IMPLX(INSTR,C)                                                 \
  bool C(const SourceCodeInfo& si) {                                   \
    return EmitX(si,INSTR);                                            \
  }

#define IMPLN(INSTR,C)                                                                          \
  bool C(const SourceCodeInfo& si, std::uint8_t narg , std::uint8_t reg , std::uint8_t base ,   \
                                                       const std::vector<std::uint8_t>& vec ) { \
    return EmitN(si,INSTR,narg,reg,base,vec);                                                        \
  }

#define __(A,B,C,D,E,F) IMPL##A(BC_##B,C)
  LAVASCRIPT_BYTECODE_LIST(__)
#undef __           // __
#undef IMPLB        // IMPLB
#undef IMPLC        // IMPLC
#undef IMPLD        // IMPLD
#undef IMPLE        // IMPLE
#undef IMPLF        // IMPLF
#undef IMPLG        // IMPLG
#undef IMPLX        // IMPLX
#undef IMPLN        // IMPLN

 public:
  /* -----------------------------------------------------
   * Jump related isntruction                            |
   * ----------------------------------------------------*/
  inline Label jmpt   ( const SourceCodeInfo& si , const std::uint8_t r );
  inline Label jmpf   ( const SourceCodeInfo& si , const std::uint8_t r );
  inline Label and_   ( const SourceCodeInfo& si );
  inline Label or_    ( const SourceCodeInfo& si );
  inline Label jmp    ( const SourceCodeInfo& si );
  inline Label brk    ( const SourceCodeInfo& si );
  inline Label cont   ( const SourceCodeInfo& si );
  inline Label fstart ( const SourceCodeInfo& si , const std::uint8_t a1 );
  inline Label festart( const SourceCodeInfo& si , const std::uint8_t a1 );

 public:
  // This function will create a Closure object from the BytecodeBuilder
  // object. After that we convert a builder's internal information to a
  // managed heap based closure which can be used in the VM
  static Handle<Prototype> New( GC* , const BytecodeBuilder& bb ,
                                      const ::lavascript::parser::ast::Function& node );

  static Handle<Prototype> NewMain( GC* , const BytecodeBuilder& bb );

 private:
  static String** BuildFunctionPrototypeString( GC* ,
                                                const ::lavascript::parser::ast::Function& );
  static Handle<Prototype> New( GC* , const BytecodeBuilder& bb ,
                                      std::size_t arg_size,
                                      String** proto );

 private:
  std::vector<std::uint32_t> code_buffer_;           // Code buffer
  std::vector<SourceCodeInfo> debug_info_;           // Debug info
  std::vector<std::int32_t> int_table_;              // Integer table
  std::vector<double> real_table_;                   // Real table
  std::vector<Handle<String>> string_table_;                // String table

  struct UpValueSlot {
    std::uint16_t index;      // Can represent the register index or the slot
                              // index inside of the upvalue array slot
    UpValueState state;       // State of UpValue
    UpValueSlot( UpValueState st , std::uint16_t idx ):
      index(idx),
      state(st)
    {}

    std::uint32_t Encode() const {
      return ((static_cast<std::uint32_t>(state) << 16) |
               static_cast<std::uint32_t>(index));
    }
  };
  std::vector<UpValueSlot> upvalue_slot_;
};

/*--------------------------------------------
 * Inline Definitions                        |
 * ------------------------------------------*/
inline bool BytecodeBuilder::EmitB( const SourceCodeInfo& sci , Bytecode bc,
                                                                std::uint8_t  a1,
                                                                std::uint16_t a2) {
  if(code_buffer_.size() == kMaxCodeLength)
    return false;
  std::uint32_t value = static_cast<std::uint32_t>(bc) |
                        (static_cast<std::uint32_t>(a1) << 8) |
                        (static_cast<std::uint32_t>(a2) << 16);
  code_buffer_.push_back(value);
  debug_info_.push_back (sci);
  return true;
}

inline bool BytecodeBuilder::EmitC( const SourceCodeInfo& sci , Bytecode bc,
                                                                std::uint16_t a1 ,
                                                                std::uint8_t a2 ) {
  if(code_buffer_.size() == kMaxCodeLength)
    return false;
  std::uint32_t value = static_cast<std::uint32_t>(bc) |
                        (static_cast<std::uint32_t>(a1) << 8) |
                        (static_cast<std::uint32_t>(a2) <<24);
  code_buffer_.push_back(value);
  debug_info_.push_back(sci);
  return true;
}

inline bool BytecodeBuilder::EmitD( const SourceCodeInfo& sci , Bytecode bc,
                                                                std::uint8_t a1,
                                                                std::uint8_t a2,
                                                                std::uint8_t a3 ) {
  if(code_buffer_.size() == kMaxCodeLength)
    return false;

  std::uint32_t value = static_cast<std::uint32_t>(bc) |
                        (static_cast<std::uint32_t>(a1) << 8)  |
                        (static_cast<std::uint32_t>(a2) << 16) |
                        (static_cast<std::uint32_t>(a3) << 24);
  code_buffer_.push_back(value);
  debug_info_.push_back(sci);
  return true;
}

inline bool BytecodeBuilder::EmitE( const SourceCodeInfo& sci , Bytecode bc,
                                                                std::uint8_t a1,
                                                                std::uint8_t a2 ) {
  if(code_buffer_.size() == kMaxCodeLength)
    return false;
  std::uint32_t value = static_cast<std::uint32_t>(bc) |
                        (static_cast<std::uint32_t>(a1) << 8) |
                        (static_cast<std::uint32_t>(a2) << 16);
  code_buffer_.push_back(value);
  debug_info_.push_back(sci);
  return true;
}

inline bool BytecodeBuilder::EmitF( const SourceCodeInfo& sci , Bytecode bc,
                                                                std::uint8_t a1 ) {
  if(code_buffer_.size() == kMaxCodeLength)
    return false;
  std::uint32_t value = static_cast<std::uint32_t>(bc) |
                        (static_cast<std::uint32_t>(a1) << 8);
  code_buffer_.push_back(value);
  debug_info_.push_back(sci);
  return true;
}

inline bool BytecodeBuilder::EmitG( const SourceCodeInfo& sci , Bytecode bc,
                                                                std::uint16_t a1 ) {
  if(code_buffer_.size() == kMaxCodeLength)
    return false;
  std::uint32_t value = static_cast<std::uint32_t>(bc) |
                        (static_cast<std::uint32_t>(a1) << 8);
  code_buffer_.push_back(value);
  debug_info_.push_back(sci);
  return true;
}

inline bool BytecodeBuilder::EmitX( const SourceCodeInfo& sci , Bytecode bc ) {
  if(code_buffer_.size() == kMaxCodeLength)
    return false;

  code_buffer_.push_back(static_cast<std::uint32_t>(bc));
  debug_info_.push_back(sci);
  return true;
}

template< int BC , int TP , bool A1 , bool A2 , bool A3 >
inline BytecodeBuilder::Label BytecodeBuilder::EmitAt( const SourceCodeInfo& sci ,
                                                       std::uint32_t a1 ,
                                                       std::uint32_t a2 ,
                                                       std::uint32_t a3 ) {
  if( code_buffer_.size() == kMaxCodeLength )
    return BytecodeBuilder::Label();

  std::size_t idx = debug_info_.size();
  std::uint32_t encode = static_cast<std::uint32_t>(BC);

  switch(TP) {
    case TYPE_B:
      if(A1) encode |= (a1 << 8);
      if(A2) encode |= (a2 <<16);
      break;
    case TYPE_C:
      if(A1) encode |= (a1 << 8);
      if(A2) encode |= (a2 <<24);
      break;
    case TYPE_D:
      if(A1) encode |= (a1 << 8);
      if(A2) encode |= (a2 <<16);
      if(A3) encode |= (a3 <<24);
      break;
    case TYPE_E:
      if(A1) encode |= (a1 << 8);
      if(A2) encode |= (a2 <<16);
      break;
    case TYPE_F:
    case TYPE_G:
      if(A1) encode |= (a1 << 8);
      break;
    default:
      break;
  }

  code_buffer_.push_back(encode);
  debug_info_.push_back(sci);
  return Label(this,idx,static_cast<BytecodeType>(TP));
}

inline BytecodeBuilder::Label BytecodeBuilder::jmpt( const SourceCodeInfo& sci,
                                                     std::uint8_t a1 ) {
  return EmitAt<BC_JMPT,TYPE_B,true,false,false>(sci,a1);
}

inline BytecodeBuilder::Label BytecodeBuilder::jmpf( const SourceCodeInfo& sci,
                                                     std::uint8_t a1 ) {
  return EmitAt<BC_JMPF,TYPE_B,true,false,false>(sci,a1);
}

inline BytecodeBuilder::Label BytecodeBuilder::and_( const SourceCodeInfo& sci ) {
  return EmitAt<BC_AND,TYPE_G,false,false,false>(sci);
}

inline BytecodeBuilder::Label BytecodeBuilder::or_ ( const SourceCodeInfo& sci ) {
  return EmitAt<BC_OR,TYPE_G,false,false,false>(sci);
}

inline BytecodeBuilder::Label BytecodeBuilder::jmp ( const SourceCodeInfo& sci ) {
  return EmitAt<BC_JMP,TYPE_G,false,false,false>(sci);
}

inline BytecodeBuilder::Label BytecodeBuilder::brk ( const SourceCodeInfo& sci ) {
  return EmitAt<BC_BRK,TYPE_G,false,false,false>(sci);
}

inline BytecodeBuilder::Label BytecodeBuilder::cont( const SourceCodeInfo& sci ) {
  return EmitAt<BC_CONT,TYPE_G,false,false,false>(sci);
}

inline BytecodeBuilder::Label BytecodeBuilder::fstart( const SourceCodeInfo& sci ,
                                                       std::uint8_t a1 ) {
  return EmitAt<BC_FSTART,TYPE_B,true,false,false>(sci,a1);
}

inline BytecodeBuilder::Label BytecodeBuilder::festart( const SourceCodeInfo& sci ,
                                                        std::uint8_t a1 ) {
  return EmitAt<BC_FESTART,TYPE_B,true,false,false>(sci,a1);
}

inline bool BytecodeBuilder::AddUpValue( UpValueState state , std::uint16_t idx ,
                                                              std::uint16_t* output ) {
  lava_debug(NORMAL,
      if(state == UV_EMBED) {
        lava_verify(idx >=0 && idx <= 255);
      }
    );

  if(upvalue_slot_.size() == kMaxUpValueSize) {
    return false;
  }
  upvalue_slot_.push_back(UpValueSlot(state,idx));
  *output = static_cast<std::uint16_t>(upvalue_slot_.size()-1);
  return true;
}

inline std::int32_t BytecodeBuilder::Add( std::int32_t ival ) {
  auto ret = std::find(int_table_.begin(),int_table_.end(),ival);
  if(ret == int_table_.end()) {
    if(int_table_.size() == kMaxLiteralSize) {
      return -1;
    }
    int_table_.push_back(ival);
    return static_cast<std::int32_t>(int_table_.size()-1);
  }
  return (static_cast<std::int32_t>(
      std::distance(int_table_.begin(),ret)));
}

inline std::int32_t BytecodeBuilder::Add( double rval ) {
  auto ret = std::find(real_table_.begin(),real_table_.end(),rval);
  if(ret == real_table_.end()) {
    if(real_table_.size() == kMaxLiteralSize) {
      return -1;
    }
    real_table_.push_back(rval);
    return static_cast<std::int32_t>(real_table_.size()-1);
  }
  return (static_cast<std::int32_t>(
      std::distance(real_table_.begin(),ret)));
}

inline BytecodeBuilder::Label::Label():
  type_(),
  index_(),
  builder_(NULL)
{}

inline BytecodeBuilder::Label::Label( BytecodeBuilder* bb , std::size_t idx ,
                                                            BytecodeType bt ):
  type_   (bt),
  index_  (idx),
  builder_(bb)
{}

inline void BytecodeBuilder::Label::Patch( std::uint16_t pc ) {
  std::uint32_t v = builder_->code_buffer_[index_];
  switch(type_) {
    case TYPE_B:
      builder_->code_buffer_[index_] = (v | (static_cast<uint32_t>(pc) << 16));
      break;
    case TYPE_G:
      builder_->code_buffer_[index_] = (v | (static_cast<uint32_t>(pc) << 8));
      break;
    default:
      lava_unreach("not implemented bytecode type or broken type");
  }
}

inline BytecodeBuilder::BytecodeBuilder():
  code_buffer_(),
  debug_info_ (),
  int_table_  (),
  real_table_ (),
  string_table_(),
  upvalue_slot_()
{ code_buffer_.reserve(kInitialCodeBufferSize); }

} // namespace interpreter
} // namespace lavascript

#endif // BYTECODE_BUILDER_H_
