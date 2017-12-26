#ifndef BYTECODE_H_
#define BYTECODE_H_
#include <cstdint>
#include <vector>
#include <algorithm>
#include "src/trace.h"


/**
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


// Bytecode's type
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

/**
 * It holds bytecode's static information and used internally
 * to do meta query against each bytecode
 */
class BytecodeUsage {
 public:
  static const int kMaxBytecodeArgumentSize = 4;

  enum {
    INPUT ,    // input register
    OUTPUT,    // output register
    INOUT ,    // input and output register

    RREF  ,    // real number reference
    SREF  ,    // string reference
    SSOREF,    // sso reference
    CLSREF,    // closure reference
    PC    ,    // address label
    IMM8  ,    // immdiate number 8 bits
    IMM16 ,    // immdiate number 16 bits
    BASE  ,    // base of a list of input registers , this *must* be input
    UNUSED     // unused register
  };

  BytecodeUsage( int arg1 , int arg2 , int arg3 , int arg4 , BytecodeType type ,
                                                             bool fb ):
    arg_       (),
    used_size_ (0),
    type_      (type),
    feedback_  (fb)
  {
    if(arg1 != UNUSED) ++used_size_;
    if(arg2 != UNUSED) ++used_size_;
    if(arg3 != UNUSED) ++used_size_;
    if(arg4 != UNUSED) ++used_size_;

    arg_[0] = arg1;
    arg_[1] = arg2;
    arg_[2] = arg3;
    arg_[3] = arg4;
  }

  BytecodeUsage():
    arg_       (),
    used_size_ (0),
    type_      (TYPE_X),
    feedback_  (false)
  {
    arg_[0] = arg_[1] = arg_[2] = arg_[3] = UNUSED;
  }

 public:
  int arg1() const { return arg_[0]; }
  int arg2() const { return arg_[1]; }
  int arg3() const { return arg_[2]; }

  // extra slot
  int arg4() const { return arg_[3]; }

  bool feedback() const { return feedback_; }

  BytecodeType type() const { return type_; }

  int used_size() const { return used_size_; }

 public:
  int GetArgument( int index ) {
    lava_debug(NORMAL,lava_verify(index >= 1 && index <= 4););
    return arg_[index-1];
  }

 private:
  int arg_[4];         // all argument's type
  int used_size_;      // type of argument 5
  BytecodeType type_;  // bytecode type
  bool feedback_;      // whether this bytecode supports feedback
};


/** NOTES: Order matters **/
#define LAVASCRIPT_BYTECODE_LIST(__) \
  /* arithmetic bytecode , if cannot hold , then spill */ \
  __(D,ADDRV , addrv , OUTPUT , RREF , INPUT , UNUSED , true ) \
  __(D,ADDVR , addvr , OUTPUT , INPUT , RREF , UNUSED , true ) \
  __(D,ADDVV , addvv , OUTPUT , INPUT , INPUT, UNUSED , true ) \
  __(D,SUBRV , subrv , OUTPUT , RREF , INPUT , UNUSED , true ) \
  __(D,SUBVR , subvr , OUTPUT , INPUT , RREF , UNUSED , true ) \
  __(D,SUBVV , subvv , OUTPUT , INPUT , INPUT, UNUSED , true ) \
  __(D,MULRV , mulrv , OUTPUT , RREF , INPUT , UNUSED , true ) \
  __(D,MULVR , mulvr , OUTPUT , INPUT , RREF,  UNUSED , true ) \
  __(D,MULVV , mulvv , OUTPUT , INPUT , INPUT ,UNUSED , true ) \
  __(D,DIVRV , divrv , OUTPUT , RREF , INPUT , UNUSED , true ) \
  __(D,DIVVR , divvr , OUTPUT , INPUT , RREF , UNUSED , true ) \
  __(D,DIVVV , divvv , OUTPUT , INPUT , INPUT ,UNUSED , true ) \
  __(D,MODVR , modvr , OUTPUT , INPUT , RREF , UNUSED , true ) \
  __(D,MODRV , modrv , OUTPUT , RREF , INPUT , UNUSED , true ) \
  __(D,MODVV , modvv , OUTPUT , INPUT , INPUT ,UNUSED , true ) \
  __(D,POWRV , powrv , OUTPUT , RREF, INPUT ,  UNUSED , true ) \
  __(D,POWVR , powvr , OUTPUT , INPUT , RREF , UNUSED , true ) \
  __(D,POWVV , powvv , OUTPUT , INPUT , INPUT ,UNUSED , true ) \
  /* comparison */ \
  __(D,LTRV  , ltrv  , OUTPUT , RREF , INPUT , UNUSED , true ) \
  __(D,LTVR  , ltvr  , OUTPUT , INPUT , RREF , UNUSED , true ) \
  __(D,LTVV  , ltvv  , OUTPUT , INPUT , INPUT, UNUSED , true ) \
  __(D,LERV  , lerv  , OUTPUT , RREF , INPUT , UNUSED , true ) \
  __(D,LEVR  , levr  , OUTPUT , INPUT , RREF , UNUSED , true ) \
  __(D,LEVV  , levv  , OUTPUT , INPUT , INPUT, UNUSED , true ) \
  __(D,GTRV  , gtrv  , OUTPUT , RREF  , INPUT, UNUSED , true ) \
  __(D,GTVR  , gtvr  , OUTPUT , INPUT , RREF , UNUSED , true ) \
  __(D,GTVV  , gtvv  , OUTPUT , INPUT , INPUT, UNUSED , true ) \
  __(D,GERV  , gerv  , OUTPUT , RREF ,  INPUT, UNUSED , true ) \
  __(D,GEVR  , gevr  , OUTPUT , INPUT , RREF , UNUSED , true ) \
  __(D,GEVV  , gevv  , OUTPUT , INPUT , INPUT, UNUSED , true ) \
  __(D,EQRV  , eqrv  , OUTPUT , RREF ,  INPUT, UNUSED , true ) \
  __(D,EQVR  , eqvr  , OUTPUT , INPUT , RREF,  UNUSED , true ) \
  __(D,EQSV  , eqsv  , OUTPUT , SREF  , INPUT, UNUSED , true ) \
  __(D,EQVS  , eqvs  , OUTPUT , INPUT , SREF , UNUSED , true ) \
  __(D,EQVV  , eqvv  , OUTPUT , INPUT , INPUT, UNUSED , true ) \
  __(D,NERV  , nerv  , OUTPUT , RREF  , INPUT, UNUSED , true ) \
  __(D,NEVR  , nevr  , OUTPUT , INPUT , RREF , UNUSED , true ) \
  __(D,NESV  , nesv  , OUTPUT , SREF  , INPUT, UNUSED , true ) \
  __(D,NEVS  , nevs  , OUTPUT , INPUT , SREF , UNUSED , true ) \
  __(D,NEVV  , nevv  , OUTPUT , INPUT , INPUT, UNUSED , true ) \
  /* unary */ \
  __(E,NEGATE, negate, OUTPUT , INPUT , UNUSED, UNUSED, true ) \
  __(E,NOT   , not_  , OUTPUT , INPUT , UNUSED, UNUSED, true ) \
  /* branch */ \
  __(B,JMPT , jmpt   , INPUT  , PC    , UNUSED, UNUSED, true ) \
  __(B,JMPF , jmpf   , INPUT  , PC    , UNUSED, UNUSED, true ) \
  __(H,TERN , tern   , INPUT  , OUTPUT, UNUSED, PC    , true ) \
  __(H,AND  , and_   , INPUT  , OUTPUT, UNUSED, PC    , true ) \
  __(H,OR   , or_    , INPUT  , OUTPUT, UNUSED, PC    , true ) \
  __(G,JMP  , jmp    , PC     , UNUSED, UNUSED, UNUSED, false) \
  /* register move */ \
  __(E,MOVE , move   , OUTPUT , INPUT , UNUSED, UNUSED, false) \
  /* constant loading */                            \
  __(F,LOAD0 , load0 , OUTPUT , UNUSED, UNUSED, UNUSED, false) \
  __(F,LOAD1 , load1 , OUTPUT , UNUSED, UNUSED, UNUSED, false) \
  __(F,LOADN1, loadn1, OUTPUT , UNUSED, UNUSED, UNUSED, false) \
  __(E,LOADR , loadr , OUTPUT , RREF  , UNUSED, UNUSED, false) \
  __(E,LOADSTR,loadstr,OUTPUT , SREF  , UNUSED, UNUSED, false) \
  __(F,LOADTRUE,loadtrue,OUTPUT,UNUSED, UNUSED, UNUSED, false) \
  __(F,LOADFALSE,loadfalse,OUTPUT,UNUSED,UNUSED,UNUSED, false) \
  __(F,LOADNULL , loadnull,OUTPUT,UNUSED,UNUSED,UNUSED, false) \
  __(F,LOADLIST0, loadlist0,OUTPUT,UNUSED,UNUSED,UNUSED,false) \
  __(E,LOADLIST1, loadlist1,OUTPUT,INPUT,UNUSED, UNUSED,false) \
  __(D,LOADLIST2, loadlist2,OUTPUT,INPUT,INPUT , UNUSED,false) \
  __(B,NEWLIST, newlist,OUTPUT,IMM8   , UNUSED , UNUSED,false) \
  __(D,ADDLIST, addlist,OUTPUT,BASE   , IMM8   , UNUSED,false) \
  __(F,LOADOBJ0,loadobj0,OUTPUT,UNUSED, UNUSED , UNUSED,false) \
  __(D,LOADOBJ1,loadobj1,OUTPUT,INPUT , INPUT  , UNUSED,false) \
  __(B,NEWOBJ  ,newobj  ,OUTPUT,IMM8  , UNUSED , UNUSED,false) \
  __(D,ADDOBJ  ,addobj , OUTPUT,INPUT , INPUT  , UNUSED,false) \
  __(B,LOADCLS ,loadcls, OUTPUT,CLSREF, UNUSED , UNUSED,false) \
  __(G,INITCLS ,initcls, OUTPUT,UNUSED, UNUSED , UNUSED,false) \
  /* property/upvalue/global value */              \
  __(D,PROPGET,propget,OUTPUT, SREF   , UNUSED , UNUSED,true ) \
  __(D,PROPSET,propset,INPUT , SREF   , INPUT  , UNUSED,true ) \
  __(D,PROPGETSSO,propgetsso,OUTPUT,SSOREF,UNUSED,UNUSED,true) \
  __(D,PROPSETSSO,propsetsso,INPUT ,SSOREF,INPUT ,UNUSED,true) \
  __(D,IDXGET ,idxget,OUTPUT,INPUT    , INPUT  , UNUSED ,true) \
  __(D,IDXSET ,idxset,INPUT , INPUT   , INPUT  , UNUSED, true) \
  __(D,IDXSETI,idxseti,INPUT, IMM8    , INPUT  , UNUSED, true) \
  __(D,IDXGETI,idxgeti,OUTPUT, INPUT  , IMM8   , UNUSED, true) \
  __(E,UVGET  ,uvget , OUTPUT, IMM8   , UNUSED , UNUSED,false) \
  __(E,UVSET  ,uvset , IMM8  , INPUT  , UNUSED , UNUSED,false) \
  __(E,GGET   ,gget  , OUTPUT, SREF   , UNUSED , UNUSED,false) \
  __(E,GGETSSO,ggetsso,OUTPUT, SSOREF , UNUSED , UNUSED,false) \
  __(E,GSET   ,gset  , SREF  , INPUT  , UNUSED , UNUSED,false) \
  __(E,GSETSSO,gsetsso,SSOREF, INPUT  , UNUSED , UNUSED,false) \
  /* subroutine */ \
  __(D,CALL,   call  , INPUT , BASE   , IMM8   , UNUSED,true ) \
  __(D,TCALL, tcall  , INPUT , BASE   , IMM8   , UNUSED,true ) \
  __(X,RETNULL,retnull, UNUSED,UNUSED , UNUSED , UNUSED,false) \
  __(X,RET  , ret    , UNUSED, UNUSED , UNUSED , UNUSED,false) \
  /* forloop tag */ \
  __(B,FSTART,fstart,  OUTPUT, PC     , UNUSED , UNUSED,true ) \
  __(H,FEND1,fend1  ,  INPUT , INPUT  , UNUSED , PC    ,true ) \
  __(H,FEND2,fend2  ,  INPUT , INPUT  , INPUT  , PC    ,true ) \
  __(X,FEVRSTART,fevrstart,UNUSED,UNUSED,UNUSED,UNUSED ,false) \
  /* fevrend also has feedback , thouth it is empty, we need it */ \
  /* simply because we can use the fevrend to stop a profile trace */ \
  /* and kicks in the actual compilation job */  \
  __(G,FEVREND,fevrend,PC    , UNUSED , UNUSED , UNUSED,false) \
  __(B,FESTART,festart,INPUT , PC     , UNUSED , UNUSED,true ) \
  __(B,FEEND  ,feend  ,INPUT , PC     , UNUSED , UNUSED,true ) \
  __(D,IDREF  ,idref  ,INPUT , OUTPUT , OUTPUT , UNUSED,false) \
  __(G,BRK   ,brk     ,PC    , UNUSED , UNUSED , UNUSED,false) \
  __(G,CONT  ,cont    ,PC    , UNUSED , UNUSED , UNUSED,false) \
  /* always the last one */\
  __(X,HLT   ,hlt     ,UNUSED, UNUSED , UNUSED , UNUSED,false)

// Used to emit IdxGetI instruction
static const std::size_t kIdxGetIMaxImm = 256; // 2^8

// Bytecode enumeration
enum Bytecode {
#define __(A,B,...) BC_##B,
  LAVASCRIPT_BYTECODE_LIST(__)
  SIZE_OF_BYTECODE
#undef __ // __
};

static_assert( SIZE_OF_BYTECODE <= 255 );

// Get the byteocde's human readable name
const char* GetBytecodeName( Bytecode );

// Get bytecode usage information
const BytecodeUsage& GetBytecodeUsage( Bytecode );

// Check whether feedback is needed for this bytecode
inline bool DoesBytecodeHasFeedback( Bytecode bc ) {
  return GetBytecodeUsage(bc).feedback();
}

// Get the bytecode's type information
inline BytecodeType GetBytecodeType( Bytecode bc ) {
  return GetBytecodeUsage(bc).type();
}

// Get the byteode type's human printable name
const char* GetBytecodeTypeName( BytecodeType );

inline const char* GetBytecodeTypeName( Bytecode bc ) {
  BytecodeType t = GetBytecodeType(bc);
  return GetBytecodeTypeName(t);
}

} // namespace interpreter
} // namespace lavascript

#endif // BYTECODE_H_
