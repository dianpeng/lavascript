#ifndef BYTECODE_H_
#define BYTECODE_H_
#include <cstdint>
#include <vector>
#include <algorithm>


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
class GC;
class String;
class Prototype;

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
  __(F,NOT   , not_  , "not"   ,REG,_,_) \
  /* branch */ \
  __(B,JMPT  , jmpt ,"jmpt" ,REG,PC,_   ) \
  __(B,JMPF  , jmpf ,"jmpf" ,REG,PC,_   ) \
  __(B,AND , and_ ,"and"  ,REG,PC,_   ) \
  __(B,OR  , or_ ,"or"  ,REG,PC,_   ) \
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
  __(F,LOADTRUE,loadtrue,"loadtrue" , REG , _ , _ ) \
  __(F,LOADFALSE,loadfalse,"loadfalse",REG, _ , _ ) \
  __(F,LOADNULL , loadnull , "loadnull" , REG , _ , _ ) \
  __(F,LOADLIST0, loadlist0, "loadlist0", REG , _ , _ ) \
  __(E,LOADLIST1, loadlist1, "loadlist1", REG , REG , _ ) \
  __(D,LOADLIST2, loadlist2, "loadlist2", REG , REG , REG ) \
  __(E,NEWLIST, newlist, "newlist" , REG , NARG  , _ ) \
  __(E,ADDLIST, addlist, "addlist" , REG , REG , _ )   \
  __(F,LOADOBJ0 , loadobj0 , "loadobj0" , REG , _ , _  ) \
  __(D,LOADOBJ1, loadobj1, "loadobj1", REG , REG , REG ) \
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
  __(X,FEVRSTART,fevrstart,"fevrstart",_,_,_) \
  __(G,FEVREND,fevrend,"frvrend",PC,_,_ ) \
  __(B,FESTART,festart,"festart",REG,PC,_) \
  __(B,FEEND  ,feend  ,"feend"  ,REG,PC,_) \
  __(E,IDREF  ,idref  ,"idref"  ,REG,REG,_) \
  __(G,BRK   ,brk,"brk",PC,_,_) \
  __(G,CONT  ,cont,"cont",PC,_,_) \
  /* always the last one */ \
  __(X,HLT,hlt,"hlt",_,_,_)


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

} // namespace interpreter
} // namespace lavascript

#endif // BYTECODE_H_
