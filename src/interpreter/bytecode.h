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
 * ---------------------------
 * | OP | A    |  B    |  C   |             type H
 * ----------------------------
 * |           D              |
 * ----------------------------
 *
 * The bytecode are register based bytecode , it has 256 registers to be used. These
 * registers are shared with local variable slots and also intermiediate expression.
 * So in theory we can run out of register , the compiler will fire a too complicated
 * expression/function error. Additionally we have a implicit accumulator register to
 * be used by certain bytecode for holding results or passing around register information
 *
 * The accumulator is aliased with #255 register . So instruction can reference accregister
 * via #255 register.
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
  __(D,ADDIV , addiv , REG , IREF , REG ) \
  __(D,ADDVI , addvi , REG , REG , IREF ) \
  __(D,ADDRV , addrv , REG , RREF , REG ) \
  __(D,ADDVR , addvr , REG , REG , RREF ) \
  __(D,ADDVV , addvv , REG , REG , REG)   \
  __(D,SUBIV , subiv , REG , IREF , REG ) \
  __(D,SUBVI , subvi , REG , REG , IREF ) \
  __(D,SUBRV , subrv , REG , RREF , REG ) \
  __(D,SUBVR , subvr , REG , REG  , RREF) \
  __(D,SUBVV , subvv , REG , REG , REG )  \
  __(D,MULIV , muliv , REG , IREF , REG ) \
  __(D,MULVI , mulvi , REG , REG , IREF ) \
  __(D,MULRV , mulrv , REG , RREF , REG ) \
  __(D,MULVR , mulvr , REG , REG  , RREF) \
  __(D,MULVV , mulvv , REG , REG , REG)   \
  __(D,DIVIV , diviv , REG , IREF , REG ) \
  __(D,DIVVI , divvi , REG , REG , IREF ) \
  __(D,DIVRV , divrv , REG , RREF , REG ) \
  __(D,DIVVR , divvr , REG , REG , RREF ) \
  __(D,DIVVV , divvv , REG , REG , REG)   \
  __(D,MODIV , modiv , REG , IREF , REG ) \
  __(D,MODVI , modvi , REG , REG , IREF ) \
  __(D,MODVV , modvv , REG , REG , REG )  \
  __(D,POWIV , powiv , REG , IREF , REG ) \
  __(D,POWVI , powvi , REG , REG , IREF ) \
  __(D,POWRV , powrv , REG , RREF , REG ) \
  __(D,POWVR , powvr , REG , REG , RREF ) \
  __(D,POWVV , powvv , REG , REG , REG)   \
  /* comparison */                        \
  __(D,LTIV  , ltiv  , REG , IREF , REG ) \
  __(D,LTVI  , ltvi  , REG , REG , IREF ) \
  __(D,LTRV  , ltrv  , REG , RREF, REG  ) \
  __(D,LTVR  , ltvr  , REG , REG , RREF ) \
  __(D,LTVV  , ltvv  , REG , REG , REG)   \
  __(D,LEIV  , leiv  , REG , IREF , REG ) \
  __(D,LEVI  , levi  , REG , REG , IREF ) \
  __(D,LERV  , lerv  , REG , RREF , REG ) \
  __(D,LEVR  , levr  , REG , REG , RREF ) \
  __(D,LEVV  , levv  , REG , REG , REG)   \
  __(D,GTIV  , gtiv  , REG , IREF , REG ) \
  __(D,GTVI  , gtvi  , REG , REG , IREF ) \
  __(D,GTRV  , gtrv  , REG , RREF , REG ) \
  __(D,GTVR  , gtvr  , REG , REG , RREF ) \
  __(D,GTVV  , gtvv  , REG , REG , REG )  \
  __(D,GEIV  , geiv  , REG , IREF , REG ) \
  __(D,GEVI  , gevi  , REG , REG , IREF ) \
  __(D,GERV  , gerv  , REG , RREF , REG ) \
  __(D,GEVR  , gevr  , REG , REG , RREF ) \
  __(D,GEVV  , gevv  , REG , REG , REG )  \
  __(D,EQIV  , eqiv  , REG , IREF , REG ) \
  __(D,EQVI  , eqvi  , REG , IREF , REG ) \
  __(D,EQRV  , eqrv  , REG , RREF , REG ) \
  __(D,EQVR  , eqvr  , REG , REG  , RREF) \
  __(D,EQSV  , eqsv  , REG , SREF , REG ) \
  __(D,EQVS  , eqvs  , REG , REG , SREF ) \
  __(D,EQVV  , eqvv  , REG , REG  , REG)  \
  __(D,NEIV  , neiv  , REG , IREF , REG ) \
  __(D,NEVI  , nevi  , REG , REG , IREF ) \
  __(D,NERV  , nerv  , REG , RREF, REG  ) \
  __(D,NEVR  , nevr  , REG , REG , RREF ) \
  __(D,NESV  , nesv  , REG , SREF , REG ) \
  __(D,NEVS  , nevs  , REG , REG , SREF ) \
  __(D,NEVV  , nevv  , REG , REG , REG)   \
  /* unary */ \
  __(E,NEGATE, negate, REG,REG,_)  \
  __(E,NOT   , not_  , REG,REG,_)  \
  /* branch */ \
  __(B,JMPT , jmpt ,REG,PC,_   ) \
  __(B,JMPF , jmpf ,REG,PC,_   ) \
  __(B,AND  , and_ ,REG,PC,_  )  \
  __(B,OR   , or_  ,REG,PC,_  )  \
  __(G,JMP  , jmp  ,PC,_,_  )    \
  /* register move */ \
  __(E,MOVE , move , REG, REG, _ )              \
  /* constant loading */                        \
  __(E,LOADI , loadi , REG , IREF , _ )         \
  __(F,LOAD0 , load0 , REG , _ , _    )         \
  __(F,LOAD1 , load1 , REG , _ , _    )         \
  __(F,LOADN1, loadn1, REG , _ , _    )         \
  __(E,LOADR , loadr , REG , RREF , _ )         \
  __(E,LOADSTR,loadstr,REG , SREF , _ )         \
  __(F,LOADTRUE,loadtrue,REG , _ , _  )         \
  __(F,LOADFALSE,loadfalse,  REG, _ , _ )       \
  __(F,LOADNULL , loadnull , REG , _ , _ )      \
  __(F,LOADLIST0, loadlist0, REG , _ , _ )      \
  __(E,LOADLIST1, loadlist1, REG , REG , _ )    \
  __(D,LOADLIST2, loadlist2, REG , REG , REG )  \
  __(B,NEWLIST, newlist, REG , NARG  , _ )      \
  __(E,ADDLIST, addlist, REG , REG , _ )        \
  __(F,LOADOBJ0 , loadobj0 , REG , _ , _  )     \
  __(D,LOADOBJ1, loadobj1, REG , REG , REG )    \
  __(B,NEWOBJ  , newobj , REG , NARG  , _  )    \
  __(D,ADDOBJ  , addobj , REG , REG , REG )     \
  __(C,LOADCLS  , loadcls  , REG , GARG , _ )   \
  /* property/upvalue/global value */           \
  __(D,PROPGET,propget,REG,REG,SREF) \
  __(D,PROPSET,propset,REG,SREF,REG) \
  __(D,IDXGET ,idxget,REG,REG,REG)   \
  __(D,IDXSET ,idxset,REG,REG,REG)   \
  __(D,IDXGETI,idxgeti,REG,REG,IREF) \
  __(E,UVGET  ,uvget ,REG,GARG,_)    \
  __(E,UVSET  ,uvset ,GARG,REG,_)    \
  __(E,GSET   ,gset  ,SREF,REG,_)    \
  __(E,GGET   ,gget  ,REG,SREF,_)    \
  /* subroutine */ \
  __(D,CALL, call , REG , BASE , NARG )         \
  __(D,TCALL, tcall, REG , BASE , NARG )        \
  __(X,RETNULL, retnull , _ , _ , _ )           \
  __(X,RET  , ret  , _ , _ , _ )                \
  /* forloop tag */ \
  __(B,FSTART,fstart,REG,PC,_)     \
  __(H,FEND1,fend1 ,REG,REG,REG)   \
  __(H,FEND2,fend2 ,REG,REG,REG)   \
  __(X,FEVRSTART,fevrstart,_,_,_)  \
  __(G,FEVREND,fevrend,PC,_,_ )    \
  __(E,INEW,inew,REG,REG,_)        \
  __(B,FESTART,festart,REG,PC,_)   \
  __(B,FEEND  ,feend  ,REG,PC,_)   \
  __(E,IDREF  ,idref  ,REG,REG,_)  \
  __(G,BRK   ,brk,PC,_,_)          \
  __(G,CONT  ,cont,PC,_,_)         \
  /* always the last one */        \
  __(X,HLT,hlt,_,_,_)


/** bytecode **/
enum Bytecode {
#define __(A,B,C,D,E,F) BC_##B,
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
  TYPE_H,
  TYPE_X,

  SIZE_OF_BYTECODE_TYPE
};

const char* GetBytecodeName( Bytecode );
BytecodeType GetBytecodeType( Bytecode );

} // namespace interpreter
} // namespace lavascript

#endif // BYTECODE_H_
