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
  static void DecodeUpValue( std::uint32_t code , std::uint8_t* index ,
                                                  UpValueState* state ) {
    *index = static_cast<std::uint8_t>(code & 0x0000ffff);
    *state = static_cast<UpValueState>( code >> 16 );
  }
 public:
  inline BytecodeBuilder();

  inline bool AddUpValue( UpValueState , std::uint16_t , std::uint16_t* );
  inline std::int32_t Add( double );
  std::int32_t Add( const ::lavascript::zone::String& , GC* );

  // Add a string into BytecodeBuilder as *SSO* table, this *doesn't* add
  // the string into normal string table. Assume user have already test
  // this string can be categorized as SSO
  std::int32_t AddSSO( const ::lavascript::zone::String& , GC* );

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
    inline void PatchDWord( std::uint32_t );
   private:
    BytecodeType type_;
    std::size_t index_;
    BytecodeBuilder* builder_;
  };

  inline bool EmitB( std::uint8_t , const SourceCodeInfo& , Bytecode , std::uint8_t , std::uint16_t );
  inline bool EmitC( std::uint8_t , const SourceCodeInfo& , Bytecode , std::uint16_t, std::uint8_t  );
  inline bool EmitD( std::uint8_t , const SourceCodeInfo& , Bytecode , std::uint8_t , std::uint8_t ,
                                                                                      std::uint8_t );
  inline bool EmitE( std::uint8_t , const SourceCodeInfo& , Bytecode , std::uint8_t , std::uint8_t );
  inline bool EmitF( std::uint8_t , const SourceCodeInfo& , Bytecode , std::uint8_t );
  inline bool EmitG( std::uint8_t , const SourceCodeInfo& , Bytecode , std::uint16_t );

  inline bool EmitH( std::uint8_t , const SourceCodeInfo& , Bytecode , std::uint8_t ,
                                                                       std::uint8_t ,
                                                                       std::uint8_t ,
                                                                       std::uint32_t );
  inline bool EmitX( std::uint8_t , const SourceCodeInfo& , Bytecode );

  template< int BC , int TP , bool A1 = false , bool A2 = false , bool A3 = false >
  inline Label EmitAt( std::uint8_t , const SourceCodeInfo& , std::uint32_t a1 = 0 ,
                                                              std::uint32_t a2 = 0 ,
                                                              std::uint32_t a3 = 0 );

 public:
  /** ---------------------------------------------------
   * Bytecode emittion
   * ---------------------------------------------------*/

#define IMPLB(INSTR,C)                                                     \
  bool C(std::uint8_t reg,const SourceCodeInfo& si , std::uint8_t a1 ,     \
                                                     std::uint16_t a2 ) {  \
    return EmitB(reg,si,INSTR,a1,a2);                                      \
  }

#define IMPLC(INSTR,C)                                                     \
  bool C(std::uint8_t reg,const SourceCodeInfo& si , std::uint16_t a1,     \
                                                     std::uint8_t a2 )  {  \
    return EmitC(reg,si,INSTR,a1,a2);                                      \
  }

#define IMPLD(INSTR,C)                                                   \
  bool C(std::uint8_t reg,const SourceCodeInfo& si , std::uint8_t a1,    \
                                                     std::uint8_t a2,    \
                                                     std::uint8_t a3 ) { \
    return EmitD(reg,si,INSTR,a1,a2,a3);                                 \
  }

#define IMPLE(INSTR,C)                                                    \
  bool C(std::uint8_t reg,const SourceCodeInfo& si , std::uint8_t a1 ,    \
                                                     std::uint8_t a2 ) {  \
    return EmitE(reg,si,INSTR,a1,a2);                                     \
  }

#define IMPLF(INSTR,C)                                                   \
  bool C(std::uint8_t reg,const SourceCodeInfo& si , std::uint8_t a1 ) { \
    return EmitF(reg,si,INSTR,a1);                                       \
  }

#define IMPLG(INSTR,C)                                                    \
  bool C(std::uint8_t reg,const SourceCodeInfo& si , std::uint16_t a1 ) { \
    return EmitG(reg,si,INSTR,a1);                                        \
  }

#define IMPLX(INSTR,C)                                                 \
  bool C(std::uint8_t reg,const SourceCodeInfo& si) {                  \
    return EmitX(reg,si,INSTR);                                        \
  }

#define IMPLH(INSTR,C) /* null body */


#define __(A,B,C,...) IMPL##A(BC_##B,C)
  LAVASCRIPT_BYTECODE_LIST(__)
#undef __           // __
#undef IMPLB        // IMPLB
#undef IMPLC        // IMPLC
#undef IMPLD        // IMPLD
#undef IMPLE        // IMPLE
#undef IMPLF        // IMPLF
#undef IMPLG        // IMPLG
#undef IMPLX        // IMPLX
#undef IMPLH        // IMPLH

 public:
  /* -----------------------------------------------------
   * TypeH instructions                                  |
   * ----------------------------------------------------*/
  inline bool fend1( std::uint8_t reg , const SourceCodeInfo& si , std::uint8_t a1,
                                                                   std::uint8_t a2,
                                                                   std::uint8_t a3,
                                                                   std::uint16_t a4 );

  inline bool fend2( std::uint8_t reg , const SourceCodeInfo& si , std::uint8_t a1,
                                                                   std::uint8_t a2,
                                                                   std::uint8_t a3,
                                                                   std::uint16_t a4 );

 public:
  /* -----------------------------------------------------
   * Jump related isntruction                            |
   * ----------------------------------------------------*/
  inline Label jmpt   ( std::uint8_t reg , const SourceCodeInfo& si , std::uint8_t r );
  inline Label jmpf   ( std::uint8_t reg , const SourceCodeInfo& si , std::uint8_t r );

  inline Label and_   ( std::uint8_t reg , const SourceCodeInfo& si , std::uint8_t,
                                                                      std::uint8_t   );
  inline Label or_    ( std::uint8_t reg , const SourceCodeInfo& si , std::uint8_t,
                                                                      std::uint8_t   );
  inline Label tern   ( std::uint8_t reg , const SourceCodeInfo& si , std::uint8_t,
                                                                      std::uint8_t   );
  inline Label jmp    ( std::uint8_t reg , const SourceCodeInfo& si );
  inline Label brk    ( std::uint8_t reg , const SourceCodeInfo& si );
  inline Label cont   ( std::uint8_t reg , const SourceCodeInfo& si );
  inline Label fstart ( std::uint8_t reg , const SourceCodeInfo& si , std::uint8_t a1 );
  inline Label festart( std::uint8_t reg , const SourceCodeInfo& si , std::uint8_t a1 );

  // NOTES:
  // hack around the inline issue, this type of code needs to be generated to return
  // a *label* however the above macro generate it with normal type X instruction code
  // stub which prevents us from override it. So we add an underscore to work around it.
  inline Label fevrstart_( std::uint8_t reg , const SourceCodeInfo& si );

 public:
  // This function will create a Closure object from the BytecodeBuilder
  // object. After that we convert a builder's internal information to a
  // managed heap based closure which can be used in the VM
  static Handle<Prototype> New( GC* , const BytecodeBuilder& bb ,
                                      const ::lavascript::parser::ast::Function& node );

  static Handle<Prototype> NewMain( GC* , const BytecodeBuilder& bb ,
                                          std::size_t max_local_var_size );

 private:
  static String** BuildFunctionPrototypeString( GC* ,
                                                const ::lavascript::parser::ast::Function& );

  static Handle<Prototype> New( GC* , const BytecodeBuilder& bb , std::size_t arg_size,
                                                                  std::size_t max_local_var_size,
                                                                  String** proto );

 private:
  std::vector<std::uint32_t> code_buffer_;           // Code buffer
  std::vector<SourceCodeInfo> debug_info_;           // Debug info
  std::vector<double> real_table_;                   // Real table
  std::vector<Handle<String>> string_table_;         // String table
  std::vector<Prototype::SSOTableEntry> sso_table_;  // SSO table

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

  std::vector<std::uint8_t> reg_offset_table_;       // Register offset table
};

/*--------------------------------------------
 * Inline Definitions                        |
 * ------------------------------------------*/
inline bool BytecodeBuilder::EmitB( std::uint8_t reg , const SourceCodeInfo& sci ,
                                                       Bytecode bc,
                                                       std::uint8_t  a1,
                                                       std::uint16_t a2) {
  if(code_buffer_.size() == kMaxCodeLength)
    return false;
  std::uint32_t value = static_cast<std::uint32_t>(bc) |
                        (static_cast<std::uint32_t>(a1) << 8) |
                        (static_cast<std::uint32_t>(a2) << 16);
  code_buffer_.push_back(value);
  debug_info_.push_back (sci);
  reg_offset_table_.push_back(reg);
  return true;
}

inline bool BytecodeBuilder::EmitC( std::uint8_t reg , const SourceCodeInfo& sci ,
                                                       Bytecode bc,
                                                       std::uint16_t a1 ,
                                                       std::uint8_t a2 ) {
  if(code_buffer_.size() == kMaxCodeLength)
    return false;
  std::uint32_t value = static_cast<std::uint32_t>(bc) |
                        (static_cast<std::uint32_t>(a1) << 8) |
                        (static_cast<std::uint32_t>(a2) <<24);
  code_buffer_.push_back(value);
  debug_info_.push_back(sci);
  reg_offset_table_.push_back(reg);
  return true;
}

inline bool BytecodeBuilder::EmitD( std::uint8_t reg , const SourceCodeInfo& sci ,
                                                       Bytecode bc,
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
  reg_offset_table_.push_back(reg);
  return true;
}

inline bool BytecodeBuilder::EmitE( std::uint8_t reg , const SourceCodeInfo& sci ,
                                                       Bytecode bc,
                                                       std::uint8_t a1,
                                                       std::uint8_t a2 ) {
  if(code_buffer_.size() == kMaxCodeLength)
    return false;
  std::uint32_t value = static_cast<std::uint32_t>(bc) |
                        (static_cast<std::uint32_t>(a1) << 8) |
                        (static_cast<std::uint32_t>(a2) << 16);
  code_buffer_.push_back(value);
  debug_info_.push_back(sci);
  reg_offset_table_.push_back(reg);
  return true;
}

inline bool BytecodeBuilder::EmitF( std::uint8_t reg , const SourceCodeInfo& sci ,
                                                       Bytecode bc,
                                                       std::uint8_t a1 ) {
  if(code_buffer_.size() == kMaxCodeLength)
    return false;
  std::uint32_t value = static_cast<std::uint32_t>(bc) |
                        (static_cast<std::uint32_t>(a1) << 8);
  code_buffer_.push_back(value);
  debug_info_.push_back(sci);
  reg_offset_table_.push_back(reg);
  return true;
}

inline bool BytecodeBuilder::EmitG( std::uint8_t reg , const SourceCodeInfo& sci ,
                                                       Bytecode bc,
                                                       std::uint16_t a1 ) {
  if(code_buffer_.size() == kMaxCodeLength)
    return false;
  std::uint32_t value = static_cast<std::uint32_t>(bc) |
                        (static_cast<std::uint32_t>(a1) << 8);
  code_buffer_.push_back(value);
  debug_info_.push_back(sci);
  reg_offset_table_.push_back(reg);
  return true;
}

inline bool BytecodeBuilder::EmitX( std::uint8_t reg , const SourceCodeInfo& sci ,
                                                       Bytecode bc ) {
  if(code_buffer_.size() == kMaxCodeLength)
    return false;

  code_buffer_.push_back(static_cast<std::uint32_t>(bc));
  debug_info_.push_back(sci);
  reg_offset_table_.push_back(reg);
  return true;
}

inline bool BytecodeBuilder::EmitH( std::uint8_t reg , const SourceCodeInfo& sci ,
                                                       Bytecode bc,
                                                       std::uint8_t a1,
                                                       std::uint8_t a2,
                                                       std::uint8_t a3,
                                                       std::uint32_t a4 ) {
  if(code_buffer_.size() + 2 > kMaxCodeLength)
    return false;

  std::uint32_t value = static_cast<std::uint32_t>(bc) |
                        (static_cast<std::uint32_t>(a1) << 8)  |
                        (static_cast<std::uint32_t>(a2) << 16) |
                        (static_cast<std::uint32_t>(a3) << 24);
  code_buffer_.push_back(value);
  code_buffer_.push_back(a4);

  // This is a 2 dword instruction
  for( std::size_t i = 0 ; i < 2 ; ++i ) {
    debug_info_.push_back(sci);
    reg_offset_table_.push_back(reg);
  }

  return true;
}

template< int BC , int TP , bool A1 , bool A2 , bool A3 >
inline BytecodeBuilder::Label BytecodeBuilder::EmitAt( std::uint8_t reg ,
                                                       const SourceCodeInfo& sci ,
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
    case TYPE_H:
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
  reg_offset_table_.push_back(reg);

  /** handle type_h since it has an extra slot **/
  if(TP == TYPE_H) {
    code_buffer_.push_back(0);
    debug_info_.push_back(sci);
    reg_offset_table_.push_back(reg);
  }

  return Label(this,idx,static_cast<BytecodeType>(TP));
}

inline bool BytecodeBuilder::fend1(std::uint8_t reg ,
                                   const SourceCodeInfo& sci ,
                                   std::uint8_t a1,
                                   std::uint8_t a2,
                                   std::uint8_t a3,
                                   std::uint16_t a4 ) {
  return EmitH(reg,sci,BC_FEND1,a1,a2,a3,a4);
}

inline bool BytecodeBuilder::fend2(std::uint8_t reg ,
                                   const SourceCodeInfo& sci ,
                                   std::uint8_t a1,
                                   std::uint8_t a2,
                                   std::uint8_t a3,
                                   std::uint16_t a4 ) {
  return EmitH(reg,sci,BC_FEND2,a1,a2,a3,a4);
}

inline BytecodeBuilder::Label BytecodeBuilder::jmpt( std::uint8_t reg ,
                                                     const SourceCodeInfo& sci,
                                                     std::uint8_t a1 ) {
  return EmitAt<BC_JMPT,TYPE_B,true,false,false>(reg,sci,a1);
}

inline BytecodeBuilder::Label BytecodeBuilder::jmpf( std::uint8_t reg ,
                                                     const SourceCodeInfo& sci,
                                                     std::uint8_t a1 ) {
  return EmitAt<BC_JMPF,TYPE_B,true,false,false>(reg,sci,a1);
}

inline BytecodeBuilder::Label BytecodeBuilder::and_( std::uint8_t reg ,
                                                     const SourceCodeInfo& sci ,
                                                     std::uint8_t a1 ,
                                                     std::uint8_t a2 ) {
  return EmitAt<BC_AND,TYPE_H,true,true,false>(reg,sci,a1,a2);
}

inline BytecodeBuilder::Label BytecodeBuilder::or_ ( std::uint8_t reg ,
                                                     const SourceCodeInfo& sci ,
                                                     std::uint8_t a1 ,
                                                     std::uint8_t a2 ) {
  return EmitAt<BC_OR,TYPE_H,true,true,false>(reg,sci,a1,a2);
}

inline BytecodeBuilder::Label BytecodeBuilder::tern( std::uint8_t reg ,
                                                     const SourceCodeInfo& sci ,
                                                     std::uint8_t a1 ,
                                                     std::uint8_t a2 ) {
  return EmitAt<BC_TERN,TYPE_H,true,true,false>(reg,sci,a1,a2);
}

inline BytecodeBuilder::Label BytecodeBuilder::jmp ( std::uint8_t reg ,
                                                     const SourceCodeInfo& sci ) {
  return EmitAt<BC_JMP,TYPE_G,false,false,false>(reg,sci);
}

inline BytecodeBuilder::Label BytecodeBuilder::brk ( std::uint8_t reg,
                                                     const SourceCodeInfo& sci ) {
  return EmitAt<BC_BRK,TYPE_G,false,false,false>(reg,sci);
}

inline BytecodeBuilder::Label BytecodeBuilder::cont( std::uint8_t reg,
                                                     const SourceCodeInfo& sci ) {
  return EmitAt<BC_CONT,TYPE_G,false,false,false>(reg,sci);
}

inline BytecodeBuilder::Label BytecodeBuilder::fstart( std::uint8_t reg,
                                                       const SourceCodeInfo& sci ,
                                                       std::uint8_t a1 ) {
  return EmitAt<BC_FSTART,TYPE_B,true,false,false>(reg,sci,a1);
}

inline BytecodeBuilder::Label BytecodeBuilder::festart( std::uint8_t reg,
                                                        const SourceCodeInfo& sci ,
                                                        std::uint8_t a1 ) {
  return EmitAt<BC_FESTART,TYPE_B,true,false,false>(reg,sci,a1);
}

inline BytecodeBuilder::Label BytecodeBuilder::fevrstart_( std::uint8_t reg ,
                                                           const SourceCodeInfo& sci ) {
  return EmitAt<BC_FEVRSTART,TYPE_G,false,false,false>(reg,sci);
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
      builder_->code_buffer_[index_] = (v | (static_cast<std::uint32_t>(pc) << 16));
      break;
    case TYPE_G:
      builder_->code_buffer_[index_] = (v | (static_cast<std::uint32_t>(pc) << 8));
      break;
    case TYPE_H:
      builder_->code_buffer_[index_+1] = (static_cast<std::uint32_t>(pc));
      break;
    default:
      lava_unreach("not implemented bytecode type or broken type");
  }
}

inline void BytecodeBuilder::Label::PatchDWord( std::uint32_t pc ) {
  lava_debug(NORMAL,lava_verify(type_ == TYPE_H););
  builder_->code_buffer_[index_+1] = pc;
}

inline BytecodeBuilder::BytecodeBuilder():
  code_buffer_(),
  debug_info_ (),
  real_table_ (),
  string_table_(),
  sso_table_  (),
  upvalue_slot_(),
  reg_offset_table_()
{ code_buffer_.reserve(kInitialCodeBufferSize); }

} // namespace interpreter
} // namespace lavascript

#endif // BYTECODE_BUILDER_H_
