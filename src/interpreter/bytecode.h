#ifndef BYTECODE_H_
#define BYTECODE_H_

/**
 *
 * Bytecode for the interpreter.
 *
 * Each bytecode occupies 4 bytes , it generally consists of 4 different types :
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
 *
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
namespace interpreter{

enum BytecodeType {
  TYPE_A,
  TYPE_B,
  TYPE_C,
  TYPE_D
};

#define LAVASCRIPT_BYTECODE_LIST(__) \
  /* arithmetic bytecode , if cannot hold , then spill */ \
  __(ADDIV , addiv , "+" , ACC , IREF , REG ) \
  __(ADDVI , addvi , "+" , ACC , REG , IREF ) \
  __(ADDRV , addrv , "+" , ACC , RREF , REG ) \
  __(ADDVR , addvr , "+" , ACC , REG , RREF ) \
  __(ADDVV , addvv , "+" , REG , REG , REG) \
  __(SUBIV , subiv , "-" , ACC , IREF , REG ) \
  __(SUBVI , subvi , "-" , ACC , REG , IREF ) \
  __(SUBRV , subrv , "-" , ACC , RREF , REG ) \
  __(SUBVR , subvr , "-" , ACC , REG  , RREF) \
  __(SUBVV , subvv , "-" , REG , REG , REG ) \
  __(MULIV , muliv , "*" , ACC , IREF , REG ) \
  __(MULVI , mulvi , "*" , ACC , REG , IREF ) \
  __(MULRV , mulrv , "*" , ACC , RREF , REG ) \
  __(MULVR , mulvr , "*" , ACC , REG  , RREF) \
  __(MULVV , mulvv , "*" , REG , REG , REG) \
  __(DIVIV , diviv , "/" , ACC , IREF , REG ) \
  __(DIVVI , divvi , "/" , ACC , REG , IREF ) \
  __(DIVRV , divrv , "/" , ACC , RREF , REG ) \
  __(DIVVR , divvr , "/" , ACC , REG , RREF ) \
  __(DIVVV , divvv , "/" , REG , REG , REG) \
  __(MODIV , modiv , "%" , ACC , IREF , REG ) \
  __(MODVI , modvi , "%" , ACC , REG , IREF ) \
  __(MODRV , modrv , "%" , ACC , RREF , REG ) \
  __(MODVR , modvr , "%" , ACC , REG , RREF ) \
  __(MODVV , modvv , "%" , REG , REG , REG ) \
  __(POWIV , powiv , "^" , ACC , IREF , REG ) \
  __(POWVI , powvi , "^" , ACC , REG , IREF ) \
  __(POWRV , powrv , "^" , ACC , RREF , REG ) \
  __(POWVR , powvr , "^" , ACC , REG , RREF ) \
  __(POWVV , powvv , "^" , REG , REG , REG) \
  /* comparison */ \
  __(EQIV  , eqiv  , "eq", ACC , IREF , REG ) \
  __(EQVI  , eqvi  , "eq", ACC , IREF , REG ) \
  __(EQRV  , eqrv  , "eq", ACC , RREF , REG ) \
  __(EQVR  , eqvr  , "eq", ACC , REG  , RREF) \
  __(EQSV  , eqsv  , "eq", ACC , SREF , REG ) \
  __(EQVS  , eqvs  , "eq", ACC , REG , SREF ) \
  __(EQVV  , eqvv  , "eq", REG , REG  , REG) \
  __(NEIV  , neiv  , "ne", ACC , IREF , REG ) \
  __(NEVI  , nevi  , "ne", ACC , REG , IREF ) \
  __(NERV  , nerv  , "ne", ACC , RREF, REG  ) \
  __(NEVR  , nevr  , "ne", ACC , REG , RREF ) \
  __(NESV  , nesv  , "ne", ACC , SREF , REG ) \
  __(NEVS  , nevs  , "ne", ACC , REG , SREF ) \
  __(NEVV  , nevv  , "ne", REG , REG , REG) \
  __(LTIV  , ltiv  , "lt", ACC , IREF , REG ) \
  __(LTVI  , ltvi  , "lt", ACC , REG , IREF ) \
  __(LTRV  , ltrv  , "lt", ACC , RREF, REG  ) \
  __(LTVR  , ltvr  , "lt", ACC , REG , RREF ) \
  __(LTSV  , ltsv  , "lt", ACC , SREF , REG ) \
  __(LTVS  , ltvs  , "lt", ACC , REG , SREF ) \
  __(LTVV  , ltvv  , "lt", REG , REG , REG) \
  __(LEIV  , leiv  , "le", ACC , IREF , REG ) \
  __(LEVI  , levi  , "le", ACC , REG , IREF ) \
  __(LERV  , lerv  , "le", ACC , RREF , REG ) \
  __(LEVR  , levr  , "le", ACC , REG , RREF ) \
  __(LESV  , lesv  , "le", ACC , SREF , REG ) \
  __(LEVS  , levs  , "le", ACC , REG , SREF ) \
  __(LEVV  , levv  , "le", REG , REG , REG) \
  __(GTIV  , gtiv  , "gt", ACC , IREF , REG ) \
  __(GTVI  , gtvi  , "gt", ACC , REG , IREF ) \
  __(GTRV  , gtrv  , "gt", ACC , RREF , REG ) \
  __(GTVR  , gtvr  , "gt", ACC , REG , RREF ) \
  __(GTSV  , gtsv  , "gt", ACC , SREF , REG ) \
  __(GTVS  , gtvs  , "gt", ACC , REG , SREF ) \
  __(GTVV  , gtvv  , "gt", REG , REG , REG ) \
  __(GEIV  , geiv  , "ge", ACC , IREF , REG ) \
  __(GEVI  , gevi  , "ge", ACC , REG , IREF ) \
  __(GERV  , gerv  , "ge", ACC , RREF , REG ) \
  __(GEVR  , gevr  , "ge", ACC , REG , RREF ) \
  __(GESV  , gesv  , "ge", ACC , SREF , REG ) \
  __(GEVS  , gevs  , "ge", ACC , REG , SREF ) \
  __(GEVV  , gevv  , "ge", REG , REG , REG ) \
  /* branch */ \
  __(JMPT  , jmpt ,"jmpt" ,REG,PC,_   ) \
  __(JMPF  , jmpf ,"jmpf" ,REG,PC,_   ) \
  __(BRT   , brt  ,"brt"  ,REG,PC,_   ) \
  __(BRF   , brf  ,"brf"  ,REG,PC,_   ) \
  __(JMP  , jmp , "jmp" ,PC, _ , _  ) \
  /* register move */ \
  __(MOVE , move , "move" , REG, REG, _ ) \
  __(GETAC getacc , "getacc" , REG , _ , _ ) \
  __(SETAC setacc , "setacc" , REG , _ , _ ) \
  /* constant loading */ \
  __(LOADI , loadi , "loadi" , REG , IREF , _    ) \
  __(LOAD0 , load0 , "load0" , REG , _ , _    ) \
  __(LOAD1 , load1 , "load1" , REG , _ , _    ) \
  __(LOADN1, loadn1, "loadn1" , REG , _ , _   ) \
  __(LOADR , loadr , "loadr" , REG , RREF , _    ) \
  __(LOADSTR,loadstr,"loadstr", REG , SREF , _   ) \
  __(LOADESTR,loadestr,"loadestr" , REG , _ , _ ) \
  __(LOADCSTR,loadcstr,"loadcstr" , REG , GARG , _ ) \
  __(LOADTRUE,loadtrue,"loadtrue" , REG , _ , _ ) \
  __(LOADFALSE,loadfalse,"loadfalse",REG, _ , _ ) \
  __(LOADNULL , loadnull , "loadnull" , REG , _ , _ ) \
  __(LOADLIST , loadlist , "loadlist" , REG , _ , _ ) \
  __(LOADOBJ  , loadobj  , "loadobj" , REG , _ , _  ) \
  __(LOADCLS  , loadcls  , "loadcls" , REG , _ , _  ) \
  /* subroutine */ \
  __(CALL , call , "call" , ARG , GARG , _ ) \
  __(FCALL, fcall, "fcall", ARG , FFUNC, _ ) \
  __(RET  , ret  , "ret"  , REF , _ , _ ) \
  /* property/upvalue/global value */ \
  __(PROPGET,propget,"propget",REG,SREF,_) \
  __(PROPSET,propset,"propset",REG,SREF,_) \
  __(IDXGET ,idxget,"idxget",REG,REG,_) \
  __(IDXSET ,idxset,"idxset",REG,REG,REG) \
  __(UVGET  ,uvget ,"uvget",REG,GARG,_) \
  __(UVSET  ,uvset ,"uvset",GARG,REG,_) \
  __(GSET   ,gset  ,"gset" ,GARG,REG,_) \
  __(GGET   ,gget  ,"gget" ,REG,GARG,_) \
  /* forloop tag */ \
  __(FSTART,fstart,"fstart",REG,PC,_) \
  __(FEND  ,fend  ,"fend"  ,REG,PC,_) \
  __(FESTART,festart,"festart",REG,PC,_) \
  __(FEEND  ,feend  ,"feend"  ,REG,PC,_) \
  __(BRK   ,brk,"brk",PC,_,_) \
  __(CONT  ,cont,"cont",PC,_,_)


/** bytecode **/
enum Bytecode {
#define __(A,B,C,D,E,F) BC_##A,
  LAVASCRIPT_BYTECODE_LIST(__)
  SIZE_OF_BYTECODE
#undef __ // __
};






#endif // BYTECODE_H_
