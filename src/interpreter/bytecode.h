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
 * | OP | A    |  B    |  C   |             type N
 * ----------------------------
 * | A1 | A2   | A3    | A4   |
 * ----------------------------
 * |   .....................  |
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
  __(C,ADDIV , addiv ,  _ , IREF , REG ) \
  __(B,ADDVI , addvi ,  _ , REG , IREF ) \
  __(C,ADDRV , addrv ,  _ , RREF , REG ) \
  __(B,ADDVR , addvr ,  _ , REG , RREF ) \
  __(E,ADDVV , addvv ,  _ , REG , REG)   \
  __(C,SUBIV , subiv ,  _ , IREF , REG ) \
  __(B,SUBVI , subvi ,  _ , REG , IREF ) \
  __(C,SUBRV , subrv ,  _ , RREF , REG ) \
  __(B,SUBVR , subvr ,  _ , REG  , RREF) \
  __(E,SUBVV , subvv ,  _ , REG , REG )  \
  __(C,MULIV , muliv ,  _ , IREF , REG ) \
  __(B,MULVI , mulvi ,  _ , REG , IREF ) \
  __(C,MULRV , mulrv ,  _ , RREF , REG ) \
  __(B,MULVR , mulvr ,  _ , REG  , RREF) \
  __(E,MULVV , mulvv ,  _ , REG , REG)   \
  __(C,DIVIV , diviv ,  _ , IREF , REG ) \
  __(B,DIVVI , divvi ,  _ , REG , IREF ) \
  __(C,DIVRV , divrv ,  _ , RREF , REG ) \
  __(B,DIVVR , divvr ,  _ , REG , RREF ) \
  __(E,DIVVV , divvv ,  _ , REG , REG)   \
  __(C,MODIV , modiv ,  _ , IREF , REG ) \
  __(B,MODVI , modvi ,  _ , REG , IREF ) \
  __(E,MODVV , modvv ,  _ , REG , REG )  \
  __(C,POWIV , powiv ,  _ , IREF , REG ) \
  __(B,POWVI , powvi ,  _ , REG , IREF ) \
  __(C,POWRV , powrv ,  _ , RREF , REG ) \
  __(B,POWVR , powvr ,  _ , REG , RREF ) \
  __(E,POWVV , powvv ,  _ , REG , REG)   \
  /* comparison */                       \
  __(C,LTIV  , ltiv  , _ , IREF , REG ) \
  __(B,LTVI  , ltvi  , _ , REG , IREF ) \
  __(C,LTRV  , ltrv  , _ , RREF, REG  ) \
  __(B,LTVR  , ltvr  , _ , REG , RREF ) \
  __(C,LTSV  , ltsv  , _ , SREF , REG ) \
  __(B,LTVS  , ltvs  , _ , REG , SREF ) \
  __(E,LTVV  , ltvv  , _ , REG , REG)   \
  __(C,LEIV  , leiv  , _ , IREF , REG ) \
  __(B,LEVI  , levi  , _ , REG , IREF ) \
  __(C,LERV  , lerv  , _ , RREF , REG ) \
  __(B,LEVR  , levr  , _ , REG , RREF ) \
  __(C,LESV  , lesv  , _ , SREF , REG ) \
  __(B,LEVS  , levs  , _ , REG , SREF ) \
  __(E,LEVV  , levv  , _ , REG , REG)   \
  __(C,GTIV  , gtiv  , _ , IREF , REG ) \
  __(B,GTVI  , gtvi  , _ , REG , IREF ) \
  __(C,GTRV  , gtrv  , _ , RREF , REG ) \
  __(B,GTVR  , gtvr  , _ , REG , RREF ) \
  __(C,GTSV  , gtsv  , _ , SREF , REG ) \
  __(B,GTVS  , gtvs  , _ , REG , SREF ) \
  __(E,GTVV  , gtvv  , _ , REG , REG )  \
  __(C,GEIV  , geiv  , _ , IREF , REG ) \
  __(B,GEVI  , gevi  , _ , REG , IREF ) \
  __(C,GERV  , gerv  , _ , RREF , REG ) \
  __(B,GEVR  , gevr  , _ , REG , RREF ) \
  __(C,GESV  , gesv  , _ , SREF , REG ) \
  __(B,GEVS  , gevs  , _ , REG , SREF ) \
  __(E,GEVV  , gevv  , _ , REG , REG )  \
  __(C,EQIV  , eqiv  , _ , IREF , REG ) \
  __(B,EQVI  , eqvi  , _ , IREF , REG ) \
  __(C,EQRV  , eqrv  , _ , RREF , REG ) \
  __(B,EQVR  , eqvr  , _ , REG  , RREF) \
  __(C,EQSV  , eqsv  , _ , SREF , REG ) \
  __(B,EQVS  , eqvs  , _ , REG , SREF ) \
  __(E,EQVV  , eqvv  , _ , REG  , REG)  \
  __(C,NEIV  , neiv  , _ , IREF , REG ) \
  __(B,NEVI  , nevi  , _ , REG , IREF ) \
  __(C,NERV  , nerv  , _ , RREF, REG  ) \
  __(B,NEVR  , nevr  , _ , REG , RREF ) \
  __(C,NESV  , nesv  , _ , SREF , REG ) \
  __(B,NEVS  , nevs  , _ , REG , SREF ) \
  __(E,NEVV  , nevv  , _ , REG , REG)   \
  /* unary */ \
  __(F,NEGATE, negate, REG,_,_) \
  __(F,NOT   , not_  , REG,_,_) \
  /* branch */ \
  __(B,JMPT , jmpt ,REG,PC,_   ) \
  __(B,JMPF , jmpf ,REG,PC,_   ) \
  __(G,AND  , and_ ,PC,_,_  )    \
  __(G,OR   , or_  ,PC,_,_  )    \
  __(G,JMP  , jmp  ,PC,_,_  )    \
  /* register move */ \
  __(E,MOVE , move , REG, REG, _ )              \
  /* constant loading */                        \
  __(B,LOADI , loadi , REG , IREF , _    )      \
  __(F,LOAD0 , load0 , REG , _ , _    )         \
  __(F,LOAD1 , load1 , REG , _ , _    )         \
  __(F,LOADN1, loadn1, REG , _ , _   )          \
  __(B,LOADR , loadr , REG , RREF , _    )      \
  __(B,LOADSTR,loadstr,REG , SREF , _   )       \
  __(F,LOADTRUE,loadtrue,REG , _ , _ )          \
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
  __(G,LOADCLS  , loadcls  , GARG , _  , _ )    \
  /* property/upvalue/global value */           \
  __(B,PROPGET,propget,REG,SREF,_) \
  __(B,PROPSET,propset,REG,SREF,_) \
  __(E,IDXGET ,idxget,REG,REG,_)   \
  __(D,IDXSET ,idxset,REG,REG,REG) \
  __(B,IDXGETI,idxgeti,REG,IREF,_) \
  __(B,UVGET  ,uvget ,REG,GARG,_)  \
  __(C,UVSET  ,uvset ,GARG,REG,_)  \
  __(C,GSET   ,gset  ,GARG,REG,_)  \
  __(B,GGET   ,gget  ,REG,GARG,_)  \
  /* subroutine */ \
  __(D,CALL, call , REG , BASE , NARG )         \
  __(D,TCALL, tcall, REG , BASE , NARG )        \
  __(X,RETNULL, retnull , _ , _ , _ )           \
  __(X,RET  , ret  , _ , _ , _ )                \
  /* forloop tag */ \
  __(B,FSTART,fstart,REG,PC,_)     \
  __(G,FEND  ,fend  ,PC,_,_)       \
  __(E,FORINC ,forinc ,REG,REG,_)  \
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
  TYPE_N,
  TYPE_X,

  SIZE_OF_BYTECODE_TYPE
};

const char* GetBytecodeName( Bytecode );
BytecodeType GetBytecodeType( Bytecode );

} // namespace interpreter
} // namespace lavascript

#endif // BYTECODE_H_
