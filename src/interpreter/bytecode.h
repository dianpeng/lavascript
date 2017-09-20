#ifndef BYTECODE_H_
#define BYTECODE_H_
#include <cstdint>
#include <vector>

#include <src/flags.h>
#include <src/source-code-info.h>

/**
 *
 * Bytecode for the interpreter.
 *
 * Each bytecode occupies 4 bytes , it generally consists of 5 different types :
 *
 * ----------------------------
 * | OP |   Operand           |             type A
 * ----------------------------
 * | OP |  A   |       B      |             type B
 * ----------------------------
 * | OP |      A       |  B   |             type C
 * ----------------------------
 * | OP |  A   |   B   |   C  |             type D
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
namespace zone {
class String;
} // namespace zone

namespace interpreter{

static const std::size_t kTotalBytecodeRegisterSize = 256;
static const std::size_t kAllocatableBytecodeRegisterSize = 255;

#define LAVASCRIPT_BYTECODE_LIST(__) \
  /* arithmetic bytecode , if cannot hold , then spill */ \
  __(C,ADDIV , addiv , "+" , ACC , IREF , REG ) \
  __(B,ADDVI , addvi , "+" , ACC , REG , IREF ) \
  __(C,ADDRV , addrv , "+" , ACC , RREF , REG ) \
  __(B,ADDVR , addvr , "+" , ACC , REG , RREF ) \
  __(D,ADDVV , addvv , "+" , REG , REG , REG) \
  __(C,SUBIV , subiv , "-" , ACC , IREF , REG ) \
  __(B,SUBVI , subvi , "-" , ACC , REG , IREF ) \
  __(C,SUBRV , subrv , "-" , ACC , RREF , REG ) \
  __(B,SUBVR , subvr , "-" , ACC , REG  , RREF) \
  __(D,SUBVV , subvv , "-" , REG , REG , REG )  \
  __(C,MULIV , muliv , "*" , ACC , IREF , REG ) \
  __(B,MULVI , mulvi , "*" , ACC , REG , IREF ) \
  __(C,MULRV , mulrv , "*" , ACC , RREF , REG ) \
  __(B,MULVR , mulvr , "*" , ACC , REG  , RREF) \
  __(D,MULVV , mulvv , "*" , REG , REG , REG) \
  __(C,DIVIV , diviv , "/" , ACC , IREF , REG ) \
  __(B,DIVVI , divvi , "/" , ACC , REG , IREF ) \
  __(C,DIVRV , divrv , "/" , ACC , RREF , REG ) \
  __(B,DIVVR , divvr , "/" , ACC , REG , RREF ) \
  __(D,DIVVV , divvv , "/" , REG , REG , REG)   \
  __(C,MODIV , modiv , "%" , ACC , IREF , REG ) \
  __(B,MODVI , modvi , "%" , ACC , REG , IREF ) \
  __(C,MODRV , modrv , "%" , ACC , RREF , REG ) \
  __(B,MODVR , modvr , "%" , ACC , REG , RREF ) \
  __(D,MODVV , modvv , "%" , REG , REG , REG ) \
  __(C,POWIV , powiv , "^" , ACC , IREF , REG ) \
  __(B,POWVI , powvi , "^" , ACC , REG , IREF ) \
  __(C,POWRV , powrv , "^" , ACC , RREF , REG ) \
  __(B,POWVR , powvr , "^" , ACC , REG , RREF ) \
  __(D,POWVV , powvv , "^" , REG , REG , REG) \
  /* comparison */ \
  __(C,EQIV  , eqiv  , "eq", ACC , IREF , REG ) \
  __(B,EQVI  , eqvi  , "eq", ACC , IREF , REG ) \
  __(C,EQRV  , eqrv  , "eq", ACC , RREF , REG ) \
  __(B,EQVR  , eqvr  , "eq", ACC , REG  , RREF) \
  __(C,EQSV  , eqsv  , "eq", ACC , SREF , REG ) \
  __(B,EQVS  , eqvs  , "eq", ACC , REG , SREF ) \
  __(D,EQVV  , eqvv  , "eq", REG , REG  , REG) \
  __(C,NEIV  , neiv  , "ne", ACC , IREF , REG ) \
  __(B,NEVI  , nevi  , "ne", ACC , REG , IREF ) \
  __(C,NERV  , nerv  , "ne", ACC , RREF, REG  ) \
  __(B,NEVR  , nevr  , "ne", ACC , REG , RREF ) \
  __(C,NESV  , nesv  , "ne", ACC , SREF , REG ) \
  __(B,NEVS  , nevs  , "ne", ACC , REG , SREF ) \
  __(D,NEVV  , nevv  , "ne", REG , REG , REG) \
  __(C,LTIV  , ltiv  , "lt", ACC , IREF , REG ) \
  __(B,LTVI  , ltvi  , "lt", ACC , REG , IREF ) \
  __(C,LTRV  , ltrv  , "lt", ACC , RREF, REG  ) \
  __(B,LTVR  , ltvr  , "lt", ACC , REG , RREF ) \
  __(C,LTSV  , ltsv  , "lt", ACC , SREF , REG ) \
  __(B,LTVS  , ltvs  , "lt", ACC , REG , SREF ) \
  __(D,LTVV  , ltvv  , "lt", REG , REG , REG) \
  __(C,LEIV  , leiv  , "le", ACC , IREF , REG ) \
  __(B,LEVI  , levi  , "le", ACC , REG , IREF ) \
  __(C,LERV  , lerv  , "le", ACC , RREF , REG ) \
  __(B,LEVR  , levr  , "le", ACC , REG , RREF ) \
  __(C,LESV  , lesv  , "le", ACC , SREF , REG ) \
  __(B,LEVS  , levs  , "le", ACC , REG , SREF ) \
  __(D,LEVV  , levv  , "le", REG , REG , REG) \
  __(C,GTIV  , gtiv  , "gt", ACC , IREF , REG ) \
  __(B,GTVI  , gtvi  , "gt", ACC , REG , IREF ) \
  __(C,GTRV  , gtrv  , "gt", ACC , RREF , REG ) \
  __(B,GTVR  , gtvr  , "gt", ACC , REG , RREF ) \
  __(C,GTSV  , gtsv  , "gt", ACC , SREF , REG ) \
  __(B,GTVS  , gtvs  , "gt", ACC , REG , SREF ) \
  __(D,GTVV  , gtvv  , "gt", REG , REG , REG ) \
  __(C,GEIV  , geiv  , "ge", ACC , IREF , REG ) \
  __(B,GEVI  , gevi  , "ge", ACC , REG , IREF ) \
  __(C,GERV  , gerv  , "ge", ACC , RREF , REG ) \
  __(B,GEVR  , gevr  , "ge", ACC , REG , RREF ) \
  __(C,GESV  , gesv  , "ge", ACC , SREF , REG ) \
  __(B,GEVS  , gevs  , "ge", ACC , REG , SREF ) \
  __(D,GEVV  , gevv  , "ge", REG , REG , REG ) \
  /* branch */ \
  __(B,JMPT  , jmpt ,"jmpt" ,REG,PC,_   ) \
  __(B,JMPF  , jmpf ,"jmpf" ,REG,PC,_   ) \
  __(B,BRT   , brt  ,"brt"  ,REG,PC,_   ) \
  __(B,BRF   , brf  ,"brf"  ,REG,PC,_   ) \
  __(A,JMP  , jmp , "jmp" ,PC, _ , _  ) \
  /* register move */ \
  __(B,MOVE , move , "move" , REG, REG, _ ) \
  /* constant loading */ \
  __(B,LOADI , loadi , "loadi" , REG , IREF , _    ) \
  __(A,LOAD0 , load0 , "load0" , REG , _ , _    ) \
  __(A,LOAD1 , load1 , "load1" , REG , _ , _    ) \
  __(A,LOADN1, loadn1, "loadn1" , REG , _ , _   ) \
  __(B,LOADR , loadr , "loadr" , REG , RREF , _    ) \
  __(B,LOADSTR,loadstr,"loadstr", REG , SREF , _   ) \
  __(A,LOADESTR,loadestr,"loadestr" , REG , _ , _ ) \
  __(B,LOADCSTR,loadcstr,"loadcstr" , REG , GARG , _ ) \
  __(A,LOADTRUE,loadtrue,"loadtrue" , REG , _ , _ ) \
  __(A,LOADFALSE,loadfalse,"loadfalse",REG, _ , _ ) \
  __(A,LOADNULL , loadnull , "loadnull" , REG , _ , _ ) \
  __(A,LOADLIST , loadlist , "loadlist" , REG , _ , _ ) \
  __(A,LOADOBJ  , loadobj  , "loadobj" , REG , _ , _  ) \
  __(B,LOADCLS  , loadcls  , "loadcls" , REG , GARG , _  ) \
  /* subroutine */ \
  __(A,CALL , call , "call" , NARG , REG ,  _ ) \
  __(B,FCALL, fcall, "fcall", NARG , FFUNC, _ ) \
  __(B,TCALL, tcall, "tcall", NARG , GARG , _ ) \
  __(X,RET  , ret  , "ret"  , _ , _ , _ ) \
  /* property/upvalue/global value */ \
  __(B,PROPGET,propget,"propget",REG,SREF,_) \
  __(B,PROPSET,propset,"propset",REG,SREF,_) \
  __(B,IDXGET ,idxget,"idxget",REG,REG,_) \
  __(D,IDXSET ,idxset,"idxset",REG,REG,REG) \
  __(B,IDXGETI,idxgeti,"idxgeti",REG,IREF,_) \
  __(B,UVGET  ,uvget ,"uvget",REG,GARG,_) \
  __(C,UVSET  ,uvset ,"uvset",GARG,REG,_) \
  __(C,GSET   ,gset  ,"gset" ,GARG,REG,_) \
  __(B,GGET   ,gget  ,"gget" ,REG,GARG,_) \
  /* forloop tag */ \
  __(B,FSTART,fstart,"fstart",REG,PC,_) \
  __(B,FEND  ,fend  ,"fend"  ,REG,PC,_) \
  __(B,FESTART,festart,"festart",REG,PC,_) \
  __(B,FEEND  ,feend  ,"feend"  ,REG,PC,_) \
  __(A,BRK   ,brk,"brk",PC,_,_) \
  __(A,CONT  ,cont,"cont",PC,_,_)


/** bytecode **/
enum Bytecode {
#define __(A,B,C,D,E,F,G) BC_##B,
  LAVASCRIPT_BYTECODE_LIST(__)
  SIZE_OF_BYTECODE
#undef __ // __
};

/** bytecode type **/
enum BytecodeType {
  TYPE_A,
  TYPE_B,
  TYPE_C,
  TYPE_D,
  TYPE_X
};

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
  static std::uint32_t kOpcodeMask = 0xff;
  static std::uint32_t kTypeA_ArgA = 0xffffff00;
  static std::uint32_t kTypeB_ArgA = 0x0000ff00;
  static std::uint32_t kTypeB_ArgB = 0xffff0000;
  static std::uint32_t kTypeC_ArgA = 0xffff0000;
  static std::uint32_t kTypeC_ArgB = 0x0000ff00;
  static std::uint32_t kTypeD_ArgA = 0xff000000;
 public:
  inline BytecodeBuilder();

  std::int32_t Add( std::int32_t );
  std::int32_t Add( double );
  std::int32_t Add( const ::lavascript::zone::String& , Context* );

 public:
  inline const SourceCodeInfo& IndexSourceCodeInfo( std::size_t index ) const;

  class Label {
   public:
    Label( BytecodeBuilder* , std::size_t );
    inline void PatchA( std::uint32_t );
    inline void PatchB( std::uint8_t , std::uint16_t );
    inline void PatchC( std::uint16_t, std::uint8_t  );
    inline void PatchD( std::uint8_t , std::uint8_t , std::uint8_t );
    inline void PatchX();
   private:
    std::size_t index_;
    BytecodeBuilder* builder_;
  };

  inline void emit_A( const SourceCodeInfo& , Bytecode , std::uint32_t );
  inline void emit_B( const SourceCodeInfo& , Bytecode , std::uint8_t , std::uint16_t );
  inline void emit_C( const SourceCodeInfo& , Bytecode , std::uint16_t, std::uint8_t  );
  inline void emit_D( const SourceCodeInfo& , Bytecode , std::uint8_t , std::uint8_t , std::uint8_t );
  inline void emit_X( const SourceCodeInfo& , Bytecode );
  inline Label emit_at( const SourceCodeInfo& , Bytecode );

 public:
  /** ---------------------------------------------------
   * Bytecode emittion
   * ---------------------------------------------------*/

#define IMPLA(INSTR) \
  (const SourceCodeInfo& si , std::uint32_t a1) { return emit_A(si,INSTR,a1); }

#define IMPLB(INSTR) \
  (const SourceCodeInfo& si , std::uint8_t a1 , std::uint16_t a2 ) { return emit_B(si,INSTR,a1,a2); }

#define IMPLC(INSTR) \
  (const SourceCodeInfo& si , std::uint16_t a1, std::uint8_t a2 )  { return emit_C(si,INSTR,a1,a2); }

#define IMPLD(INSTR) \
  (const SourceCodeInfo& si , std::uint8_t a1, std::uint8_t a2, std::uint8_t a3 ) \
  { return emit_D(si,INSTR,a1,a2,a3); }

#define IMPLE(INSTR) \
  (const SourceCodeInfo& si) { return emit_X(si,INSTR); }

#define __(A,B,C,D,E,F,G) inline void C IMPL##A(B)

  LAVASCRIPT_BYTECODE_LIST(__)

#undef __ // __
#undef IMPLA
#undef IMPLB
#undef IMPLC
#undef IMPLD
#undef IMPLX

 public:
  Label jmpt( const SourceCodeInfo& si ) { return emit_at(si,BC_JMPT); }
  Label jmpf( const SourceCodeInfo& si ) { return emit_at(si,BC_JMPF); }
  Label brt ( const SourceCodeInfo& si ) { return emit_at(si,BC_BRT ); }
  Label brf ( const SourceCodeInfo& si ) { return emit_at(si,BC_BRF ); }
  Label jmp ( const SourceCodeInfo& si ) { return emit_at(si,BC_JMP ); }
  Label brk ( const SourceCodeInfo& si ) { return emit_at(si,BC_BRK ); }
  Label cont( const SourceCodeInfo& si ) { return emit_at(si,BC_CONT); }

 public:
  void Dump( DumpFlag flag , const char* file = NULL ) const;

 private:
  std::vector<std::uint32_t> code_buffer_;           // Code buffer
  std::vector<SourceCodeInfo> debug_info_;           // Debug info
  std::vector<std::int32_t> int_table_;              // Integer table
  std::vector<std::double> real_table_;              // Real table
  std::vector<String*> string_table_;                // String table
};


} // namespace interpreter
} // namespace lavascript

#endif // BYTECODE_H_
