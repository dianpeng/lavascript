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
  __(D,ADDRV , addrv , REG , RREF , REG , FB ) \
  __(D,ADDVR , addvr , REG , REG , RREF , FB ) \
  __(D,ADDVV , addvv , REG , REG , REG  , FB ) \
  __(D,SUBRV , subrv , REG , RREF , REG , FB ) \
  __(D,SUBVR , subvr , REG , REG  , RREF, FB ) \
  __(D,SUBVV , subvv , REG , REG , REG  , FB ) \
  __(D,MULRV , mulrv , REG , RREF , REG , FB ) \
  __(D,MULVR , mulvr , REG , REG  , RREF, FB ) \
  __(D,MULVV , mulvv , REG , REG , REG  , FB ) \
  __(D,DIVRV , divrv , REG , RREF , REG , FB ) \
  __(D,DIVVR , divvr , REG , REG , RREF , FB ) \
  __(D,DIVVV , divvv , REG , REG , REG  , FB ) \
  __(D,MODVR , modvr , REG , REG , REG  , FB ) \
  __(D,MODRV , modrv , REG , REG , REG  , FB ) \
  __(D,MODVV , modvv , REG , REG , REG  , FB ) \
  __(D,POWRV , powrv , REG , RREF , REG , FB ) \
  __(D,POWVR , powvr , REG , REG , RREF , FB ) \
  __(D,POWVV , powvv , REG , REG , REG  , FB ) \
  /* comparison */                        \
  __(D,LTRV  , ltrv  , REG , RREF, REG  , FB ) \
  __(D,LTVR  , ltvr  , REG , REG , RREF , FB ) \
  __(D,LTVV  , ltvv  , REG , REG , REG  , FB ) \
  __(D,LERV  , lerv  , REG , RREF , REG , FB ) \
  __(D,LEVR  , levr  , REG , REG , RREF , FB ) \
  __(D,LEVV  , levv  , REG , REG , REG  , FB ) \
  __(D,GTRV  , gtrv  , REG , RREF , REG , FB ) \
  __(D,GTVR  , gtvr  , REG , REG , RREF , FB ) \
  __(D,GTVV  , gtvv  , REG , REG , REG  , FB ) \
  __(D,GERV  , gerv  , REG , RREF , REG , FB ) \
  __(D,GEVR  , gevr  , REG , REG , RREF , FB ) \
  __(D,GEVV  , gevv  , REG , REG , REG  , FB ) \
  __(D,EQRV  , eqrv  , REG , RREF , REG , FB ) \
  __(D,EQVR  , eqvr  , REG , REG  , RREF, FB ) \
  __(D,EQSV  , eqsv  , REG , SREF , REG , FB ) \
  __(D,EQVS  , eqvs  , REG , REG , SREF , FB ) \
  __(D,EQVV  , eqvv  , REG , REG  , REG , FB ) \
  __(D,NERV  , nerv  , REG , RREF, REG  , FB ) \
  __(D,NEVR  , nevr  , REG , REG , RREF , FB ) \
  __(D,NESV  , nesv  , REG , SREF , REG , FB ) \
  __(D,NEVS  , nevs  , REG , REG , SREF , FB ) \
  __(D,NEVV  , nevv  , REG , REG , REG  , FB ) \
  /* unary */ \
  __(E,NEGATE, negate, REG,REG,_ , FB )        \
  __(E,NOT   , not_  , REG,REG,_ , FB )        \
  /* branch */ \
  __(B,JMPF , jmpf ,REG,PC,_, _  )             \
  __(B,AND  , and_ ,REG,PC,_, _  )             \
  __(B,OR   , or_  ,REG,PC,_, _  )             \
  __(G,JMP  , jmp  ,PC,_,_  , _  )             \
  /* register move */ \
  __(E,MOVE , move , REG, REG, _, _)                \
  /* constant loading */                            \
  __(F,LOAD0 , load0 , REG , _ , _ , _ )            \
  __(F,LOAD1 , load1 , REG , _ , _ , _ )            \
  __(F,LOADN1, loadn1, REG , _ , _ , _ )            \
  __(E,LOADR , loadr , REG , RREF , _ , _ )         \
  __(E,LOADSTR,loadstr,REG , SREF , _ , _ )         \
  __(F,LOADTRUE,loadtrue,REG , _ , _  , _ )         \
  __(F,LOADFALSE,loadfalse,  REG, _ , _ , _ )       \
  __(F,LOADNULL , loadnull , REG , _ , _ , _ )      \
  __(F,LOADLIST0, loadlist0, REG , _ , _ , _ )      \
  __(E,LOADLIST1, loadlist1, REG , REG , _ , _ )    \
  __(D,LOADLIST2, loadlist2, REG , REG , REG , _ )  \
  __(B,NEWLIST, newlist, REG , NARG  , _ , _ )      \
  __(D,ADDLIST, addlist, REG , REG , NARG, _ )      \
  __(F,LOADOBJ0 , loadobj0 , REG , _ , _ , _ )      \
  __(D,LOADOBJ1, loadobj1, REG , REG , REG , _ )    \
  __(B,NEWOBJ  , newobj , REG , NARG  , _  , _ )    \
  __(D,ADDOBJ  , addobj , REG , REG , REG , _ )     \
  __(B,LOADCLS  , loadcls  , REG , GARG , _ , _ )   \
  __(G,INITCLS , initcls, GARG , _ , _ , _ )        \
  /* property/upvalue/global value */              \
  __(D,PROPGET,propget,REG,REG,SRE,FB)             \
  __(D,PROPSET,propset,REG,SREF,REG,FB)            \
  __(D,PROPGETSSO,propgetsso,REG,REG,SSO,FB)       \
  __(D,PROPSETSSO,propsetsso,REG,SSO,REG,FB)       \
  __(D,IDXGET ,idxget,REG,REG,REG,FB)              \
  __(D,IDXSET ,idxset,REG,REG,REG,FB)              \
  __(D,IDXSETI,idxseti,REG,IMM,REG,FB)             \
  __(D,IDXGETI,idxgeti,REG,REG,IMM,FB)             \
  __(E,UVGET  ,uvget ,REG,GARG,_,_)               \
  __(E,UVSET  ,uvset ,GARG,REG,_,_)               \
  __(E,GGET   ,gget  ,REG,SREF,_,_)               \
  __(E,GGETSSO,ggetsso,RREG,SSO,_,_)              \
  __(E,GSET   ,gset  ,SREF,REG,_,_)               \
  __(E,GSETSSO,gsetsso,REG,SSO,_,_)               \
  /* subroutine */ \
  __(D,CALL, call , REG , BASE , NARG ,FB)        \
  __(D,TCALL, tcall, REG , BASE , NARG,FB)        \
  __(X,RETNULL, retnull , _ , _ , _ ,_)           \
  __(X,RET  , ret  , _ , _ , _ ,_ )               \
  /* forloop tag */ \
  __(B,FSTART,fstart,REG,PC,_,FB)                \
  __(H,FEND1,fend1 ,REG,REG,_,FB)                \
  __(H,FEND2,fend2 ,REG,REG,REG,FB)              \
  __(G,FEVRSTART,fevrstart,PC,_,_,_)             \
  /* fevrend also has feedback , thouth it is empty, we need it */ \
  /* simply because we can use the fevrend to stop a profile trace */ \
  /* and kicks in the actual compilation job */  \
  __(G,FEVREND,fevrend,PC,_,_ ,FB)               \
  __(B,FESTART,festart,REG,PC,_,FB)              \
  __(B,FEEND  ,feend  ,REG,PC,_,FB)              \
  __(D,IDREF  ,idref  ,REG,REG,REG,_)            \
  __(G,BRK   ,brk,PC,_,_,_)                      \
  __(G,CONT  ,cont,PC,_,_,_)                     \
  /* always the last one */\
  __(X,HLT,hlt,_,_,_,_)

// Used to emit IdxGetI instruction
static const std::size_t kIdxGetIMaxImm = 256; // 2^8

/** bytecode **/
enum Bytecode {
#define __(A,B,...) BC_##B,
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

// Check whether feedback is needed for this bytecode
bool DoesBytecodeHasFeedback( Bytecode );

} // namespace interpreter
} // namespace lavascript

#endif // BYTECODE_H_
