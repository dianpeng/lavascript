#ifndef BYTECODE_H_
#define BYTECODE_H_
#include <cstdint>
#include <vector>

#include <src/objects.h>
#include <src/flags.h>
#include <src/source-code-info.h>

/**
 *
 * Bytecode for the interpreter.
 *
 * Each bytecode occupies 4 bytes , it generally consists of following different types :
 *
 * ----------------------------
 * | OP |  A   |       B      |             type B
 * ----------------------------
 * | OP |      A       |  B   |             type C
 * ----------------------------
 * | OP |  A   |   B   |   C  |             type D
 * ----------------------------
 * | OP |   A  |   B   | xxxx |             type E
 * ----------------------------
 * | OP |   A  | xxxxxxxxxxxxx|             type F
 * ----------------------------
 * | OP |  A           | xxxx |             type G
 * ----------------------------
 * | OP | xxxxxxxxxxxxxxxxxxxx|             type X
 * ----------------------------
 *
 * The bytecode are register based bytecode , it has 256 registers to be used. These
 * registers are shared with local variable slots and also intermiediate expression.
 * So in theory we can run out of register , the compiler will fire a too complicated
 * expression/function error. Additionally we have a implicit accumulator register to
 * be used by certain bytecode for holding results or passing around register information
 *
 * There's another accumulator register will be used implicitly. The accumulator register
 * is aliased with #0 register . So instruction can reference acc register via #0 register.
 *
 */

namespace lavascript {
class Context;
class String;

namespace zone {
class String;
} // namespace zone

namespace parser {
namespace ast {
struct Function;
} // namespace ast
} // namespace parser

namespace interpreter{

static const std::size_t kTotalBytecodeRegisterSize = 256;
static const std::size_t kAllocatableBytecodeRegisterSize = 255;

/** NOTES: Order matters **/
#define LAVASCRIPT_BYTECODE_LIST(__) \
  /* arithmetic bytecode , if cannot hold , then spill */ \
  __(C,ADDIV , addiv , "+" , ACC , IREF , REG ) \
  __(B,ADDVI , addvi , "+" , ACC , REG , IREF ) \
  __(C,ADDRV , addrv , "+" , ACC , RREF , REG ) \
  __(B,ADDVR , addvr , "+" , ACC , REG , RREF ) \
  __(E,ADDVV , addvv , "+" , ACC , REG , REG)   \
  __(C,SUBIV , subiv , "-" , ACC , IREF , REG ) \
  __(B,SUBVI , subvi , "-" , ACC , REG , IREF ) \
  __(C,SUBRV , subrv , "-" , ACC , RREF , REG ) \
  __(B,SUBVR , subvr , "-" , ACC , REG  , RREF) \
  __(E,SUBVV , subvv , "-" , ACC , REG , REG )  \
  __(C,MULIV , muliv , "*" , ACC , IREF , REG ) \
  __(B,MULVI , mulvi , "*" , ACC , REG , IREF ) \
  __(C,MULRV , mulrv , "*" , ACC , RREF , REG ) \
  __(B,MULVR , mulvr , "*" , ACC , REG  , RREF) \
  __(E,MULVV , mulvv , "*" , ACC , REG , REG)   \
  __(C,DIVIV , diviv , "/" , ACC , IREF , REG ) \
  __(B,DIVVI , divvi , "/" , ACC , REG , IREF ) \
  __(C,DIVRV , divrv , "/" , ACC , RREF , REG ) \
  __(B,DIVVR , divvr , "/" , ACC , REG , RREF ) \
  __(E,DIVVV , divvv , "/" , ACC , REG , REG)   \
  __(C,MODIV , modiv , "%" , ACC , IREF , REG ) \
  __(B,MODVI , modvi , "%" , ACC , REG , IREF ) \
  __(E,MODVV , modvv , "%" , ACC , REG , REG )  \
  __(C,POWIV , powiv , "^" , ACC , IREF , REG ) \
  __(B,POWVI , powvi , "^" , ACC , REG , IREF ) \
  __(C,POWRV , powrv , "^" , ACC , RREF , REG ) \
  __(B,POWVR , powvr , "^" , ACC , REG , RREF ) \
  __(E,POWVV , powvv , "^" , ACC , REG , REG)   \
  /* comparison */                              \
  __(C,LTIV  , ltiv  , "lt", ACC , IREF , REG ) \
  __(B,LTVI  , ltvi  , "lt", ACC , REG , IREF ) \
  __(C,LTRV  , ltrv  , "lt", ACC , RREF, REG  ) \
  __(B,LTVR  , ltvr  , "lt", ACC , REG , RREF ) \
  __(C,LTSV  , ltsv  , "lt", ACC , SREF , REG ) \
  __(B,LTVS  , ltvs  , "lt", ACC , REG , SREF ) \
  __(E,LTVV  , ltvv  , "lt", ACC , REG , REG)   \
  __(C,LEIV  , leiv  , "le", ACC , IREF , REG ) \
  __(B,LEVI  , levi  , "le", ACC , REG , IREF ) \
  __(C,LERV  , lerv  , "le", ACC , RREF , REG ) \
  __(B,LEVR  , levr  , "le", ACC , REG , RREF ) \
  __(C,LESV  , lesv  , "le", ACC , SREF , REG ) \
  __(B,LEVS  , levs  , "le", ACC , REG , SREF ) \
  __(E,LEVV  , levv  , "le", ACC , REG , REG)   \
  __(C,GTIV  , gtiv  , "gt", ACC , IREF , REG ) \
  __(B,GTVI  , gtvi  , "gt", ACC , REG , IREF ) \
  __(C,GTRV  , gtrv  , "gt", ACC , RREF , REG ) \
  __(B,GTVR  , gtvr  , "gt", ACC , REG , RREF ) \
  __(C,GTSV  , gtsv  , "gt", ACC , SREF , REG ) \
  __(B,GTVS  , gtvs  , "gt", ACC , REG , SREF ) \
  __(E,GTVV  , gtvv  , "gt", ACC , REG , REG )  \
  __(C,GEIV  , geiv  , "ge", ACC , IREF , REG ) \
  __(B,GEVI  , gevi  , "ge", ACC , REG , IREF ) \
  __(C,GERV  , gerv  , "ge", ACC , RREF , REG ) \
  __(B,GEVR  , gevr  , "ge", ACC , REG , RREF ) \
  __(C,GESV  , gesv  , "ge", ACC , SREF , REG ) \
  __(B,GEVS  , gevs  , "ge", ACC , REG , SREF ) \
  __(E,GEVV  , gevv  , "ge", ACC , REG , REG )  \
  __(C,EQIV  , eqiv  , "eq", ACC , IREF , REG ) \
  __(B,EQVI  , eqvi  , "eq", ACC , IREF , REG ) \
  __(C,EQRV  , eqrv  , "eq", ACC , RREF , REG ) \
  __(B,EQVR  , eqvr  , "eq", ACC , REG  , RREF) \
  __(C,EQSV  , eqsv  , "eq", ACC , SREF , REG ) \
  __(B,EQVS  , eqvs  , "eq", ACC , REG , SREF ) \
  __(E,EQVV  , eqvv  , "eq", ACC , REG  , REG)  \
  __(C,NEIV  , neiv  , "ne", ACC , IREF , REG ) \
  __(B,NEVI  , nevi  , "ne", ACC , REG , IREF ) \
  __(C,NERV  , nerv  , "ne", ACC , RREF, REG  ) \
  __(B,NEVR  , nevr  , "ne", ACC , REG , RREF ) \
  __(C,NESV  , nesv  , "ne", ACC , SREF , REG ) \
  __(B,NEVS  , nevs  , "ne", ACC , REG , SREF ) \
  __(E,NEVV  , nevv  , "ne", ACC , REG , REG)   \
  /* unary */ \
  __(F,NEGATE, negate, "negate",REG,_,_) \
  __(F,NOT   , not   , "not"   ,REG,_,_) \
  /* branch */ \
  __(B,JMPT  , jmpt ,"jmpt" ,REG,PC,_   ) \
  __(B,JMPF  , jmpf ,"jmpf" ,REG,PC,_   ) \
  __(B,AND , and ,"and"  ,REG,PC,_   ) \
  __(B,OR  , or ,"or"  ,REG,PC,_   ) \
  __(G,JMP  , jmp , "jmp" ,PC, _ , _  ) \
  /* register move */ \
  __(B,MOVE , move , "move" , REG, REG, _ ) \
  /* constant loading */ \
  __(B,LOADI , loadi , "loadi" , REG , IREF , _    ) \
  __(F,LOAD0 , load0 , "load0" , REG , _ , _    ) \
  __(F,LOAD1 , load1 , "load1" , REG , _ , _    ) \
  __(F,LOADN1, loadn1, "loadn1" , REG , _ , _   ) \
  __(B,LOADR , loadr , "loadr" , REG , RREF , _    ) \
  __(B,LOADSTR,loadstr,"loadstr", REG , SREF , _   ) \
  __(F,LOADESTR,loadestr,"loadestr" , REG , _ , _ ) \
  __(B,LOADCSTR,loadcstr,"loadcstr" , REG , GARG , _ ) \
  __(A,LOADTRUE,loadtrue,"loadtrue" , REG , _ , _ ) \
  __(F,LOADFALSE,loadfalse,"loadfalse",REG, _ , _ ) \
  __(F,LOADNULL , loadnull , "loadnull" , REG , _ , _ ) \
  __(F,LOADLIST0, loadlist0, "loadlist0", REG , _ , _ ) \
  __(E,LOADLIST1, loadlist1, "loadlist1", REG , REG , _ ) \
  __(D,LOADLIST2, loadlist2, "loadlist2", REG , REG , REG ) \
  __(E,NEWLIST, newlist, "newlist" , REG , NARG  , _ ) \
  __(E,ADDLIST, addlist, "addlist" , REG , REG , _ )   \
  __(F,LOADOBJ0 , loadobj0 , "loadobj0" , REG , _ , _ , _ ) \
  __(E,LOADOBJ1, loadobj1, "loadobj1", REG , REG , REG ) \
  __(E,NEWOBJ  , newobj , "newobj" , REG , NARG  , _  ) \
  __(D,ADDOBJ  , addobj , "addobj" , REG , REG , REG ) \
  __(G,LOADCLS  , loadcls  , "loadcls" , GARG , _  , _ ) \
  /* subroutine */ \
  __(D,CALL , call , "call" , NARG , REG , BASE ) \
  __(D,TCALL, tcall, "tcall", NARG , REG , BASE ) \
  __(X,RET0 , ret0 , "ret0" , _ , _ , _ ) \
  __(X,RET  , ret  , "ret"  , _ , _ , _ ) \
  /* property/upvalue/global value */ \
  __(B,PROPGET,propget,"propget",REG,SREF,_) \
  __(B,PROPSET,propset,"propset",REG,SREF,_) \
  __(E,IDXGET ,idxget,"idxget",REG,REG,_) \
  __(D,IDXSET ,idxset,"idxset",REG,REG,REG) \
  __(B,IDXGETI,idxgeti,"idxgeti",REG,IREF,_) \
  __(B,UVGET  ,uvget ,"uvget",REG,GARG,_) \
  __(C,UVSET  ,uvset ,"uvset",GARG,REG,_) \
  __(C,GSET   ,gset  ,"gset" ,GARG,REG,_) \
  __(B,GGET   ,gget  ,"gget" ,REG,GARG,_) \
  /* forloop tag */ \
  __(B,FSTART,fstart,"fstart",REG,PC,_) \
  __(G,FEND  ,fend  ,"fend"  ,PC,_,_) \
  __(G,FEVRSTART,fevrstart,"fevrstart",PC,_,_) \
  __(G,FEVREND,fevrend,"frvrend",PC,_,_ ) \
  __(B,FESTART,festart,"festart",REG,PC,_) \
  __(B,FEEND  ,feend  ,"feend"  ,REG,PC,_) \
  __(E,IDREF  ,idref  ,"idref"  ,REG,REG,_) \
  __(G,BRK   ,brk,"brk",PC,_,_) \
  __(G,CONT  ,cont,"cont",PC,_,_) \
  /* always the last one */ \
  __(X,BC_HLT,hlt,"hlt",_,_,_)


/** bytecode **/
enum Bytecode {
#define __(A,B,C,D,E,F,G) BC_##B,
  LAVASCRIPT_BYTECODE_LIST(__)
  SIZE_OF_BYTECODE
#undef __ // __
};

static_assert( SIZE_OF_BYTECODE <= 255 );

/** bytecode type **/
enum BytecodeType {
  TYPE_B,
  TYPE_C,
  TYPE_D,
  TYPE_E,
  TYPE_F,
  TYPE_G,
  TYPE_X
};

/**
 * UpValue management
 * UpValue is a value that is not inside of a function's lexical scope
 * but in its upper level enclosed function scope. We use a similar startegy
 * with Lua. Basically just collapse the value along with each closure's
 * upvalue slots
 *
 * UpValue has 2 states , if its state is *EMBED* , which means the UpValue
 * should be retrieved in register/stack slot ; other state is *DETACH* ,
 * which means the UpValue should be retrieved in its enclosed function's
 * upvalue slot
 */
enum UpValueState {
  UV_EMBED,
  UV_DETACH
};

const char* GetUpValueStateName( UpValueState );

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
  static const std::size_t kMaxCodeLength = 65536;
 public:
  inline BytecodeBuilder();

 public:
  inline bool AddUpValue( UpValueState , std::uint16_t , std::uint16_t* );

  std::int32_t Add( std::int32_t );
  std::int32_t Add( double );
  std::int32_t Add( const ::lavascript::zone::String& , Context* );

 public:
  std::uint16_t CodePosition() const {
    return static_cast<uint16_t>(code_buffer_.size());
  }

  inline const SourceCodeInfo& IndexSourceCodeInfo( std::size_t index ) const;

  class Label {
   public:
    Label( BytecodeBuilder* , std::size_t , BytecodeType );
    Label();
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
  inline bool EmitD( const SourceCodeInfo& , Bytecode , std::uint8_t , std::uint8_t ,
                                                                       std::uint8_t );
  inline bool EmitE( const SourceCodeInfo& , Bytecode , std::uint8_t , std::uint8_t );
  inline bool EmitF( const SourceCodeInfo& , Bytecode , std::uint8_t );
  inline bool EmitG( const SourceCodeInfo& , Bytecode , std::uint16_t );
  inline bool EmitX( const SourceCodeInfo& , Bytecode );

  template< int BC , int TP , bool A1 = false , bool A2 = false , bool A3 = false >
  inline Lable EmitAt( const SourceCodeInfo& , std::uint32_t a1 = 0 ,
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
  bool C(const SourceCodeInfo& si , std::uint8_t al , std::uint8_t a2 ) { \
    return EmitE(so,INSTR,a1,a2);                                         \
  }

#define IMPLF(INSTR,C)                                                 \
  bool C(const SourceCodeInfo& si , std::uint8_t a1 ) {                \
    return EmitF(so,INSTR,a1);                                         \
  }

#define IMPLG(INSTR,C)                                                 \
  bool C(const SourceCodeInfo& si , std::uint16_t a1 ) {               \
    return EmitG(so,INSTR,a1);                                         \
  }

#define IMPLX(INSTR,C)                                                 \
  bool C(const SourceCodeInfo& si) {                                   \
    return EmitX(si,INSTR);                                            \
  }

#define __(A,B,C,D,E,F,G) IMPL##A(BC_##B,C)
  LAVASCRIPT_BYTECODE_LIST(__)
#undef __           // __
#undef IMPLB        // IMPLB
#undef IMPLC        // IMPLC
#undef IMPLD        // IMPLD
#undef IMPLE        // IMPLE
#undef IMPLF        // IMPLF
#undef IMPLG        // IMPLG
#undef IMPLX        // IMPLX

 public:
  /* -----------------------------------------------------
   * Jump related isntruction                            |
   * ----------------------------------------------------*/
  inline Label jmpt   ( const SourceCodeInfo& si , const std::uint8_t r );
  inline Label jmpf   ( const SourceCodeInfo& si , const std::uint8_t r );
  inline Label and    ( const SourceCodeInfo& si );
  inline Label or     ( const SourceCodeInfo& si );
  inline Label jmp    ( const SourceCodeInfo& si );
  inline Label brk    ( const SourceCodeInfo& si );
  inline Label cont   ( const SourceCodeInfo& si );
  inline Label fstart ( const SourceCodeInfo& si , const std::uint8_t a1 );
  inline Label festart( const SourceCodeInfo& si , const std::uint8_t a1 );

 public:
  void Dump( DumpFlag flag , const char* file = NULL ) const;

 public:
  // This function will create a Closure object from the BytecodeBuilder
  // object. After that we convert a builder's internal information to a
  // managed heap based closure which can be used in the VM
  static Handle<Prototype> New( GC* , const BytecodeBuilder& bb ,
                                      const ::lavascript::parser::ast::Function& node );
  static Handle<Prototype> New( GC* , const BytecodeBuilder& bb );

 private:
  std::vector<std::uint32_t> code_buffer_;           // Code buffer
  std::vector<SourceCodeInfo> debug_info_;           // Debug info
  std::vector<std::int32_t> int_table_;              // Integer table
  std::vector<double> real_table_;                   // Real table
  std::vector<String*> string_table_;                // String table

  struct UpValueSlot {
    std::uint16_t index;      // Can represent the register index or the slot
                              // index inside of the upvalue array slot
    UpValueState state;       // State of UpValue
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

  debug_info_.push_back(encode);
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

inline BytecodeBuilder::Label BytecodeBuilder::and ( const SourceCodeInfo& sci ) {
  return EmitAt<BC_AND,TYPE_G,false,false,false>(sci);
}

inline BytecodeBuilder::Label BytecodeBuilder::or  ( const SourceCodeInfo& sci ) {
  return EmitAt<BC_OR,TYPE_G,false,false,false>(sci);
}

inline BytecodeBuilder::Label BytecodeBuilder::jmp ( const SourceCodeInfo& sci ) {
  return EmitAt<BC_OR,TYPE_G,false,false,false>(sci);
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
  return EmitAt(BC_FSTART,TYPE_B,true,false,false>(sci,a1);
}


} // namespace interpreter
} // namespace lavascript

#endif // BYTECODE_H_
