/*
** This file has been pre-processed with DynASM.
** http://luajit.org/dynasm.html
** DynASM version 1.3.0, DynASM x64 version 1.3.0
** DO NOT EDIT! The original file is in "src/interpreter/bytecode-interpreter.dasc".
*/

#line 1 "src/interpreter/bytecode-interpreter.dasc"
#include "bytecode-interpreter.h"
#include "interpreter-frame.h"
#include "interpreter-runtime.h"

#include "src/context.h"
#include "src/trace.h"
#include "src/os.h"
#include "src/config.h"

#include <algorithm>
#include <map>
#include <cassert>
#include <climits>
#include <Zydis/Zydis.h>

// This the C symbol we use to resolve certain runtime function. Ideally these
// functions *should* be replaced with carefully tuned inline assembly but we
// could just call this out for current phase/stage. Later on to work at better
// version of these functions
extern "C" {
double pow(double,double);
} // extern "C"

namespace lavascript {
namespace interpreter{

inline void SetValueFlag( Value* v , std::uint32_t flag ) {
  v->raw_ = static_cast<std::uint64_t>(flag) << 32;
}

inline std::uint32_t GetValueFlag( const Value& v ) {
  return static_cast<std::uint32_t>(v.raw_ >>32);
}

namespace {

// Used in dynasm library
int ResolveExternAddress( void**,unsigned char*,int,int );

// Workaround for ODR
#include "dep/dynasm/dasm_proto.h"

#define DASM_EXTERN_FUNC(a,b,c,d) ResolveExternAddress((void**)a,b,c,d)
#include "dep/dynasm/dasm_x86.h"

// -------------------------------------------------------------
// BuildContext
//
// Build phase context, used to *generate* templated interpreter
// -------------------------------------------------------------
struct BuildContext {
  dasm_State* dasm_ctx;
  int tag;

  BuildContext():
    dasm_ctx(NULL),
    tag(0)
  {}

  ~BuildContext() {
    if(dasm_ctx) dasm_free(&dasm_ctx);
  }
};


// Shut the GCC's mouth fucked up
template< typename T >
int HorribleCast( T* ptr ) {
  std::uint64_t iptr = reinterpret_cast<std::uint64_t>(ptr);
  int ret = static_cast<int>(iptr);
  lava_verify(reinterpret_cast<T*>(ret) == ptr);
  return ret;
}


// ------------------------------------------------------------------
// Prototype for the main interpreter function
//
// @ARG1: runtime
// @ARG2: Prototype** of the function
// @ARG3: start of the stack
// @ARG4: start of the code buffer for the *Prototype*
// @ARG5: start of the dispatch table
typedef bool (*Main)(Runtime*,Prototype**,void*,void*,void*);

// ------------------------------------------------------------------
//
// Helper function/macros to register its literal name into a global
// table to help resolve the function's address during assembly link
// phase
//
// ------------------------------------------------------------------
typedef std::map<std::string,void*> ExternSymbolTable;

ExternSymbolTable* GetExternSymbolTable() {
  static ExternSymbolTable kTable;
  return &kTable;
}

// Macro to register a external function's symbol name into global table
#define INTERPRETER_REGISTER_EXTERN_SYMBOL(XX)                               \
  struct XX##_Registry {                                                     \
    XX##_Registry() {                                                        \
      ExternSymbolTable* table = GetExternSymbolTable();                     \
      table->insert(std::make_pair(#XX,reinterpret_cast<void*>(&XX)));       \
    }                                                                        \
  };                                                                         \
  static XX##_Registry k##XX##_Registry;


// ------------------------------------------------------------------
// Builtin libc function exposure section
// ------------------------------------------------------------------
INTERPRETER_REGISTER_EXTERN_SYMBOL(pow)


// -------------------------------------------------------------------
// Helper to set Value object to indicate special meaning
// -------------------------------------------------------------------
#define VALUE_FAIL Value::FLAG_1

/* --------------------------------------------------------------------------
 *
 * Interpreter C++ Part Function Implementation
 *
 * -------------------------------------------------------------------------*/
inline Bytecode PrevOpcode( Runtime* sandbox ) {
  std::uint32_t pbc = sandbox->cur_pc[-1];
  return static_cast<Bytecode>( pbc & 0xff );
}

// --------------------------------------------------------------------------
// Arithmetic Helper
// --------------------------------------------------------------------------
Value InterpreterDoArithmetic( Runtime* sandbox ,
                               Value left ,
                               Value right ) {
  (void)sandbox;
  (void)left;
  (void)right;
  {
    Value r;
    SetValueFlag( &r, VALUE_FAIL );
    return r;
  }
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterDoArithmetic)

Value InterpreterPow         ( Runtime* sandbox ,
                               Value left,
                               Value right ,
                               Value* output ) {
  (void)sandbox;
  (void)left;
  (void)right;
  (void)output;
  {
    Value r;
    SetValueFlag( &r, VALUE_FAIL );
    return r;
  }
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterPow)

void InterpreterDivByZero    ( Runtime* sandbox , std::uint32_t* pc ) {
  (void)sandbox;
  (void)pc;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterDivByZero)


// ---------------------------------------------------------------------------
// Comparison Helper
// ---------------------------------------------------------------------------
Value InterpreterDoCompare  ( Runtime* sandbox , Value left ,
                                                 Value right ) {
  (void)sandbox;
  (void)left;
  (void)right;
  {
    Value r;
    SetValueFlag( &r, VALUE_FAIL );
    return r;
  }
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterDoCompare)

// ----------------------------------------------------------------------------
// Unary Helper
// ----------------------------------------------------------------------------
bool InterpreterDoNegate   ( Runtime* sandbox , const Value& operand ,
                                                Value* result ) {
  (void)sandbox;
  (void)operand;
  (void)result;
  return false;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterDoNegate)

// ----------------------------------------------------------------------------
// Jump Helper
// ----------------------------------------------------------------------------
void* InterpreterDoCondJmpT  ( Runtime* sandbox , Value condition ,
                                                  std::uint32_t where ,
                                                  void* start_of_pc ) {
  (void)sandbox;
  (void)condition;
  (void)where;
  (void)start_of_pc;
  return NULL;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterDoCondJmpT)

void* InterpreterDoCondJmpF  ( Runtime* sandbox , Value condition ,
                                                  std::uint32_t where ,
                                                  void* start_of_pc ) {
  (void)sandbox;
  (void)condition;
  (void)where;
  (void)start_of_pc;
  return NULL;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterDoCondJmpF)


void* InterpreterDoCondAnd   ( Runtime* sandbox , Value condition ,
                                                  std::uint32_t where ,
                                                  void* start_of_pc ) {
  (void)sandbox;
  (void)condition;
  (void)where;
  (void)start_of_pc;
  return NULL;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterDoCondAnd)

void* InterpreterDoCondOr    ( Runtime* sandbox , Value condition ,
                                                  std::uint32_t where ,
                                                  void* start_of_pc ) {
  (void)sandbox;
  (void)condition;
  (void)where;
  (void)start_of_pc;
  return NULL;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterDoCondOr)

// ----------------------------------------------------------------------------
// Literal Loader Helper
// ----------------------------------------------------------------------------
bool InterpreterDoLoadList0  ( Runtime* sandbox , Value* output ) {
  (void)sandbox;
  (void)output;
  return false;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterDoLoadList0)

bool InterpreterDoLoadList1  ( Runtime* sandbox , Value* output ,
                                                  Value e1 ) {
  (void)sandbox;
  (void)output;
  (void)e1;
  return false;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterDoLoadList1)

bool InterpreterDoLoadList2  ( Runtime* sandbox , Value* output ,
                                                  Value e1,
                                                  Value e2 ) {
  (void)sandbox;
  (void)output;
  (void)e1;
  (void)e2;
  return false;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterDoLoadList2)

bool InterpreterDoNewList   ( Runtime* sandbox , Value* output ,
                                                  std::uint32_t narg ) {
  (void)sandbox;
  (void)output;
  (void)narg;
  return false;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterDoNewList)

bool InterpreterDoAddList   ( Runtime* sandbox , Value* output ,
                                                 Value  val ) {
  (void)sandbox;
  (void)output;
  (void)val;
  return false;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterDoAddList)

bool InterpreterDoLoadObj0   ( Runtime* sandbox , Value* output ) {
  (void)sandbox;
  (void)output;
  return false;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterDoLoadObj0)

bool InterpreterDoLoadObj1   ( Runtime* sandbox , Value* output ,
                                                  Value  key,
                                                  Value  val ) {
  (void)sandbox;
  (void)output;
  (void)key;
  (void)val;
  return false;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterDoLoadObj1)

bool InterpreterDoNewObj    ( Runtime* sandbox , Value* output ,
                                                 std::uint32_t narg ) {
  (void)sandbox;
  (void)output;
  (void)narg;
  return false;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterDoNewObj)

bool InterpreterDoAddObj    ( Runtime* sandbox , Value* output ,
                                                 Value  key,
                                                 Value  val ) {
  (void)sandbox;
  (void)output;
  (void)key;
  (void)val;
  return false;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterDoAddObj)

bool InterpreterDoLoadCls   ( Runtime* sandbox , Value* start_of_stack ,
                                                 std::uint32_t prototype_id ,
                                                 Value* dest ) {
  (void)sandbox;
  (void)start_of_stack;
  (void)prototype_id;
  (void)dest;
  return false;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterDoLoadCls)

// ----------------------------------------------------------------------------
// Property Get/Set
// ----------------------------------------------------------------------------
Value InterpreterDoPropGet   ( Runtime* sandbox , Value obj , String** key ) {
  (void)sandbox;
  (void)obj;
  (void)key;
  {
    Value r;
    SetValueFlag( &r, VALUE_FAIL );
    return r;
  }
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterDoPropGet)

void InterpreterPropGetNotFound( Runtime* sandbox , Value obj , String** key ) {
  (void)sandbox;
  (void)obj;
  (void)key;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterPropGetNotFound)

void InterpreterPropGetNotObject( Runtime* sandbox, Value obj , String** key ) {
  (void)sandbox;
  (void)obj;
  (void)key;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterPropGetNotObject)

bool InterpreterDoPropSet      ( Runtime* sandbox , Value obj , String** key ,
                                                                Value val ) {
  (void)sandbox;
  (void)obj;
  (void)key;
  (void)val;
  return false;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterDoPropSet)

Value InterpreterDoIdxGet      ( Runtime* sandbox , Value obj , Value key ) {
  (void)sandbox;
  (void)obj;
  (void)key;
  {
    Value r;
    SetValueFlag( &r, VALUE_FAIL );
    return r;
  }
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterDoIdxGet)

Value InterpreterDoIdxGetI     ( Runtime* sandbox , Value obj , std::int32_t idx ) {
  (void)sandbox;
  (void)obj;
  (void)idx;
  {
    Value r;
    SetValueFlag( &r, VALUE_FAIL );
    return r;
  }
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterDoIdxGetI)

bool InterpreterDoIdxSet      ( Runtime* sandbox , Value obj , Value key ,
                                                               Value val ) {
  (void)sandbox;
  (void)obj;
  (void)key;
  (void)val;
  return false;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterDoIdxSet)

bool InterpreterDoGGet       ( Runtime* sandbox , Value* output , String** key ) {
  (void)sandbox;
  (void)key;
  (void)output;
  return false;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterDoGGet)

bool InterpreterDoGSet       ( Runtime* sandbox , String** key , Value value ) {
  (void)sandbox;
  (void)key;
  (void)value;
  return false;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterDoGSet)


// ----------------------------------------------------------------------------
// Loop
// ----------------------------------------------------------------------------
bool InterpreterForEnd1     ( Runtime* sandbox , const Value& lhs , const Value& rhs ,
                                                                    std::uint32_t offset ) {
  (void)sandbox;
  (void)lhs;
  (void)rhs;
  return false;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterForEnd1)

bool InterpreterForEnd2     ( Runtime* sandbox , const Value& lhs , const Value& rhs ,
                                                                    const Value& cond ,
                                                                    std::uint32_t offset ) {
  (void)sandbox;
  (void)lhs;
  (void)rhs;
  return false;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterForEnd2)


/* ---------------------------------------------------------------------
 *
 * Implementation of AssemblyIntepreter
 *
 * --------------------------------------------------------------------*/
//|.arch x64
#if DASM_VERSION != 10300
#error "Version mismatch between DynASM and included encoding engine"
#endif
#line 464 "src/interpreter/bytecode-interpreter.dasc"
//|.actionlist actions
static const unsigned char actions[5494] = {
  254,1,248,10,237,237,255,248,11,237,237,255,248,12,237,255,248,13,237,255,
  248,14,0,0,0,0,0,0,0,0,255,254,0,249,248,15,255,72,131,252,236,72,76,137,
  100,36,40,76,137,108,36,32,76,137,116,36,24,76,137,124,36,16,72,137,108,36,
  8,72,137,92,36,48,255,73,137,252,252,73,137,252,245,73,137,214,72,137,205,
  77,137,199,255,72,137,12,36,255,139,69,0,72,15,182,200,72,137,207,255,232,
  251,1,0,255,72,184,237,237,252,255,208,255,139,69,0,72,15,182,200,72,131,
  197,4,193,232,8,65,252,255,36,207,255,249,248,16,49,192,76,139,100,36,40,
  76,139,108,36,32,76,139,116,36,24,76,139,124,36,16,72,139,108,36,8,72,139,
  92,36,48,72,131,196,72,195,255,249,248,17,73,139,134,252,248,7,0,0,73,137,
  132,253,36,233,72,199,192,1,0,0,0,255,249,248,18,73,137,172,253,36,233,76,
  137,231,77,139,93,0,73,139,180,253,219,233,255,232,251,1,1,255,73,137,195,
  73,193,252,235,32,73,129,252,251,239,15,132,244,16,73,137,134,252,248,7,0,
  0,139,69,0,72,15,182,200,72,137,207,255,249,248,19,73,137,172,253,36,233,
  76,137,231,77,139,93,0,73,139,148,253,195,233,255,249,248,20,73,137,172,253,
  36,233,76,137,231,73,139,52,222,73,139,20,198,255,249,248,21,73,137,172,253,
  36,233,76,137,231,77,139,93,0,73,139,180,253,195,233,73,139,20,206,73,141,
  12,222,255,232,251,1,2,255,249,248,22,73,137,172,253,36,233,76,137,231,73,
  139,52,198,77,139,93,0,73,139,148,253,203,233,73,141,12,222,255,249,248,23,
  73,137,172,253,36,233,72,15,182,216,193,232,8,15,182,204,37,252,255,0,0,0,
  76,137,231,73,139,52,198,73,139,20,206,73,141,12,222,255,249,248,24,76,137,
  231,72,141,117,252,252,255,232,251,1,3,255,252,233,244,16,255,249,248,25,
  73,137,172,253,36,233,76,137,231,77,139,93,0,73,139,180,253,195,233,73,139,
  20,198,255,232,251,1,4,255,249,248,26,73,137,172,253,36,233,76,137,231,73,
  139,52,222,77,139,93,0,73,139,148,253,195,233,255,249,248,27,73,137,172,253,
  36,233,76,137,231,73,139,52,222,73,139,20,198,255,249,248,28,73,137,172,253,
  36,233,76,137,231,73,139,52,222,77,139,93,0,77,139,147,233,73,139,20,194,
  255,232,251,1,5,255,249,248,29,73,137,172,253,36,233,76,137,231,73,139,52,
  222,77,139,93,0,77,139,147,233,73,139,20,218,255,232,251,1,6,255,249,248,
  30,73,137,172,253,36,233,76,137,231,73,139,52,222,77,139,93,0,77,139,147,
  233,73,139,20,194,255,232,251,1,7,255,249,248,31,73,137,172,253,36,233,76,
  137,231,73,139,52,222,137,194,255,232,251,1,8,255,133,192,15,132,244,16,139,
  69,0,72,15,182,200,72,137,207,255,249,65,199,134,252,252,7,0,0,237,252,233,
  244,17,255,249,252,233,244,17,255,249,15,182,216,193,232,8,73,139,12,198,
  73,137,12,222,139,69,0,72,15,182,200,72,137,207,255,249,15,182,216,102,15,
  87,192,252,242,65,15,17,4,222,139,69,0,72,15,182,200,72,137,207,255,249,15,
  182,216,73,187,237,237,102,73,15,110,195,252,242,65,15,17,4,222,139,69,0,
  72,15,182,200,72,137,207,255,249,15,182,216,193,232,8,77,139,93,0,252,242,
  65,15,16,132,253,195,233,252,242,65,15,17,4,222,139,69,0,72,15,182,200,72,
  137,207,255,249,15,182,216,65,199,68,222,4,237,139,69,0,72,15,182,200,72,
  137,207,255,249,15,182,216,193,232,8,77,139,93,0,77,139,147,233,73,139,52,
  194,72,11,53,244,10,73,137,52,222,139,69,0,72,15,182,200,72,137,207,255,249,
  15,182,216,73,137,172,253,36,233,76,137,231,73,141,52,222,255,232,251,1,9,
  255,249,15,182,216,193,232,8,73,137,172,253,36,233,76,137,231,73,141,52,222,
  73,139,20,198,255,232,251,1,10,255,249,72,15,182,216,193,232,8,15,182,204,
  37,252,255,0,0,0,73,137,172,253,36,233,76,137,231,73,141,52,222,73,139,20,
  198,73,139,12,206,255,232,251,1,11,255,249,252,233,244,32,72,15,182,216,193,
  232,8,73,137,172,253,36,233,76,137,231,73,141,52,222,137,194,255,232,251,
  1,12,255,232,251,1,13,255,232,251,1,14,255,232,251,1,15,255,249,72,15,182,
  216,193,232,8,73,137,172,253,36,233,76,137,231,73,141,52,222,137,194,255,
  232,251,1,16,255,232,251,1,17,255,249,72,15,183,216,193,232,16,73,137,172,
  253,36,233,76,137,231,76,137,252,246,137,194,73,141,12,222,255,232,251,1,
  18,255,249,72,15,182,216,193,232,8,15,182,204,37,252,255,0,0,0,73,139,20,
  206,72,137,215,72,193,252,239,32,129,252,255,239,15,131,244,18,77,139,93,
  0,252,242,65,15,16,132,253,195,233,102,72,15,110,202,252,242,15,88,193,252,
  242,65,15,17,4,222,139,69,0,72,15,182,200,72,137,207,255,249,72,15,182,216,
  193,232,8,15,182,204,37,252,255,0,0,0,73,139,20,206,72,137,215,72,193,252,
  239,32,129,252,255,239,15,131,244,18,77,139,93,0,252,242,65,15,16,132,253,
  195,233,102,72,15,110,202,252,242,15,92,193,252,242,65,15,17,4,222,139,69,
  0,72,15,182,200,72,137,207,255,249,72,15,182,216,193,232,8,15,182,204,37,
  252,255,0,0,0,73,139,20,206,72,137,215,72,193,252,239,32,129,252,255,239,
  15,131,244,18,77,139,93,0,252,242,65,15,16,132,253,195,233,102,72,15,110,
  202,252,242,15,89,193,252,242,65,15,17,4,222,139,69,0,72,15,182,200,72,137,
  207,255,249,72,15,182,216,193,232,8,15,182,204,37,252,255,0,0,0,73,139,20,
  206,72,137,215,72,193,252,239,32,129,252,255,239,15,131,244,18,77,139,93,
  0,252,242,65,15,16,132,253,195,233,102,72,15,110,202,252,242,15,94,193,252,
  242,65,15,17,4,222,139,69,0,72,15,182,200,72,137,207,255,249,72,15,182,216,
  193,232,8,15,182,204,37,252,255,0,0,0,73,139,52,198,72,137,252,247,72,193,
  252,239,32,129,252,255,239,15,131,244,19,102,72,15,110,198,77,139,93,0,252,
  242,65,15,16,140,253,203,233,252,242,15,88,193,252,242,65,15,17,4,222,139,
  69,0,72,15,182,200,72,137,207,255,249,72,15,182,216,193,232,8,15,182,204,
  37,252,255,0,0,0,73,139,52,198,72,137,252,247,72,193,252,239,32,129,252,255,
  239,15,131,244,19,102,72,15,110,198,77,139,93,0,252,242,65,15,16,140,253,
  203,233,252,242,15,92,193,252,242,65,15,17,4,222,139,69,0,72,15,182,200,72,
  137,207,255,249,72,15,182,216,193,232,8,15,182,204,37,252,255,0,0,0,73,139,
  52,198,72,137,252,247,72,193,252,239,32,129,252,255,239,15,131,244,19,102,
  72,15,110,198,77,139,93,0,252,242,65,15,16,140,253,203,233,252,242,15,89,
  193,252,242,65,15,17,4,222,139,69,0,72,15,182,200,72,137,207,255,249,72,15,
  182,216,193,232,8,15,182,204,37,252,255,0,0,0,73,139,52,198,72,137,252,247,
  72,193,252,239,32,129,252,255,239,15,131,244,19,102,72,15,110,198,77,139,
  93,0,252,242,65,15,16,140,253,203,233,252,242,15,94,193,252,242,65,15,17,
  4,222,139,69,0,72,15,182,200,72,137,207,255,249,72,15,182,216,193,232,8,15,
  182,204,37,252,255,0,0,0,73,139,52,198,129,252,254,239,15,131,244,20,73,139,
  20,206,129,252,250,239,15,131,244,20,252,242,65,15,16,4,198,252,242,65,15,
  88,4,206,252,242,65,15,17,4,222,139,69,0,72,15,182,200,72,137,207,255,249,
  72,15,182,216,193,232,8,15,182,204,37,252,255,0,0,0,73,139,52,198,129,252,
  254,239,15,131,244,20,73,139,20,206,129,252,250,239,15,131,244,20,252,242,
  65,15,16,4,198,252,242,65,15,92,4,206,252,242,65,15,17,4,222,139,69,0,72,
  15,182,200,72,137,207,255,249,72,15,182,216,193,232,8,15,182,204,37,252,255,
  0,0,0,73,139,52,198,129,252,254,239,15,131,244,20,73,139,20,206,129,252,250,
  239,15,131,244,20,252,242,65,15,16,4,198,252,242,65,15,89,4,206,252,242,65,
  15,17,4,222,139,69,0,72,15,182,200,72,137,207,255,249,72,15,182,216,193,232,
  8,15,182,204,37,252,255,0,0,0,73,139,52,198,129,252,254,239,15,131,244,20,
  73,139,20,206,129,252,250,239,15,131,244,20,252,242,65,15,16,4,198,252,242,
  65,15,94,4,206,252,242,65,15,17,4,222,139,69,0,72,15,182,200,72,137,207,255,
  249,72,15,182,216,193,232,8,15,182,204,37,252,255,0,0,0,255,65,129,124,253,
  198,4,239,15,131,244,20,255,73,139,125,0,252,242,15,45,140,253,207,233,252,
  242,65,15,45,4,198,255,133,201,15,132,244,24,255,153,252,247,252,249,252,
  242,15,42,194,252,242,65,15,17,4,222,139,69,0,72,15,182,200,72,137,207,255,
  65,129,124,253,206,4,239,15,131,244,20,255,73,139,125,0,252,242,15,45,132,
  253,199,233,252,242,65,15,45,12,206,255,249,72,15,182,216,193,232,8,15,182,
  204,37,252,255,0,0,0,65,129,124,253,198,4,239,15,131,244,20,255,252,242,65,
  15,45,4,198,252,242,65,15,45,12,206,255,249,72,15,182,216,193,232,8,15,182,
  204,37,252,255,0,0,0,77,139,93,0,252,242,65,15,16,132,253,195,233,65,139,
  84,206,4,129,252,250,239,15,131,244,21,252,242,65,15,16,12,206,255,232,251,
  1,19,255,249,72,15,182,216,193,232,8,15,182,204,37,252,255,0,0,0,77,139,93,
  0,252,242,65,15,16,140,253,203,233,65,139,116,198,4,129,252,254,239,15,131,
  244,22,252,242,65,15,16,4,198,255,249,252,233,244,23,255,249,72,15,182,216,
  193,232,8,15,182,204,37,252,255,0,0,0,73,139,20,206,72,137,215,72,193,252,
  239,32,129,252,255,239,15,131,244,25,77,139,93,0,252,242,65,15,16,132,253,
  195,233,102,72,15,110,202,102,15,46,193,15,131,244,247,65,199,68,222,4,237,
  248,2,139,69,0,72,15,182,200,72,137,207,255,139,69,0,72,15,182,200,72,131,
  197,4,193,232,8,65,252,255,36,207,248,1,65,199,68,222,4,237,252,233,244,2,
  255,249,72,15,182,216,193,232,8,15,182,204,37,252,255,0,0,0,73,139,20,206,
  72,137,215,72,193,252,239,32,129,252,255,239,15,131,244,25,77,139,93,0,252,
  242,65,15,16,132,253,195,233,102,72,15,110,202,102,15,46,193,15,135,244,247,
  65,199,68,222,4,237,248,2,139,69,0,72,15,182,200,72,137,207,255,249,72,15,
  182,216,193,232,8,15,182,204,37,252,255,0,0,0,73,139,20,206,72,137,215,72,
  193,252,239,32,129,252,255,239,15,131,244,25,77,139,93,0,252,242,65,15,16,
  132,253,195,233,102,72,15,110,202,102,15,46,193,15,134,244,247,65,199,68,
  222,4,237,248,2,139,69,0,72,15,182,200,72,137,207,255,249,72,15,182,216,193,
  232,8,15,182,204,37,252,255,0,0,0,73,139,20,206,72,137,215,72,193,252,239,
  32,129,252,255,239,15,131,244,25,77,139,93,0,252,242,65,15,16,132,253,195,
  233,102,72,15,110,202,102,15,46,193,15,130,244,247,65,199,68,222,4,237,248,
  2,139,69,0,72,15,182,200,72,137,207,255,249,72,15,182,216,193,232,8,15,182,
  204,37,252,255,0,0,0,73,139,20,206,72,137,215,72,193,252,239,32,129,252,255,
  239,15,131,244,25,77,139,93,0,252,242,65,15,16,132,253,195,233,102,72,15,
  110,202,102,15,46,193,15,133,244,247,65,199,68,222,4,237,248,2,139,69,0,72,
  15,182,200,72,137,207,255,249,72,15,182,216,193,232,8,15,182,204,37,252,255,
  0,0,0,73,139,20,206,72,137,215,72,193,252,239,32,129,252,255,239,15,131,244,
  25,77,139,93,0,252,242,65,15,16,132,253,195,233,102,72,15,110,202,102,15,
  46,193,15,132,244,247,65,199,68,222,4,237,248,2,139,69,0,72,15,182,200,72,
  137,207,255,249,72,15,182,216,193,232,8,15,182,204,37,252,255,0,0,0,73,139,
  52,198,72,137,215,72,193,252,239,32,129,252,255,239,15,131,244,26,77,139,
  93,0,252,242,65,15,16,140,253,203,233,102,72,15,110,198,102,15,46,193,15,
  131,244,247,65,199,68,222,4,237,248,2,139,69,0,72,15,182,200,72,137,207,255,
  249,72,15,182,216,193,232,8,15,182,204,37,252,255,0,0,0,73,139,52,198,72,
  137,215,72,193,252,239,32,129,252,255,239,15,131,244,26,77,139,93,0,252,242,
  65,15,16,140,253,203,233,102,72,15,110,198,102,15,46,193,15,135,244,247,65,
  199,68,222,4,237,248,2,139,69,0,72,15,182,200,72,137,207,255,249,72,15,182,
  216,193,232,8,15,182,204,37,252,255,0,0,0,73,139,52,198,72,137,215,72,193,
  252,239,32,129,252,255,239,15,131,244,26,77,139,93,0,252,242,65,15,16,140,
  253,203,233,102,72,15,110,198,102,15,46,193,15,134,244,247,65,199,68,222,
  4,237,248,2,139,69,0,72,15,182,200,72,137,207,255,249,72,15,182,216,193,232,
  8,15,182,204,37,252,255,0,0,0,73,139,52,198,72,137,215,72,193,252,239,32,
  129,252,255,239,15,131,244,26,77,139,93,0,252,242,65,15,16,140,253,203,233,
  102,72,15,110,198,102,15,46,193,15,130,244,247,65,199,68,222,4,237,248,2,
  139,69,0,72,15,182,200,72,137,207,255,249,72,15,182,216,193,232,8,15,182,
  204,37,252,255,0,0,0,73,139,52,198,72,137,215,72,193,252,239,32,129,252,255,
  239,15,131,244,26,77,139,93,0,252,242,65,15,16,140,253,203,233,102,72,15,
  110,198,102,15,46,193,15,133,244,247,65,199,68,222,4,237,248,2,139,69,0,72,
  15,182,200,72,137,207,255,249,72,15,182,216,193,232,8,15,182,204,37,252,255,
  0,0,0,73,139,52,198,72,137,215,72,193,252,239,32,129,252,255,239,15,131,244,
  26,77,139,93,0,252,242,65,15,16,140,253,203,233,102,72,15,110,198,102,15,
  46,193,15,132,244,247,65,199,68,222,4,237,248,2,139,69,0,72,15,182,200,72,
  137,207,255,249,72,15,182,216,193,232,8,15,182,204,37,252,255,0,0,0,65,129,
  124,253,198,4,239,15,131,244,27,65,129,124,253,206,4,239,15,131,244,27,252,
  242,65,15,16,4,198,102,65,15,46,4,206,15,131,244,247,65,199,68,222,4,237,
  248,2,139,69,0,72,15,182,200,72,137,207,255,249,72,15,182,216,193,232,8,15,
  182,204,37,252,255,0,0,0,65,129,124,253,198,4,239,15,131,244,27,65,129,124,
  253,206,4,239,15,131,244,27,252,242,65,15,16,4,198,102,65,15,46,4,206,15,
  135,244,247,65,199,68,222,4,237,248,2,139,69,0,72,15,182,200,72,137,207,255,
  249,72,15,182,216,193,232,8,15,182,204,37,252,255,0,0,0,65,129,124,253,198,
  4,239,15,131,244,27,65,129,124,253,206,4,239,15,131,244,27,252,242,65,15,
  16,4,198,102,65,15,46,4,206,15,134,244,247,65,199,68,222,4,237,248,2,139,
  69,0,72,15,182,200,72,137,207,255,249,72,15,182,216,193,232,8,15,182,204,
  37,252,255,0,0,0,65,129,124,253,198,4,239,15,131,244,27,65,129,124,253,206,
  4,239,15,131,244,27,252,242,65,15,16,4,198,102,65,15,46,4,206,15,130,244,
  247,65,199,68,222,4,237,248,2,139,69,0,72,15,182,200,72,137,207,255,249,72,
  15,182,216,193,232,8,15,182,204,37,252,255,0,0,0,65,129,124,253,198,4,239,
  15,131,244,249,65,129,124,253,198,4,239,15,131,244,249,252,242,65,15,16,4,
  198,102,65,15,46,4,206,15,132,244,247,65,199,68,222,4,237,248,2,139,69,0,
  72,15,182,200,72,137,207,255,139,69,0,72,15,182,200,72,131,197,4,193,232,
  8,65,252,255,36,207,248,1,65,199,68,222,4,237,252,233,244,2,248,3,73,139,
  52,198,73,139,20,206,72,137,252,247,73,137,211,72,193,252,238,48,72,193,252,
  234,48,72,57,214,15,132,244,250,129,252,254,239,15,132,244,251,129,252,250,
  239,15,132,244,251,65,199,68,222,4,237,252,233,244,2,248,4,65,199,68,222,
  4,237,252,233,244,2,248,5,255,72,35,61,244,11,128,191,233,235,15,133,244,
  33,72,139,63,76,35,29,244,11,65,128,187,233,235,15,133,244,33,77,139,27,76,
  57,223,15,133,244,252,65,199,68,222,4,237,248,6,65,199,68,222,4,237,252,233,
  244,2,255,249,72,15,182,216,193,232,8,15,182,204,37,252,255,0,0,0,77,139,
  93,0,77,139,147,233,73,139,52,194,73,139,20,206,72,139,54,128,190,233,235,
  15,133,244,247,72,139,54,73,137,211,73,193,252,235,48,65,129,252,251,239,
  15,133,244,247,72,35,21,244,11,72,139,18,128,186,233,235,15,133,244,247,72,
  139,18,72,57,214,15,133,244,254,65,199,68,222,4,237,252,233,244,253,248,8,
  255,65,199,68,222,4,237,248,7,139,69,0,72,15,182,200,72,137,207,255,139,69,
  0,72,15,182,200,72,131,197,4,193,232,8,65,252,255,36,207,248,1,252,233,244,
  34,255,249,72,15,182,216,193,232,8,15,182,204,37,252,255,0,0,0,73,139,52,
  198,77,139,93,0,77,139,147,233,73,139,20,202,73,137,252,243,73,193,252,235,
  48,65,129,252,251,239,15,133,244,247,72,35,53,244,11,72,139,54,128,190,233,
  235,15,133,244,247,72,139,54,72,139,18,128,186,233,235,15,133,244,247,72,
  139,18,72,57,214,15,133,244,254,65,199,68,222,4,237,252,233,244,253,248,8,
  255,139,69,0,72,15,182,200,72,131,197,4,193,232,8,65,252,255,36,207,248,1,
  252,233,244,35,255,249,72,15,182,216,193,232,8,15,182,204,37,252,255,0,0,
  0,77,139,93,0,77,139,147,233,73,139,52,194,73,139,20,206,72,139,54,128,190,
  233,235,15,133,244,247,72,139,54,73,137,211,73,193,252,235,48,65,129,252,
  251,239,15,133,244,247,72,35,21,244,11,72,139,18,128,186,233,235,15,133,244,
  247,72,139,18,72,57,214,15,132,244,254,65,199,68,222,4,237,252,233,244,253,
  248,8,255,249,72,15,182,216,193,232,8,15,182,204,37,252,255,0,0,0,73,139,
  52,198,77,139,93,0,77,139,147,233,73,139,20,202,73,137,252,243,73,193,252,
  235,48,65,129,252,251,239,15,133,244,247,72,35,53,244,11,72,139,54,128,190,
  233,235,15,133,244,247,72,139,54,72,139,18,128,186,233,235,15,133,244,247,
  72,139,18,72,57,214,15,132,244,254,65,199,68,222,4,237,252,233,244,253,248,
  8,255,249,15,182,216,193,232,8,65,129,124,253,198,4,239,15,131,244,254,255,
  252,242,65,15,16,4,198,73,187,237,237,102,73,15,110,203,102,15,87,193,252,
  242,65,15,17,4,222,139,69,0,72,15,182,200,72,137,207,255,248,8,76,137,231,
  73,139,52,198,73,141,20,222,255,232,251,1,20,255,249,15,182,216,193,232,8,
  185,237,255,102,65,129,124,253,198,6,238,15,132,244,247,65,129,124,253,198,
  4,239,15,71,13,244,12,248,1,65,137,76,222,4,139,69,0,72,15,182,200,72,137,
  207,255,249,72,15,182,216,193,232,8,102,65,129,124,253,222,6,238,15,132,244,
  248,65,129,124,253,222,4,239,15,135,244,247,248,2,72,139,12,36,72,141,44,
  129,248,1,139,69,0,72,15,182,200,72,137,207,255,249,72,15,182,216,193,232,
  8,102,65,129,124,253,222,6,238,15,132,244,248,65,129,124,253,222,4,239,15,
  134,244,248,72,139,12,36,72,141,44,129,248,2,139,69,0,72,15,182,200,72,137,
  207,255,249,72,15,182,216,193,232,8,102,65,129,124,253,222,6,238,15,132,244,
  247,65,129,124,253,222,4,239,15,134,244,247,72,139,12,36,72,141,44,129,248,
  1,139,69,0,72,15,182,200,72,137,207,255,249,72,15,183,216,72,139,12,36,72,
  141,44,153,139,69,0,72,15,182,200,72,137,207,255,249,72,15,182,216,193,232,
  8,255,102,65,129,124,253,222,6,238,15,133,244,254,73,139,52,222,72,35,53,
  244,11,72,139,54,128,190,233,235,15,133,244,255,255,72,35,21,244,36,72,139,
  18,128,186,233,235,15,133,244,254,72,139,18,255,73,137,252,242,69,139,146,
  233,68,139,154,233,65,131,252,234,1,69,33,211,68,141,150,233,71,141,28,91,
  71,141,20,218,76,141,158,233,65,139,178,233,252,247,198,237,15,132,244,253,
  248,1,252,247,198,237,15,133,244,252,73,139,50,72,139,54,128,190,233,235,
  15,133,244,252,72,139,54,72,139,54,72,57,214,15,133,244,252,77,139,146,233,
  77,137,150,233,255,139,69,0,72,15,182,200,72,131,197,4,193,232,8,65,252,255,
  36,207,248,6,65,139,178,233,252,247,198,237,15,132,244,253,129,230,239,68,
  141,20,118,71,141,20,211,66,139,52,213,237,252,233,244,1,248,7,252,233,244,
  29,255,248,8,252,233,244,28,255,248,9,252,233,244,30,255,249,72,15,182,216,
  193,232,8,73,137,172,253,36,233,76,137,231,73,139,182,252,248,7,0,0,73,139,
  20,222,77,139,93,0,77,139,147,233,73,139,12,194,255,232,251,1,21,255,249,
  15,182,216,193,232,8,73,137,172,253,36,233,76,137,231,73,139,52,222,73,139,
  20,198,255,232,251,1,22,255,249,72,15,182,216,193,232,8,15,182,204,37,252,
  255,0,0,0,73,137,172,253,36,233,76,137,231,73,139,52,222,73,139,20,198,73,
  139,12,206,255,232,251,1,23,255,249,72,15,182,216,193,232,8,77,139,156,253,
  36,233,77,139,147,233,73,139,52,194,73,137,52,222,139,69,0,72,15,182,200,
  72,137,207,255,249,72,15,183,216,193,232,16,73,139,20,198,77,139,156,253,
  36,233,77,139,147,233,73,137,20,218,139,69,0,72,15,182,200,72,137,207,255,
  249,72,15,183,216,193,232,16,73,137,172,253,36,233,76,137,231,77,139,93,0,
  77,139,147,233,73,139,52,218,73,139,20,198,255,232,251,1,24,255,249,72,15,
  182,216,193,232,8,73,137,172,253,36,233,76,137,231,73,141,52,222,77,139,93,
  0,77,139,147,233,73,139,20,194,255,232,251,1,25,255,65,129,190,253,252,252,
  7,0,0,239,15,132,244,247,248,2,139,69,0,72,15,182,200,72,137,207,255,139,
  69,0,72,15,182,200,72,131,197,4,193,232,8,65,252,255,36,207,248,1,72,139,
  12,36,72,141,44,129,252,233,244,2,255,249,15,182,216,193,232,8,65,129,124,
  253,222,4,239,15,131,244,37,255,65,129,124,253,198,4,239,15,131,244,37,255,
  252,242,65,15,16,4,222,102,65,15,46,4,198,15,131,244,254,255,139,93,0,72,
  139,12,36,72,141,44,153,248,7,139,69,0,72,15,182,200,72,137,207,255,139,69,
  0,72,15,182,200,72,131,197,4,193,232,8,65,252,255,36,207,248,8,72,131,197,
  4,252,233,244,8,255,248,6,76,137,231,73,139,52,222,73,139,20,198,139,77,0,
  255,232,251,1,26,255,133,192,15,132,244,16,73,139,172,253,36,233,139,69,0,
  72,15,182,200,72,137,207,255,249,72,15,182,216,193,232,8,15,182,204,37,252,
  255,0,0,0,65,129,124,253,222,4,239,15,131,244,252,65,129,124,253,198,4,239,
  15,131,244,252,65,129,124,253,206,4,239,15,131,244,252,255,252,242,65,15,
  16,4,222,252,242,65,15,88,4,206,102,65,15,46,4,198,252,242,65,15,17,4,222,
  15,131,244,254,255,139,69,0,72,15,182,200,72,131,197,4,193,232,8,65,252,255,
  36,207,248,8,72,131,197,4,252,233,244,7,255,248,6,76,137,231,73,141,52,222,
  73,139,20,198,73,139,12,206,68,139,69,0,255,232,251,1,27,255,249,139,69,0,
  72,15,182,200,72,137,207,255,249,205,3,255
};

#line 465 "src/interpreter/bytecode-interpreter.dasc"
//|.globals GLBNAME_
enum {
  GLBNAME_ValueHeapMaskStore,
  GLBNAME_ValueHeapMaskLoad,
  GLBNAME_FlagTrueConst,
  GLBNAME_FlagFalseConst,
  GLBNAME_RealZero,
  GLBNAME_InterpStart,
  GLBNAME_InterpFail,
  GLBNAME_InterpReturn,
  GLBNAME_InterpArithRealL,
  GLBNAME_InterpArithRealR,
  GLBNAME_InterpArithVV,
  GLBNAME_InterpPowSlowRV,
  GLBNAME_InterpPowSlowVR,
  GLBNAME_InterpPowSlowVV,
  GLBNAME_DivByZero,
  GLBNAME_InterpCompareRV,
  GLBNAME_InterpCompareVR,
  GLBNAME_InterpCompareVV,
  GLBNAME_InterpPropGet,
  GLBNAME_InterpPropGetNotFound,
  GLBNAME_InterpPropGetNotObject,
  GLBNAME_InterpIdxGetI,
  GLBNAME_InterpNewList,
  GLBNAME_InterpCompareHH,
  GLBNAME_InterpCompareSV,
  GLBNAME_InterpCompareVS,
  GLBNAME_ValueHeapLoadMask,
  GLBNAME_InterpreterForEnd1,
  GLBNAME__MAX
};
#line 466 "src/interpreter/bytecode-interpreter.dasc"
//|.globalnames glbnames
static const char *const glbnames[] = {
  "ValueHeapMaskStore",
  "ValueHeapMaskLoad",
  "FlagTrueConst",
  "FlagFalseConst",
  "RealZero",
  "InterpStart",
  "InterpFail",
  "InterpReturn",
  "InterpArithRealL",
  "InterpArithRealR",
  "InterpArithVV",
  "InterpPowSlowRV",
  "InterpPowSlowVR",
  "InterpPowSlowVV",
  "DivByZero",
  "InterpCompareRV",
  "InterpCompareVR",
  "InterpCompareVV",
  "InterpPropGet",
  "InterpPropGetNotFound",
  "InterpPropGetNotObject",
  "InterpIdxGetI",
  "InterpNewList",
  "InterpCompareHH",
  "InterpCompareSV",
  "InterpCompareVS",
  "ValueHeapLoadMask",
  "InterpreterForEnd1",
  (const char *)0
};
#line 467 "src/interpreter/bytecode-interpreter.dasc"
//|.externnames extnames
static const char *const extnames[] = {
  "PrintOP",
  "InterpreterDoArithmetic",
  "InterpreterPow",
  "InterpreterDivByZero",
  "InterpreterDoCompare",
  "InterpreterDoPropGet",
  "InterpreterPropGetNotFound",
  "InterpreterPropGetNotObject",
  "InterpreterDoIdxGetI",
  "InterpreterDoLoadList0",
  "InterpreterDoLoadList1",
  "InterpreterDoLoadList2",
  "InterpreterDoNewList",
  "InterpreterDoAddList",
  "InterpreterDoLoadObj0",
  "InterpreterDoLoadObj1",
  "InterpreterDoNewObj",
  "InterpreterDoAddObj",
  "InterpreterDoLoadCls",
  "pow",
  "InterpreterDoNegate",
  "InterpreterDoPropSet",
  "InterpreterDoIdxGet",
  "InterpreterDoIdxSet",
  "InterpreterDoGSet",
  "InterpreterDoGGet",
  "InterpreterForEnd1",
  "InterpreterForEnd2",
  (const char *)0
};
#line 468 "src/interpreter/bytecode-interpreter.dasc"
//|.section code,data
#define DASM_SECTION_CODE	0
#define DASM_SECTION_DATA	1
#define DASM_MAXSECTION		2
#line 469 "src/interpreter/bytecode-interpreter.dasc"

/* -------------------------------------------------------------------
 * Preprocessor option for dynasm
 * ------------------------------------------------------------------*/
//|.define CHECK_DIV_BY_ZERO
//|.define CHECK_NUMBER_MEMORY,0
//|.define TRACE_OP,1
//|.define USE_CMOV_COMP,0
//|.define USE_CMOV_NEG

/* -------------------------------------------------------------------
 * 64 bits call
 *
 * Since 64 bits call cannot accept a imm value due to it is too long,
 * we need to generate different *types* of call instruction based on
 * the callsite
 * -------------------------------------------------------------------*/
inline bool CheckAddress( std::uintptr_t addr ) {
  static const std::uintptr_t k2G = 0x80000000;
  if(addr > 0 && addr < k2G)
    return true;
  else
    return false;
}

//|.macro fcall,FUNC
//|| if(CheckAddress(reinterpret_cast<std::uintptr_t>(FUNC))) {
//|    call extern FUNC
//|| } else {
//||   lava_warn("%s","Function FUNC address is not in 0-2GB");
//|.if 0
// I don't know whether this is faster than use rax , need profile. I see
// this one is used in MoarVM. It uses memory address to work araoud the
// address space problem. But I am kind of unsure about it since it maybe
// because MoarVM already allocate rax for other things
//|9:
//|.dword (std::uint32_t)((std::uintptr_t)(FUNC)),(std::uint32_t)((std::uintptr_t)((FUNC)>>32))
//|    call qword[<9]
//|.else
//|    mov64 rax, reinterpret_cast<std::uintptr_t>(FUNC)
//|    call rax
//|.endif
//|| }
//|.endmacro

/* ---------------------------------------------------------------
 * summary of register usage                                     |
 * --------------------------------------------------------------*/
// Runtime pointer
//|.define RUNTIME,               r12   // callee saved

// Current prototype's GCRef pointer
//|.define PROTO,                 r13   // callee saved

// Top stack's pointer
//|.define STK,                   r14   // callee saved
//|.define ACCIDX,                2040
//|.define ACCFIDX,               2044
//|.define ACCFHIDX,              2046  // for heap flag
//|.define ACC,                   STK+ACCIDX

// Dispatch table pointer
//|.define DISPATCH,              r15  // callee saved

// Bytecode array
//|.define PC,                    rbp  // callee saved

// Hold the decoded unit
//|.define INSTR,                 eax
//|.define INSTR_OP,              al
//|.define INSTR_A8L,             al
//|.define INSTR_A8H,             ah
//|.define INSTR_A16,             ax

// Frame -------------------------------------------------------
// We store the frame sizeof(IFrame) above STK pointer
static_assert( sizeof(IFrame) == 16 );
//|.define CFRAME,                STK-16
//|.define FRAMELEN,              16

// Instruction's argument
//|.define ARG1_8,                bl
//|.define ARG1_16,               bx
//|.define ARG1,                  ebx  // must *not* be 64 bits due to we use ah and it MUST be callee saved
//|.define ARG1F,                 rbx

// Used to help decode function's call argument
//|.define FARG,                  ebx  // aliased with ARG1
//|.define FARG16,                bx
//|.define FARG8L,                bl
//|.define FARG8H,                bh

//|.define ARG2_8,                al
//|.define ARG2_16,               ax
//|.define ARG2,                  eax
//|.define ARG2F,                 rax

//|.define ARG3_8,                cl
//|.define ARG3_16,               cx
//|.define ARG3,                  ecx
//|.define ARG3F,                 rcx

// temporarily alias OP to be ARG3 because during the
// decoding time , we don't need to hold anything there
//|.define OP,                    rcx

// temporary register are r10 and r11
//|.define LREG,                  rsi
//|.define LREGL,                 esi
//|.define LREGLL,                 si
//|.define RREG,                  rdx
//|.define RREGL,                 edx
//|.define RREGLL,                 dx

// absolute safe temporary variables
//
//
// In most places , prefer T0 as temporary register if ARGX
// is not free since it doesn't require REX encoding. But it
// alias with CARG1 , use with cautious
//|.define T0,                    rdi
//|.define T0L,                   edi
//|.define T0L16,                 di

//|.define T1,                    r11
//|.define T1L,                   r11d
//|.define T1L16,                 r11w

//|.define T2,                    r10
//|.define T2L,                   r10d
//|.define T2L16,                 r10w

// registers for normal C function calling ABI
//|.define CARG1,                 rdi
//|.define CARG2,                 rsi    // LREG
//|.define CARG3,                 rdx    // RREG
//|.define CARG4,                 rcx
//|.define CARG5,                 r8
//|.define CARG6,                 r9

//|.define CARG1L,                edi
//|.define CARG2L,                esi
//|.define CARG3L,                edx
//|.define CARG4L,                ecx
//|.define CARG5L,                r8d
//|.define CARG6L,                r9d

//|.define CARG1LL,               dil
//|.define CARG2LL,               sil
//|.define CARG3LL,               dl
//|.define CARG4LL,               cl
//|.define CARG5LL,               r8b
//|.define CARG6LL,               r9b

// saved callee registers plus some other important stuff
// 72 = 64 + (8 padding for function call)
//|.define RESERVE_RSP,           72
//|.define SAVED_RBX,             [rsp+48]
//|.define SAVED_R12,             [rsp+40]
//|.define SAVED_R13,             [rsp+32]
//|.define SAVED_R14,             [rsp+24]
//|.define SAVED_R15,             [rsp+16]
//|.define SAVED_RBP,             [rsp+8]

// DO NOT MODIFY IT UNLESS YOU KNOW SHIT
//|.define SAVED_PPC,             rsp
//|.define SAVED_PC ,             [rsp]

// Used to save certain registers while we call cross the function
// boundary. Like we may call into ToBoolean function to get value
// of certain register's Boolean value and we may need to save register
// like rax which is part of our argument/operand of isntructions
//|.define SAVED_SLOT1,           [rsp+56]
//|.define SAVED_SLOT2,           [rsp+64]

/* ---------------------------------------------------------------
 * debug helper                                                  |
 * --------------------------------------------------------------*/
//|.macro Break
//|  int 3
//|.endmacro

void PrintOP( int op ) {
  lava_error("OP:%s",GetBytecodeName(static_cast<Bytecode>(op)));
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(PrintOP)

void Print2( int a , int b ) {
  lava_error("L:%d,R:%d",a,b);
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(Print2)

void PrintF( double v ) {
  lava_error("Value:%f",v);
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(PrintF)

void Print64( std::uint64_t a , std::uint64_t b ) {
  lava_error("%" LAVA_FMTU64 ":%" LAVA_FMTU64,a,b);
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(Print64)

/* ---------------------------------------------------------------
 * dispatch table                                                |
 * --------------------------------------------------------------*/
//|.macro Dispatch
//|.if TRACE_OP
//|  mov INSTR,dword [PC]
//|  movzx OP,INSTR_OP
//|  mov CARG1,OP
//|  fcall PrintOP
//|.endif
// IFETCH starts, one of the hottest code segement
// The top 4 instructions are ordered as 4-1-1-1 to optimize decoder so
// it can be decoded in one cycle.
//|  mov INSTR,dword [PC]  // 2 uops
//|  movzx OP,INSTR_OP     // 1 uops
//|  add PC,4              // fused 1uops
//|  shr INSTR,8           // fused 1uops
//|  jmp aword [DISPATCH+OP*8]
//|.endmacro

/* ---------------------------------------------------------------
 * decode each instruction's argument/operand                    |
 * --------------------------------------------------------------*/

//|.macro instr_B
// cannot use ARG1F due to INSTR_A8 uses ah
//|  movzx ARG1F,INSTR_A8L
//|  shr INSTR,8
//|.endmacro

//|.macro instr_C
//|  movzx ARG1F,INSTR_A16
//|  shr INSTR,16
//|.endmacro

//|.macro instr_D
//|  movzx ARG1F,INSTR_A8L
//|  shr INSTR,8
// do not change ARG3 --> ARG3F even if it is better, due to the fact
// we cannot use ax register when rex prefix is used in instruction.
//|  movzx ARG3,INSTR_A8H
//|  and ARG2,0xff
//|.endmacro

//|.macro instr_E
//|  movzx ARG1,INSTR_A8L
//|  shr INSTR,8
//|.endmacro

//|.macro instr_F
//|  movzx ARG1,INSTR_A8L
//|.endmacro

//|.macro instr_G
//|  movzx ARG1F,INSTR_A16
//|.endmacro

//|.macro instr_X
//|.endmacro

//|.macro instr_N
//|  instr_D
//|.endmacro

/* -----------------------------------------------------------
 * Special Constant for Real                                 |
 * ----------------------------------------------------------*/
//|.macro LdRConstH,XREG,HIGH
//|  mov64 T1,(static_cast<std::uint64_t>(HIGH)<<32)
//|  movd XREG,T1
//|.endmacro

//|.macro LdRConstL,XREG,LOW
//|  mov64 T1,(static_cast<std::uint64_t>(LOW))
//|  movd XREG,T1
//|.endmacro

//|.macro LdRConst,XREG,X64V
//|  mov64 T1,X64V
//|  movd  XREG,T1
//|.endmacro

// Used to negate the double precision number's sign bit
//|.macro LdRConst_sign,XREG; LdRConstH XREG,0x80000000; .endmacro
//|.macro LdRConst_one ,XREG; LdRConstH XREG,0x3ff00000; .endmacro
//|.macro LdRConst_neg_one,XREG; LdRConstH XREG,0xbff00000; .endmacro

/* -----------------------------------------------------------
 * constant loading                                          |
 * ----------------------------------------------------------*/

// Currently our constant loading is *slow* due to the design of our GC
// and also the layout of each constant array. I think we have a way to
// optimize away one memory move. LuaJIT's constant loading is just one
// single instruction since they only get one constant array and they don't
// need to worry about GC move the reference

//|.macro LdReal,reg,index
//|  mov T1,qword [PROTO]
//|  movsd reg, qword [T1+index*8+PrototypeLayout::kRealTableOffset]
//|.endmacro

//|.macro LdRealV,reg,index
//|  mov T1,qword [PROTO]
//|  mov reg, qword [T1+index*8+PrototypeLayout::kRealTableOffset]
//|.endmacro

//|.macro LdReal2Int,reg,index,temp
//|  mov temp,qword [PROTO]
//|  cvtsd2si reg,qword [temp+index*8+PrototypeLayout::kRealTableOffset]
//|.endmacro

//|.macro StRealACC,reg
//|  movsd qword [ACC],reg
//|.endmacro

//|.macro StReal,idx,reg
//|  movsd qword [STK+idx*8], reg
//|.endmacro

//|.macro StRealFromInt,idx,reg
//|  cvtsi2sd xmm0,reg
//|  movsd qword [STK+idx*8], xmm0
//|.endmacro

// --------------------------------------------
// load upvalue value into register
//|.macro LdUV,reg,index
//|  mov T1,qword [RUNTIME+RuntimeLayout::kCurClsOffset]
//|  mov T2,qword [T1+ClosureLayout::kUpValueOffset]
//|  mov reg, qword [T2+index*8]
//|.endmacro

//|.macro StUV,index,reg
//|  mov T1,qword [RUNTIME+RuntimeLayout::kCurClsOffset]
//|  mov T2,qword [T1+ClosureLayout::kUpValueOffset]
//|  mov qword [T2+index*8], reg
//|.endmacro

// ----------------------------------------------
// Heap value related stuff

// This byte offset in little endian for type pattern inside of
// heap object header
#define HOH_TYPE_OFFSET 7

// Check whether a Value is a HeapObject
//|.macro CheckHeap,val,fail_label
//|  mov T1,val
//|  shr T1,48
//|  cmp T1L, Value::FLAG_HEAP
//|  jne >fail_label
//|.endmacro

// Set a pointer into a register , this is really painful
//|.macro StHeap,val

//|.if 0
//|  mov T1,Value::FLAG_HEAP
//|  shl T1,48
//|  or val,T1
//|.else
//|  or  val,qword [->ValueHeapMaskStore]
//|.endif

//|.endmacro

// Load a pointer from Value object , assume this object
// is a pointer type
//|.macro LdPtrFromV,dest,val
//|.if 1
//|  mov dest,Value::FLAG_HEAP_UNMASK
//|  shl dest,48
//|  or  dest,val
//|.else
//|  mov dest,val
//|  or  dest,qword [->ValueHeapLoadMask]
//|.endif
//|.endmacro

//|.macro DerefPtrFromV,v
//|  and v, qword [->ValueHeapLoadMask]
//|.endmacro

// It is painful to load a string into its Value format
//|.macro LdStrV,val,index
//|  mov T1 , qword [PROTO]
//|  mov T2 , qword [T1+PrototypeLayout::kStringTableOffset]
//|  mov val, qword [T2+index*8]
//|  StHeap val
//|.endmacro

//|.macro LdStr,val,index
//|  mov T1 , qword [PROTO]
//|  mov T2 , qword [T1+PrototypeLayout::kStringTableOffset]
//|  mov val, qword [T2+index*8]
//|.endmacro

// General macro to check a heap object is certain type
//|.macro CheckHeapPtrT,val,pattern,fail_label
//|  cmp byte [val-HOH_TYPE_OFFSET], pattern
//|  jne >fail_label
//|.endmacro

//|.macro CheckHeapT,val,pattern,fail_label
//|  and val,qword [->ValueHeapMaskLoad]
//|  mov val, qword [val]
//|  CheckHeapPtrT val,pattern,fail_label
//|.endmacro

// -------------------------------------------------------------------------
// Object bit pattern

#define OBJECT_BIT_PATTERN TYPE_OBJECT

#define LIST_BIT_PATTERN TYPE_LIST

#define SSO_BIT_PATTERN TYPE_STRING

#define CLOSURE_BIT_PATTERN TYPE_CLOSURE

#define EXTENSION_BIT_PATTERN TYPE_EXTENSION

// -------------------------------------------------------------------------
// Check an *GCRef* is Object or not

//|.macro CheckObj,val,fail_label
//|  CheckHeapT val,OBJECT_BIT_PATTERN,fail_label
//|.endmacro

//|.macro CheckObjV,val,fail_label
//|  CheckHeap val,fail_label
//|  CheckObj val,fail_label
//|.endmacro

// --------------------------------------------------------------------------
// Check an *GCRef* is List or not

//|.macro CheckList,val,fail_label
//|  CheckHeapT val,LIST_BIT_PATTERN,fail_label
//|.endmacro

//|.macro CheckListV,val,fail_label
//|  CheckHeap val,fail_label
//|  CheckObj val,fail_label
//|.endmacro

// --------------------------------------------------------------------------
// Check a *GCRef* is SSO or not , not stored in Value

// reg : pointer of String
//|.macro CheckSSO,reg,fail
//|  mov reg, qword [reg]
//|  CheckHeapPtrT reg,SSO_BIT_PATTERN,fail
// dereference *once* from String* --> SSO*
//|  mov reg, qword [reg]
//|.endmacro

//|.macro CheckSSOV,reg,fail
//|  CheckHeap reg,fail
//|  CheckHeapT,reg,SSO_BIT_PATTERN,fail
// dereference *once* from String* --> SSO*
//|  mov reg, qword [reg]
//|.endmacro

//|.macro CheckSSORaw,reg,fail
//|  and reg,qword [->ValueHeapMaskLoad]
//|  cmp byte [reg-HOH_TYPE_OFFSET], SSO_BIT_PATTERN
//|  jne fail
//|  mov reg,qword [reg]
//|.endmacro

#define INTERP_HELPER_LIST(__) \
  /* arithmetic */                           \
  __(INTERP_START,InterpStart)               \
  __(INTERP_FAIL ,InterpFail)                \
  __(INTERP_RETURN,InterpReturn)             \
  __(INTERP_ARITH_REALL,InterpArithRealL)    \
  __(INTERP_ARITH_REALR,InterpArithRealR)    \
  __(INTERP_ARITH_VV,InterpArithVV)          \
  __(INTERP_POW_SLOWVR,InterpPowSlowVR)      \
  __(INTERP_POW_SLOWRV,InterpPowSlowRV)      \
  __(INTERP_POW_SLOWVV,InterpPowSlowVV)      \
  __(DIV_BY_ZERO,DivByZero)                  \
  /* comparison */                           \
  __(INTERP_COMPARERV,InterpCompareRV)       \
  __(INTERP_COMPAREVR,InterpCompareVR)       \
  __(INTERP_COMPAREVV,InterpCompareVV)       \
  __(INTERP_COMPAREHH,InterpCompareHH)       \
  /* property get/set */                     \
  __(INTERP_PROPGET_NOTFOUND,InterpPropGetNotFound)      \
  __(INTERP_PROPGET_NOTOBJECT,InterpPropGetNotObject)    \
  __(INTERP_PROPGET,InterpPropGet)           \
  __(INTERP_IDXGETI,InterpIdxGetI)           \
  __(INTERP_IDXGET ,InterpIdxGet )           \
  /* ---- debug helper ---- */               \
  __(PRINT_OP,PrintOP)                       \
  __(PRINT2  ,Print2 )                       \
  __(PRINT64 ,Print64)                       \
  __(PRINTF  ,PrintF )

enum {
  INTERP_HELPER_DUMMY = SIZE_OF_BYTECODE,

#define __(A,B) A,
  INTERP_HELPER_LIST(__)
#undef __

  DASM_GROWABLE_PC_SIZE
};

#define INTERP_HELPER_START (INTERP_HELPER_DUMMY+1)
#define INTERP_HELPER_SIZE (DASM_GROWABLE_PC_SIZE-INTERP_HELPER_ROUTINE_ENUM-1)

const char* GetInterpHelperName( int idx ) {
  switch(idx) {
#define __(A,B) case A: return #B;
    INTERP_HELPER_LIST(__)
    default:
      lava_unreachF("unknown helper with index:%d",idx);
      return NULL;
#undef __ // __
  }
}

/* -----------------------------------------------------------
 * Macro Interfaces for Dynasm                               |
 * ----------------------------------------------------------*/
#define Dst (&(bctx->dasm_ctx))

/* -----------------------------------------------------------
 * Interpreter Prolog                                        |
 * ----------------------------------------------------------*/
void GenerateInterpMisc( BuildContext* bctx ) {
  /* -------------------------------------------
   * Constant value needed for the interpreter |
   * ------------------------------------------*/
  // Align with cache line ???
  //|.data
  dasm_put(Dst, 0);
#line 1010 "src/interpreter/bytecode-interpreter.dasc"
  //|->ValueHeapMaskStore:
  //|.dword Value::TAG_HEAP_STORE_MASK_LOWER,Value::TAG_HEAP_STORE_MASK_HIGHER // 8 bytes
  dasm_put(Dst, 2, Value::TAG_HEAP_STORE_MASK_LOWER, Value::TAG_HEAP_STORE_MASK_HIGHER);
#line 1012 "src/interpreter/bytecode-interpreter.dasc"

  //|->ValueHeapMaskLoad:
  //|.dword Value::TAG_HEAP_LOAD_MASK_LOWER,Value::TAG_HEAP_LOAD_MASK_HIGHER // 8 bytes
  dasm_put(Dst, 7, Value::TAG_HEAP_LOAD_MASK_LOWER, Value::TAG_HEAP_LOAD_MASK_HIGHER);
#line 1015 "src/interpreter/bytecode-interpreter.dasc"

  //|->FlagTrueConst:
  //|.dword Value::FLAG_TRUE // 4 bytes
  dasm_put(Dst, 12, Value::FLAG_TRUE);
#line 1018 "src/interpreter/bytecode-interpreter.dasc"

  //|->FlagFalseConst:
  //|.dword Value::FLAG_FALSE // 4 bytes
  dasm_put(Dst, 16, Value::FLAG_FALSE);
#line 1021 "src/interpreter/bytecode-interpreter.dasc"

  //|->RealZero:
  //|.dword 0,0  // 8 btyes
  dasm_put(Dst, 20);
#line 1024 "src/interpreter/bytecode-interpreter.dasc"

  //|.code
  dasm_put(Dst, 31);
#line 1026 "src/interpreter/bytecode-interpreter.dasc"

  /* -------------------------------------------
   * Start of the code                         |
   * ------------------------------------------*/

  //|.macro interp_prolog
  //|  sub   rsp, RESERVE_RSP             // make room on the stack

  //|  mov qword SAVED_R12,r12            // runtime
  //|  mov qword SAVED_R13,r13            // proto
  //|  mov qword SAVED_R14,r14            // stack
  //|  mov qword SAVED_R15,r15            // dispatch
  //|  mov qword SAVED_RBP,rbp            // PC
  //|  mov qword SAVED_RBX,rbx            // used by ARG2 , this may be changed in the future
  //|.endmacro

  //|.macro interp_epilog
  //|  mov r12, qword SAVED_R12
  //|  mov r13, qword SAVED_R13
  //|  mov r14, qword SAVED_R14
  //|  mov r15, qword SAVED_R15
  //|  mov rbp, qword SAVED_RBP
  //|  mov rbx, qword SAVED_RBX

  //|  add rsp, RESERVE_RSP
  //|.endmacro

  /* -------------------------------------------
   * Interpreter Prolog                        |
   * ------------------------------------------*/
  //|=> INTERP_START:
  //|->InterpStart:
  dasm_put(Dst, 33,  INTERP_START);
#line 1058 "src/interpreter/bytecode-interpreter.dasc"
  // save all callee saved register since we use them to keep tracking of
  // our most important data structure
  //|  interp_prolog
  dasm_put(Dst, 37);
#line 1061 "src/interpreter/bytecode-interpreter.dasc"

  //|  mov RUNTIME ,CARG1                 // runtime
  //|  mov PROTO   ,CARG2                 // proto
  //|  mov STK     ,CARG3                 // stack
  //|  mov PC      ,CARG4                 // pc
  //|  mov DISPATCH,CARG5                 // dispatch
  dasm_put(Dst, 73);
#line 1067 "src/interpreter/bytecode-interpreter.dasc"

  //|  mov qword SAVED_PC,CARG4           // save the *start* of bc array
  dasm_put(Dst, 91);
#line 1069 "src/interpreter/bytecode-interpreter.dasc"
  // run
  //|  Dispatch
  dasm_put(Dst, 96);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 107);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 120);
#line 1071 "src/interpreter/bytecode-interpreter.dasc"

  /* -------------------------------------------
   * Interpreter exit handler                  |
   * ------------------------------------------*/
  //|=> INTERP_FAIL:
  //|->InterpFail:
  //|  xor eax,eax
  //|  interp_epilog
  //|  ret
  dasm_put(Dst, 140,  INTERP_FAIL);
#line 1080 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_RETURN:
  //|->InterpReturn:
  //|  mov rax, qword [ACC]
  //|  mov qword [RUNTIME+RuntimeLayout::kRetOffset],rax
  //|  mov rax,1
  dasm_put(Dst, 181,  INTERP_RETURN, RuntimeLayout::kRetOffset);
#line 1086 "src/interpreter/bytecode-interpreter.dasc"

  //|  interp_epilog
  //|  ret
  dasm_put(Dst, 145);
#line 1089 "src/interpreter/bytecode-interpreter.dasc"
}

/* ------------------------------------------
 * helper functions/routines generation     |
 * -----------------------------------------*/

// ----------------------------------------
// helper macros
// ----------------------------------------
//|.macro ret2acc
//|  mov T1,rax
//|  shr T1,32
//|  cmp T1,VALUE_FAIL
//|  je ->InterpFail
//|  mov qword [ACC], rax
//|  Dispatch
//|.endmacro

//|.macro retbool
//|  test eax,eax
//|  je ->InterpFail
//|  Dispatch
//|.endmacro

// saving the current PC into the Runtime object, this is
// needed for GC to figure out the correct active register
// layout during the GC marking phase
//|.macro savepc
//|  mov qword [RUNTIME+RuntimeLayout::kCurPCOffset], PC
//|.endmacro

void GenerateHelper( BuildContext* bctx ) {

  /* ----------------------------------------
   * InterpArithXXX                         |
   * ---------------------------------------*/
  //|=> INTERP_ARITH_REALL:
  //|->InterpArithRealL:
  //|  savepc
  //|  mov CARG1,RUNTIME
  //|  LdRealV CARG2,ARG1F
  //|  fcall InterpreterDoArithmetic
  dasm_put(Dst, 206,  INTERP_ARITH_REALL, RuntimeLayout::kCurPCOffset, PrototypeLayout::kRealTableOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic))) {
  dasm_put(Dst, 229);
   } else {
     lava_warn("%s","Function InterpreterDoArithmetic address is not in 0-2GB");
  dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic))>>32));
   }
#line 1131 "src/interpreter/bytecode-interpreter.dasc"
  //|  ret2acc
  dasm_put(Dst, 234, VALUE_FAIL);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 107);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 120);
#line 1132 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_ARITH_REALR:
  //|->InterpArithRealR:
  //|  savepc
  //|  mov CARG1,RUNTIME
  //|  LdRealV CARG3,ARG2F
  //|  fcall InterpreterDoArithmetic
  dasm_put(Dst, 270,  INTERP_ARITH_REALR, RuntimeLayout::kCurPCOffset, PrototypeLayout::kRealTableOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic))) {
  dasm_put(Dst, 229);
   } else {
     lava_warn("%s","Function InterpreterDoArithmetic address is not in 0-2GB");
  dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic))>>32));
   }
#line 1139 "src/interpreter/bytecode-interpreter.dasc"
  //|  ret2acc
  dasm_put(Dst, 234, VALUE_FAIL);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 107);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 120);
#line 1140 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_ARITH_VV:
  //|->InterpArithVV:
  //|  savepc
  //|  mov CARG1, RUNTIME
  //|  mov CARG2, qword [STK+ARG1F*8]
  //|  mov CARG3, qword [STK+ARG2F*8]
  //|  fcall InterpreterDoArithmetic
  dasm_put(Dst, 293,  INTERP_ARITH_VV, RuntimeLayout::kCurPCOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic))) {
  dasm_put(Dst, 229);
   } else {
     lava_warn("%s","Function InterpreterDoArithmetic address is not in 0-2GB");
  dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic))>>32));
   }
#line 1148 "src/interpreter/bytecode-interpreter.dasc"
  //|  ret2acc
  dasm_put(Dst, 234, VALUE_FAIL);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 107);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 120);
#line 1149 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_POW_SLOWRV:
  //|->InterpPowSlowRV:
  //|  savepc
  //|  mov CARG1, RUNTIME
  //|  LdRealV CARG2,ARG2F
  //|  mov CARG3,qword [STK+ARG3F*8]
  //|  lea CARG4, [STK+ARG1F*8]
  //|  fcall InterpreterPow
  dasm_put(Dst, 314,  INTERP_POW_SLOWRV, RuntimeLayout::kCurPCOffset, PrototypeLayout::kRealTableOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterPow))) {
  dasm_put(Dst, 345);
   } else {
     lava_warn("%s","Function InterpreterPow address is not in 0-2GB");
  dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterPow)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterPow))>>32));
   }
#line 1158 "src/interpreter/bytecode-interpreter.dasc"
  //|  ret2acc
  dasm_put(Dst, 234, VALUE_FAIL);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 107);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 120);
#line 1159 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_POW_SLOWVR:
  //|->InterpPowSlowVR:
  //|  savepc
  //|  mov CARG1, RUNTIME
  //|  mov CARG2, qword [STK+ARG2F*8]
  //|  LdRealV CARG3,ARG3F
  //|  lea CARG4, [STK+ARG1F*8]
  //|  fcall InterpreterPow
  dasm_put(Dst, 350,  INTERP_POW_SLOWVR, RuntimeLayout::kCurPCOffset, PrototypeLayout::kRealTableOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterPow))) {
  dasm_put(Dst, 345);
   } else {
     lava_warn("%s","Function InterpreterPow address is not in 0-2GB");
  dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterPow)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterPow))>>32));
   }
#line 1168 "src/interpreter/bytecode-interpreter.dasc"
  //|  ret2acc
  dasm_put(Dst, 234, VALUE_FAIL);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 107);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 120);
#line 1169 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_POW_SLOWVV:
  //|->InterpPowSlowVV:
  //|  savepc
  //|  instr_D
  //|  mov CARG1, RUNTIME
  //|  mov CARG2, qword [STK+ARG2F*8]
  //|  mov CARG3, qword [STK+ARG3F*8]
  //|  lea CARG4, [STK+ARG1F*8]
  //|  fcall InterpreterPow
  dasm_put(Dst, 381,  INTERP_POW_SLOWVV, RuntimeLayout::kCurPCOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterPow))) {
  dasm_put(Dst, 345);
   } else {
     lava_warn("%s","Function InterpreterPow address is not in 0-2GB");
  dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterPow)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterPow))>>32));
   }
#line 1179 "src/interpreter/bytecode-interpreter.dasc"
  //|  ret2acc
  dasm_put(Dst, 234, VALUE_FAIL);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 107);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 120);
#line 1180 "src/interpreter/bytecode-interpreter.dasc"

  /* -------------------------------------------
   * Interp Arithmetic Exception               |
   * ------------------------------------------*/
  //|=> DIV_BY_ZERO:
  //|->DivByZero:
  //|  mov CARG1,RUNTIME
  //|  lea CARG2,[PC-4]
  //|  fcall InterpreterDivByZero
  dasm_put(Dst, 422,  DIV_BY_ZERO);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDivByZero))) {
  dasm_put(Dst, 434);
   } else {
     lava_warn("%s","Function InterpreterDivByZero address is not in 0-2GB");
  dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDivByZero)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDivByZero))>>32));
   }
#line 1189 "src/interpreter/bytecode-interpreter.dasc"
  //|  jmp ->InterpFail
  dasm_put(Dst, 439);
#line 1190 "src/interpreter/bytecode-interpreter.dasc"

  /* -------------------------------------------
   * Interp Comparison                         |
   * ------------------------------------------*/
  //|=> INTERP_COMPARERV:
  //|->InterpCompareRV:
  //|  savepc
  //|  mov CARG1,RUNTIME
  //|  LdRealV CARG2,ARG2F
  //|  mov CARG3, qword [STK+ARG2F*8]
  //|  fcall InterpreterDoCompare
  dasm_put(Dst, 444,  INTERP_COMPARERV, RuntimeLayout::kCurPCOffset, PrototypeLayout::kRealTableOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoCompare))) {
  dasm_put(Dst, 471);
   } else {
     lava_warn("%s","Function InterpreterDoCompare address is not in 0-2GB");
  dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoCompare)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoCompare))>>32));
   }
#line 1201 "src/interpreter/bytecode-interpreter.dasc"
  //|  ret2acc
  dasm_put(Dst, 234, VALUE_FAIL);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 107);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 120);
#line 1202 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_COMPAREVR:
  //|->InterpCompareVR:
  //|  savepc
  //|  mov CARG1,RUNTIME
  //|  mov CARG2,qword [STK+ARG1F*8]
  //|  LdRealV CARG3,ARG2F
  //|  fcall InterpreterDoCompare
  dasm_put(Dst, 476,  INTERP_COMPAREVR, RuntimeLayout::kCurPCOffset, PrototypeLayout::kRealTableOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoCompare))) {
  dasm_put(Dst, 471);
   } else {
     lava_warn("%s","Function InterpreterDoCompare address is not in 0-2GB");
  dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoCompare)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoCompare))>>32));
   }
#line 1210 "src/interpreter/bytecode-interpreter.dasc"
  //|  ret2acc
  dasm_put(Dst, 234, VALUE_FAIL);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 107);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 120);
#line 1211 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_COMPAREVV:
  //|->InterpCompareVV:
  //|  savepc
  //|  mov CARG1, RUNTIME
  //|  mov CARG2, qword [STK+ARG1F*8]
  //|  mov CARG3, qword [STK+ARG2F*8]
  //|  fcall InterpreterDoCompare
  dasm_put(Dst, 503,  INTERP_COMPAREVV, RuntimeLayout::kCurPCOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoCompare))) {
  dasm_put(Dst, 471);
   } else {
     lava_warn("%s","Function InterpreterDoCompare address is not in 0-2GB");
  dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoCompare)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoCompare))>>32));
   }
#line 1219 "src/interpreter/bytecode-interpreter.dasc"
  //|  ret2acc
  dasm_put(Dst, 234, VALUE_FAIL);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 107);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 120);
#line 1220 "src/interpreter/bytecode-interpreter.dasc"

  /* -------------------------------------------------
   * Property Get/Set                                |
   * ------------------------------------------------*/
  //|=> INTERP_PROPGET:
  //|->InterpPropGet:
  //|  savepc
  //|  mov CARG1, RUNTIME
  //|  mov CARG2, qword [STK+ARG1F*8]
  //|  LdStr CARG3, ARG2F
  //|  fcall InterpreterDoPropGet
  dasm_put(Dst, 524,  INTERP_PROPGET, RuntimeLayout::kCurPCOffset, PrototypeLayout::kStringTableOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoPropGet))) {
  dasm_put(Dst, 553);
   } else {
     lava_warn("%s","Function InterpreterDoPropGet address is not in 0-2GB");
  dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoPropGet)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoPropGet))>>32));
   }
#line 1231 "src/interpreter/bytecode-interpreter.dasc"
  //|  ret2acc
  dasm_put(Dst, 234, VALUE_FAIL);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 107);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 120);
#line 1232 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_PROPGET_NOTFOUND:
  //|->InterpPropGetNotFound:
  //|  savepc
  //|  mov CARG1, RUNTIME
  //|  mov CARG2, qword [STK+ARG1F*8]
  //|  LdStr CARG3,ARG1F
  //|  fcall InterpreterPropGetNotFound
  dasm_put(Dst, 558,  INTERP_PROPGET_NOTFOUND, RuntimeLayout::kCurPCOffset, PrototypeLayout::kStringTableOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterPropGetNotFound))) {
  dasm_put(Dst, 587);
   } else {
     lava_warn("%s","Function InterpreterPropGetNotFound address is not in 0-2GB");
  dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterPropGetNotFound)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterPropGetNotFound))>>32));
   }
#line 1240 "src/interpreter/bytecode-interpreter.dasc"
  //|  jmp ->InterpFail
  dasm_put(Dst, 439);
#line 1241 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_PROPGET_NOTOBJECT:
  //|->InterpPropGetNotObject:
  //|  savepc
  //|  mov CARG1, RUNTIME
  //|  mov CARG2, qword [STK+ARG1F*8]
  //|  LdStr CARG3, ARG2F
  //|  fcall InterpreterPropGetNotObject
  dasm_put(Dst, 592,  INTERP_PROPGET_NOTOBJECT, RuntimeLayout::kCurPCOffset, PrototypeLayout::kStringTableOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterPropGetNotObject))) {
  dasm_put(Dst, 621);
   } else {
     lava_warn("%s","Function InterpreterPropGetNotObject address is not in 0-2GB");
  dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterPropGetNotObject)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterPropGetNotObject))>>32));
   }
#line 1249 "src/interpreter/bytecode-interpreter.dasc"
  //|  jmp ->InterpFail
  dasm_put(Dst, 439);
#line 1250 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_IDXGETI:
  //|->InterpIdxGetI:
  //|  savepc
  //|  mov CARG1, RUNTIME
  //|  mov CARG2, qword [STK+ARG1F*8]
  //|  mov CARG3L , ARG2
  //|  fcall InterpreterDoIdxGetI
  dasm_put(Dst, 626,  INTERP_IDXGETI, RuntimeLayout::kCurPCOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoIdxGetI))) {
  dasm_put(Dst, 645);
   } else {
     lava_warn("%s","Function InterpreterDoIdxGetI address is not in 0-2GB");
  dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoIdxGetI)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoIdxGetI))>>32));
   }
#line 1258 "src/interpreter/bytecode-interpreter.dasc"
  //|  retbool
  dasm_put(Dst, 650);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 107);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 120);
#line 1259 "src/interpreter/bytecode-interpreter.dasc"
}

void GenerateOneBytecode( BuildContext* bctx, Bytecode bc ) {
  switch(bc) {
    /** =====================================================
     *  Call handling                                       |
     *  ====================================================*/
    case BC_RETNULL:
      //|=> bc:
      //|  instr_X
      //|  mov dword [STK+ACCFIDX],Value::FLAG_NULL
      //|  jmp ->InterpReturn
      dasm_put(Dst, 667,  bc, Value::FLAG_NULL);
#line 1271 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_RET:
      //|=> bc:
      //|  instr_X
      //|  jmp ->InterpReturn
      dasm_put(Dst, 682,  bc);
#line 1277 "src/interpreter/bytecode-interpreter.dasc"
      break;

    /** =====================================================
     *  Register Move                                       |
     *  ====================================================*/
    case BC_MOVE:
      //|=> bc:
      //|  instr_E
      //|  mov ARG3F,qword [STK+ARG2F*8]
      //|  mov qword [STK+ARG1F*8],ARG3F
      //|  Dispatch
      dasm_put(Dst, 688,  bc);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1288 "src/interpreter/bytecode-interpreter.dasc"
      break;

    /** =====================================================
     *  Constant Loading                                    |
     *  ====================================================*/
    case BC_LOAD0:
      //|=> bc:
      //|  instr_F
      //|  xorpd xmm0,xmm0
      //|  movsd qword[STK+ARG1F*8], xmm0
      //|  Dispatch
      dasm_put(Dst, 714,  bc);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1299 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_LOAD1:
      //|=> bc:
      //|  instr_F
      //|  LdRConst_one xmm0
      //|  movsd qword[STK+ARG1F*8], xmm0
      //|  Dispatch
      dasm_put(Dst, 740,  bc, (unsigned int)((static_cast<std::uint64_t>(0x3ff00000)<<32)), (unsigned int)(((static_cast<std::uint64_t>(0x3ff00000)<<32))>>32));
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1307 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_LOADN1:
      //|=> bc:
      //|  instr_F
      //|  LdRConst_neg_one xmm0
      //|  movsd qword[STK+ARG1F*8], xmm0
      //|  Dispatch
      dasm_put(Dst, 740,  bc, (unsigned int)((static_cast<std::uint64_t>(0xbff00000)<<32)), (unsigned int)(((static_cast<std::uint64_t>(0xbff00000)<<32))>>32));
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1315 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_LOADR:
      //|=> bc:
      //|  instr_E
      //|  LdReal xmm0,ARG2F
      //|  movsd qword [STK+ARG1F*8],xmm0
      //|  Dispatch
      dasm_put(Dst, 771,  bc, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1323 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_LOADNULL:
      //|=> bc:
      //|  instr_F
      //|  mov dword [STK+ARG1F*8+4],Value::FLAG_NULL
      //|  Dispatch
      dasm_put(Dst, 809,  bc, Value::FLAG_NULL);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1330 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_LOADTRUE:
      //|=> bc:
      //|  instr_F
      //|  mov dword [STK+ARG1F*8+4],Value::FLAG_TRUE
      //|  Dispatch
      dasm_put(Dst, 809,  bc, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1337 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_LOADFALSE:
      //|=> bc:
      //|  instr_F
      //|  mov dword [STK+ARG1F*8+4],Value::FLAG_FALSE
      //|  Dispatch
      dasm_put(Dst, 809,  bc, Value::FLAG_FALSE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1344 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_LOADSTR:
      //|=> bc:
      //|  instr_E
      //|  LdStrV LREG,ARG2F
      //|  mov qword [STK+ARG1F*8],LREG
      //|  Dispatch
      dasm_put(Dst, 830,  bc, PrototypeLayout::kStringTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1352 "src/interpreter/bytecode-interpreter.dasc"
      break;

    // -------------------------------------------------
    // Loading List/Object/Clousure
    //
    //
    // For these bytecodes, no optimization is performed but
    // directly yield back to C++ function to do the job. It
    // has no points to optimize these cases since they are
    // small amount of bytecodes and also complicated to write
    // in assembly without too much gain
    //
    //
    // To reduce ICache stress, the decode routine is *not*
    // placed inlined with each BC_XXX here but in the slow
    // path. This is purposely to make the main part of
    // interpreter to be small which helps about the icache
    // footprint of hot code
    // ------------------------------------------------*/
    case BC_LOADLIST0:
      //|=> bc:
      //|  instr_F
      //|  savepc
      //|  mov CARG1,RUNTIME
      //|  lea CARG2,[STK+ARG1F*8]
      //|  fcall InterpreterDoLoadList0
      dasm_put(Dst, 869,  bc, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoLoadList0))) {
      dasm_put(Dst, 887);
       } else {
         lava_warn("%s","Function InterpreterDoLoadList0 address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoLoadList0)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoLoadList0))>>32));
       }
#line 1378 "src/interpreter/bytecode-interpreter.dasc"
      //|  retbool
      dasm_put(Dst, 650);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1379 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_LOADLIST1:
      //|=>bc:
      //|  instr_E
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  lea CARG2, [STK+ARG1F*8]
      //|  mov CARG3, qword [STK+ARG2F*8]
      //|  fcall InterpreterDoLoadList1
      dasm_put(Dst, 892, bc, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoLoadList1))) {
      dasm_put(Dst, 917);
       } else {
         lava_warn("%s","Function InterpreterDoLoadList1 address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoLoadList1)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoLoadList1))>>32));
       }
#line 1388 "src/interpreter/bytecode-interpreter.dasc"
      //|  retbool
      dasm_put(Dst, 650);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1389 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_LOADLIST2:
      //|=>bc:
      //|  instr_D
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  lea CARG2, [STK+ARG1F*8]
      //|  mov CARG3, qword [STK+ARG2F*8]
      //|  mov CARG4, qword [STK+ARG3F*8]
      //|  fcall InterpreterDoLoadList2
      dasm_put(Dst, 922, bc, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoLoadList2))) {
      dasm_put(Dst, 961);
       } else {
         lava_warn("%s","Function InterpreterDoLoadList2 address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoLoadList2)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoLoadList2))>>32));
       }
#line 1399 "src/interpreter/bytecode-interpreter.dasc"
      //|  retbool
      dasm_put(Dst, 650);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1400 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_NEWLIST:
      //|=>bc:
      //|  jmp ->InterpNewList
      //|  instr_B
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  lea CARG2, [STK+ARG1F*8]
      //|  mov CARG3L, ARG2
      //|  fcall InterpreterDoNewList
      dasm_put(Dst, 966, bc, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoNewList))) {
      dasm_put(Dst, 994);
       } else {
         lava_warn("%s","Function InterpreterDoNewList address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoNewList)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoNewList))>>32));
       }
#line 1410 "src/interpreter/bytecode-interpreter.dasc"
      //|  retbool
      dasm_put(Dst, 650);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1411 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_ADDLIST:
      //|=>bc:
      //|  instr_E
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  lea CARG2, [STK+ARG1F*8]
      //|  mov CARG3, qword [STK+ARG2F*8]
      //|  fcall InterpreterDoAddList
      dasm_put(Dst, 892, bc, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoAddList))) {
      dasm_put(Dst, 999);
       } else {
         lava_warn("%s","Function InterpreterDoAddList address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoAddList)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoAddList))>>32));
       }
#line 1420 "src/interpreter/bytecode-interpreter.dasc"
      //|  retbool
      dasm_put(Dst, 650);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1421 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_LOADOBJ0:
      //|=>bc:
      //|  instr_F
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  lea CARG2, [STK+ARG1F*8]
      //|  fcall InterpreterDoLoadObj0
      dasm_put(Dst, 869, bc, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoLoadObj0))) {
      dasm_put(Dst, 1004);
       } else {
         lava_warn("%s","Function InterpreterDoLoadObj0 address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoLoadObj0)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoLoadObj0))>>32));
       }
#line 1429 "src/interpreter/bytecode-interpreter.dasc"
      //|  retbool
      dasm_put(Dst, 650);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1430 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_LOADOBJ1:
      //|=>bc:
      //|  instr_D
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  lea CARG2, [STK+ARG1F*8]
      //|  mov CARG3, qword [STK+ARG2F*8]
      //|  mov CARG4, qword [STK+ARG3F*8]
      //|  fcall InterpreterDoLoadObj1
      dasm_put(Dst, 922, bc, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoLoadObj1))) {
      dasm_put(Dst, 1009);
       } else {
         lava_warn("%s","Function InterpreterDoLoadObj1 address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoLoadObj1)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoLoadObj1))>>32));
       }
#line 1440 "src/interpreter/bytecode-interpreter.dasc"
      //|  retbool
      dasm_put(Dst, 650);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1441 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_NEWOBJ:
      //|=>bc:
      //|  instr_B
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  lea CARG2, [STK+ARG1F*8]
      //|  mov CARG3L, ARG2
      //|  fcall InterpreterDoNewObj
      dasm_put(Dst, 1014, bc, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoNewObj))) {
      dasm_put(Dst, 1038);
       } else {
         lava_warn("%s","Function InterpreterDoNewObj address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoNewObj)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoNewObj))>>32));
       }
#line 1450 "src/interpreter/bytecode-interpreter.dasc"
      //|  retbool
      dasm_put(Dst, 650);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1451 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_ADDOBJ:
      //|=>bc:
      //|  instr_D
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  lea CARG2, [STK+ARG1F*8]
      //|  mov CARG3, qword [STK+ARG2F*8]
      //|  mov CARG4, qword [STK+ARG3F*8]
      //|  fcall InterpreterDoAddObj
      dasm_put(Dst, 922, bc, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoAddObj))) {
      dasm_put(Dst, 1043);
       } else {
         lava_warn("%s","Function InterpreterDoAddObj address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoAddObj)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoAddObj))>>32));
       }
#line 1461 "src/interpreter/bytecode-interpreter.dasc"
      //|  retbool
      dasm_put(Dst, 650);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1462 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_LOADCLS:
      //|=>bc:
      //|  instr_C
      //|  savepc
      //|  mov CARG1,RUNTIME
      //|  mov CARG2, STK
      //|  mov CARG3L, ARG2
      //|  lea CARG4 , [STK+ARG1F*8]
      //|  fcall InterpreterDoLoadCls
      dasm_put(Dst, 1048, bc, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoLoadCls))) {
      dasm_put(Dst, 1076);
       } else {
         lava_warn("%s","Function InterpreterDoLoadCls address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoLoadCls)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoLoadCls))>>32));
       }
#line 1472 "src/interpreter/bytecode-interpreter.dasc"
      //|  retbool
      dasm_put(Dst, 650);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1473 "src/interpreter/bytecode-interpreter.dasc"
      break;

    /** =====================================================
     *  Arith XV                                            |
     *  ====================================================*/
    //|.macro arith_rv,BC,slow_path,instr
    //|  instr_D
    //|  mov RREG, qword [STK+ARG3F*8]

    // Use arg3 as temporary in favoer over T1/T2
    //|.if CHECK_NUMBER_MEMORY
    //|  mov T0L, dword[STK+ARG3F*8+4]
    //|.else
    //|  mov T0,RREG
    //|  shr T0,32
    //|.endif

    //|  cmp T0L, Value::FLAG_REAL
    //|  jnb ->slow_path

    //|  LdReal xmm0,ARG2F
    //|  movd xmm1, RREG
    //|  instr xmm0,xmm1
    //|  StReal ARG1F,xmm0
    //|  Dispatch
    //|.endmacro

    case BC_ADDRV:
      //|=>bc:
      //|  arith_rv BC_ADDRV,InterpArithRealL,addsd
      dasm_put(Dst, 1081, bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1503 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_SUBRV:
      //|=>bc:
      //|  arith_rv BC_SUBRV,InterpArithRealL,subsd
      dasm_put(Dst, 1159, bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1508 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_MULRV:
      //|=>bc:
      //|  arith_rv BC_MULRV,InterpArithRealL,mulsd
      dasm_put(Dst, 1237, bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1513 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_DIVRV:
      //|=>bc:
      //|  arith_rv BC_DIVRV,InterpArithRealL,divsd
      dasm_put(Dst, 1315, bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1518 "src/interpreter/bytecode-interpreter.dasc"
      break;

    /* =========================================================
     * Arith VX                                                |
     * ========================================================*/
    //|.macro arith_vr,BC,slow_path,instr
    //|  instr_D
    //|  mov LREG,qword [STK+ARG2F*8]

    //|.if CHECK_NUMBER_MEMORY
    //|  mov T0L ,dword [STK+ARG2F*8+4]
    //|.else
    //|  mov T0,LREG
    //|  shr T0,32
    //|.endif

    //|  cmp T0L,Value::FLAG_REAL
    //|  jnb ->slow_path

    //|  movd xmm0,LREG
    //|  LdReal xmm1,ARG3F
    //|  instr xmm0,xmm1
    //|  StReal ARG1F,xmm0
    //|  Dispatch
    //|.endmacro


    case BC_ADDVR:
      //|=> bc:
      //|  arith_vr BC_ADDVR,InterpArithRealR,addsd
      dasm_put(Dst, 1393,  bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1548 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_SUBVR:
      //|=> bc:
      //|  arith_vr BC_SUBVR,InterpArithRealR,subsd
      dasm_put(Dst, 1472,  bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1553 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_MULVR:
      //|=> bc:
      //|  arith_vr BC_MULVR,InterpArithRealR,mulsd
      dasm_put(Dst, 1551,  bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1558 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_DIVVR:
      //|=> bc:
      //|  arith_vr BC_DIVVR,InterpArithRealR,divsd
      dasm_put(Dst, 1630,  bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1563 "src/interpreter/bytecode-interpreter.dasc"
      break;

    /* ========================================================
     * ArithVV
     *
     * The arithVV is also optimized for common path here.
     * We inline all numeric calculation cases, int/real.
     * Other cases will be pushed back to call C++ function
     * which may be extended to support meta function call
     *
     * ========================================================*/
    // perform VV calaculation based on instruction
    //|.macro arith_vv,BC,instrR
    //|=> BC:
    //|  instr_D

    // check the lhs to be integer or not
    //|  mov LREG,qword [STK+ARG2F*8]
    //|  cmp LREGL,Value::FLAG_REAL
    //|  jnb ->InterpArithVV

    //| // real && xx
    //|  mov RREG,qword [STK+ARG3F*8]
    //|  cmp RREGL,Value::FLAG_REAL
    //|  jnb ->InterpArithVV

    //|  movsd xmm0, qword [STK+ARG2F*8]
    //|  instrR xmm0, qword [STK+ARG3F*8]
    //|  StReal ARG1F,xmm0
    //|  Dispatch
    //|.endmacro

    case BC_ADDVV:
      //|  arith_vv BC_ADDVV,addsd
      dasm_put(Dst, 1709,  BC_ADDVV, Value::FLAG_REAL, Value::FLAG_REAL);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1597 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_SUBVV:
      //|  arith_vv BC_SUBVV,subsd
      dasm_put(Dst, 1782,  BC_SUBVV, Value::FLAG_REAL, Value::FLAG_REAL);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1600 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_MULVV:
      //|  arith_vv BC_MULVV,mulsd
      dasm_put(Dst, 1855,  BC_MULVV, Value::FLAG_REAL, Value::FLAG_REAL);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1603 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_DIVVV:
      //|  arith_vv BC_DIVVV,divsd
      dasm_put(Dst, 1928,  BC_DIVVV, Value::FLAG_REAL, Value::FLAG_REAL);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1606 "src/interpreter/bytecode-interpreter.dasc"
      break;

    /* ========================================================
     * MODXX
     *
     *   Similar implementation like Lua not Luajit. Return casted
     *   integer's mod value instead of fmod style value. BTW, fmod
     *   is really slow
     *
     * ========================================================*/


    case BC_MODVR:
      //|=>bc:
      //|  instr_D
      dasm_put(Dst, 2001, bc);
#line 1621 "src/interpreter/bytecode-interpreter.dasc"

      //|  cmp dword [STK+ARG2F*8+4], Value::FLAG_REAL
      //|  jnb ->InterpArithVV
      dasm_put(Dst, 2019, Value::FLAG_REAL);
#line 1624 "src/interpreter/bytecode-interpreter.dasc"

      //|  LdReal2Int ARG3,ARG3F,T0
      //|  cvtsd2si eax ,qword [STK+ARG2F*8]  // ARG2F == rax
      dasm_put(Dst, 2031, PrototypeLayout::kRealTableOffset);
#line 1627 "src/interpreter/bytecode-interpreter.dasc"

      //|.if CHECK_DIV_BY_ZERO
      //|  test ARG3,ARG3
      //|  je ->DivByZero
      //|.endif
      dasm_put(Dst, 2051);
#line 1632 "src/interpreter/bytecode-interpreter.dasc"

      //|  cdq
      //|  idiv     ARG3
      //|  StRealFromInt ARG1F,edx
      //|  Dispatch
      dasm_put(Dst, 2058);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1637 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_MODRV:
      //|=>bc:
      //|  instr_D
      dasm_put(Dst, 2001, bc);
#line 1642 "src/interpreter/bytecode-interpreter.dasc"

      //|  cmp dword [STK+ARG3F*8+4], Value::FLAG_REAL
      //|  jnb ->InterpArithVV
      dasm_put(Dst, 2086, Value::FLAG_REAL);
#line 1645 "src/interpreter/bytecode-interpreter.dasc"

      //|  LdReal2Int eax,ARG2F,T0  // ARG2F == rax
      //|  cvtsd2si ARG3 ,qword [STK+ARG3F*8]
      dasm_put(Dst, 2098, PrototypeLayout::kRealTableOffset);
#line 1648 "src/interpreter/bytecode-interpreter.dasc"

      //|.if CHECK_DIV_BY_ZERO
      //|  test ARG3,ARG3
      //|  je ->DivByZero
      //|.endif
      dasm_put(Dst, 2051);
#line 1653 "src/interpreter/bytecode-interpreter.dasc"

      //|  cdq
      //|  idiv ARG3
      //|  StRealFromInt ARG1F,edx
      //|  Dispatch
      dasm_put(Dst, 2058);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1658 "src/interpreter/bytecode-interpreter.dasc"

      break;

    case BC_MODVV:
      //|=>bc :
      //|  instr_D
      //|  cmp dword [STK+ARG2F*8+4] , Value::FLAG_REAL
      //|  jnb ->InterpArithVV
      dasm_put(Dst, 2118, bc , Value::FLAG_REAL);
#line 1666 "src/interpreter/bytecode-interpreter.dasc"

      //|  cmp dword [STK+ARG3F*8+4] , Value::FLAG_REAL
      //|  jnb ->InterpArithVV
      dasm_put(Dst, 2086, Value::FLAG_REAL);
#line 1669 "src/interpreter/bytecode-interpreter.dasc"

      //|  cvtsd2si eax, qword [STK+ARG2F*8]  // ARG2F == rax
      //|  cvtsd2si ARG3,qword [STK+ARG3F*8]
      dasm_put(Dst, 2147);
#line 1672 "src/interpreter/bytecode-interpreter.dasc"

      //|.if CHECK_DIV_BY_ZERO
      //|  test ARG3,ARG3
      //|  je ->DivByZero
      //|.endif
      dasm_put(Dst, 2051);
#line 1677 "src/interpreter/bytecode-interpreter.dasc"

      //|  cdq
      //|  idiv ARG3
      //|  StRealFromInt ARG1F,edx
      //|  Dispatch
      dasm_put(Dst, 2058);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1682 "src/interpreter/bytecode-interpreter.dasc"
      break;

    /* ==============================================================
     * POW part
     *
     * Currently we directly use std::pow/pow in libc for simplicity.
     * For numeric type we will directly call pow for other types
     * we will fallback to slow C++ function
     * =============================================================*/

    //|.macro call_pow
    //|  fcall pow
    //|  movsd qword [STK+ARG1F*8], xmm0  // ARG1F is callee saved
    //|.endmacro

    //|.macro arith_pow,REGL,XREG,ARG,slow_pow
    //|  mov REGL,dword [STK+ARG*8+4]
    //|  cmp REGL,Value::FLAG_REAL
    //|  jnb ->slow_pow
    //|  movsd XREG,qword [STK+ARG*8]
    //|  call_pow
    //|  Dispatch
    //|.endmacro

    case BC_POWRV:
      //|=> bc:
      //|  instr_D
      //|  LdReal xmm0,ARG2F
      //|  arith_pow RREGL,xmm1,ARG3F,InterpPowSlowRV
      dasm_put(Dst, 2162,  bc, PrototypeLayout::kRealTableOffset, Value::FLAG_REAL);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(pow))) {
      dasm_put(Dst, 2213);
       } else {
         lava_warn("%s","Function pow address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(pow)), (unsigned int)((reinterpret_cast<std::uintptr_t>(pow))>>32));
       }
      dasm_put(Dst, 722);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1711 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_POWVR:
      //|=> bc:
      //|  instr_D
      //|  LdReal xmm1,ARG3F
      //|  arith_pow LREGL,xmm0,ARG2F,InterpPowSlowVR
      dasm_put(Dst, 2218,  bc, PrototypeLayout::kRealTableOffset, Value::FLAG_REAL);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(pow))) {
      dasm_put(Dst, 2213);
       } else {
         lava_warn("%s","Function pow address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(pow)), (unsigned int)((reinterpret_cast<std::uintptr_t>(pow))>>32));
       }
      dasm_put(Dst, 722);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 1718 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_POWVV:
      //|=> bc:
      //|  jmp ->InterpPowSlowVV
      dasm_put(Dst, 2269,  bc);
#line 1723 "src/interpreter/bytecode-interpreter.dasc"
      break;


    /* ====================================================================
     * Comparison
     *
     * Inline numeric comparison and also do promotion inline
     * ===================================================================*/

    /* --------------------------------------------------------------------
     * Comparison XV                                                      |
     * -------------------------------------------------------------------*/
    //|.macro comp_xv,BC,slow_path,false_jmp
    //|  instr_D
    //|  mov RREG, qword[STK+ARG3F*8]

    //|.if CHECK_NUMBER_MEMORY
    //|  mov T0L , dword[STK+ARG3F*8+4]
    //|.else
    //|  mov T0,RREG
    //|  shr T0,32
    //|.endif

    //|  cmp T0L, Value::FLAG_REAL
    //|  jnb ->slow_path
    //|  LdReal xmm0, ARG2F
    //|  movd xmm1, RREG
    //|  ucomisd xmm0, xmm1
    // cmov instruction is slower here , doesn't worth it
    //|  false_jmp >1
    //|  mov dword[STK+ARG1F*8+4], Value::FLAG_TRUE
    //|2:
    //|  Dispatch
    //|1:
    //|  mov dword[STK+ARG1F*8+4], Value::FLAG_FALSE
    //|  jmp <2
    //|.endmacro

    case BC_LTRV:
      //|=>bc:
      //|  comp_xv BC_LTRV,InterpCompareRV,jae
      dasm_put(Dst, 2275, bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2357, Value::FLAG_FALSE);
#line 1764 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_LERV:
      //|=> bc:
      //|  comp_xv BC_LERV,InterpCompareRV,ja
      dasm_put(Dst, 2389,  bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2357, Value::FLAG_FALSE);
#line 1768 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_GTRV:
      //|=>bc:
      //|  comp_xv BC_GTRV,InterpCompareRV,jbe
      dasm_put(Dst, 2471, bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2357, Value::FLAG_FALSE);
#line 1772 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_GERV:
      //|=> bc:
      //|  comp_xv BC_GERV,InterpCompareRV,jb
      dasm_put(Dst, 2553,  bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2357, Value::FLAG_FALSE);
#line 1776 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_EQRV:
      //|=> bc:
      //|  comp_xv BC_EQRV,InterpCompareRV,jne
      dasm_put(Dst, 2635,  bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2357, Value::FLAG_FALSE);
#line 1780 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_NERV:
      //|=> bc:
      //|  comp_xv BC_NERV,InterpCompareRV,je
      dasm_put(Dst, 2717,  bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2357, Value::FLAG_FALSE);
#line 1784 "src/interpreter/bytecode-interpreter.dasc"
      break;

    /* --------------------------------------------------------------------
     * Comparison VX                                                      |
     * -------------------------------------------------------------------*/
    //|.macro comp_vx,BC,slow_path,false_jmp
    //|  instr_D

    //|  mov LREG,qword [STK+ARG2F*8]

    //|.if CHECK_NUMBER_MEMORY
    //|  mov T0L, dword [STK+ARG2F*8+4]
    //|.else
    //|  mov T0, RREG
    //|  shr T0, 32
    //|.endif

    //|  cmp T0L, Value::FLAG_REAL
    //|  jnb ->slow_path

    //|  LdReal xmm1,ARG3F
    //|  movd xmm0, LREG
    //|  ucomisd xmm0,xmm1
    //|  false_jmp >1
    //|  mov dword [STK+ARG1F*8+4], Value::FLAG_TRUE
    //|2:
    //|  Dispatch
    //|1:
    //|  mov dword [STK+ARG1F*8+4], Value::FLAG_FALSE
    //|  jmp <2
    //|.endmacro

    case BC_LTVR:
      //|=>bc:
      //|  comp_vx BC_LTVR,InterpCompareVR,jae
      dasm_put(Dst, 2799, bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2357, Value::FLAG_FALSE);
#line 1819 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_LEVR:
      //|=>bc:
      //|  comp_vx BC_LEVR,InterpCompareVR,ja
      dasm_put(Dst, 2881, bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2357, Value::FLAG_FALSE);
#line 1823 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_GTVR:
      //|=>bc:
      //|  comp_vx BC_GTVR,InterpCompareVR,jbe
      dasm_put(Dst, 2963, bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2357, Value::FLAG_FALSE);
#line 1827 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_GEVR:
      //|=>bc:
      //|  comp_vx BC_GEVR,InterpCompareVR,jb
      dasm_put(Dst, 3045, bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2357, Value::FLAG_FALSE);
#line 1831 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_EQVR:
      //|=>bc:
      //|  comp_vx BC_EQVR,InterpCompareVR,jne
      dasm_put(Dst, 3127, bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2357, Value::FLAG_FALSE);
#line 1835 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_NEVR:
      //|=>bc:
      //|  comp_vx BC_NEVR,InterpCompareVR,je
      dasm_put(Dst, 3209, bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2357, Value::FLAG_FALSE);
#line 1839 "src/interpreter/bytecode-interpreter.dasc"
      break;


    /* --------------------------------------------------------
     * comparison VV
     *
     * We do inline type promotion and comparison for all
     * numeric type
     * --------------------------------------------------------*/
    //|.macro comp_vv,BC,false_jmp
    //|  instr_D

    //|  cmp dword [STK+ARG2F*8+4], Value::FLAG_REAL
    //|  jnb ->InterpCompareVV

    //|  cmp dword [STK+ARG3F*8+4], Value::FLAG_REAL
    //|  jnb ->InterpCompareVV

    //|  movsd xmm0, qword [STK+ARG2F*8]
    //|  ucomisd xmm0, qword  [STK+ARG3F*8]
    //|  false_jmp >1
    //|  mov dword [STK+ARG1F*8+4], Value::FLAG_TRUE
    //|2:
    //|  Dispatch
    //|1:
    //|  mov dword [STK+ARG1F*8+4], Value::FLAG_FALSE
    //|  jmp <2
    //|.endmacro

    case BC_LTVV:
      //|=>bc:
      //|  comp_vv,BC_LTVV,jae
      dasm_put(Dst, 3291, bc, Value::FLAG_REAL, Value::FLAG_REAL, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2357, Value::FLAG_FALSE);
#line 1871 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_LEVV:
      //|=>bc:
      //|  comp_vv,BC_LEVV,ja
      dasm_put(Dst, 3366, bc, Value::FLAG_REAL, Value::FLAG_REAL, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2357, Value::FLAG_FALSE);
#line 1875 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_GTVV:
      //|=>bc:
      //|  comp_vv,BC_GTVV,jbe
      dasm_put(Dst, 3441, bc, Value::FLAG_REAL, Value::FLAG_REAL, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2357, Value::FLAG_FALSE);
#line 1879 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_GEVV:
      //|=>bc:
      //|  comp_vv,BC_GEVV,jb
      dasm_put(Dst, 3516, bc, Value::FLAG_REAL, Value::FLAG_REAL, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2357, Value::FLAG_FALSE);
#line 1883 "src/interpreter/bytecode-interpreter.dasc"
      break;

    //|.macro comp_eqne_vv,BC,T,F
    //|  instr_D
    // We fast check numeric number's value. Pay attension that bit comparison
    // is not okay due to the fact we have +0 and -0, we need to do the conversion
    // to do the correct comparison
    //|  cmp dword [STK+ARG2F*8+4], Value::FLAG_REAL
    //|  jnb >3

    //|  cmp dword [STK+ARG2F*8+4], Value::FLAG_REAL
    //|  jnb >3

    //|  movsd xmm0, qword [STK+ARG2F*8]
    //|  ucomisd xmm0, qword [STK+ARG3F*8]
    //|  je >1
    //|  mov dword [STK+ARG1F*8+4], T
    //|2:
    //|  Dispatch
    //|1:
    //|  mov dword [STK+ARG1F*8+4], F
    //|  jmp <2

    // Here we mainly do a comparison between other primitive types
    //|3:
    //|  mov LREG, qword [STK+ARG2F*8]
    //|  mov RREG, qword [STK+ARG3F*8]
    //|  mov T0  , LREG
    //|  mov T1  , RREG
    //|  shr LREG, 48
    //|  shr RREG, 48

    //|  cmp LREG, RREG
    //|  je >4

    // LREG and RREG doesn't match, we need to rule out heap type to
    // actually tell whether LREGL and RREGL are the same or not
    //|  cmp LREGL, Value::FLAG_HEAP
    //|  je >5
    //|  cmp RREGL, Value::FLAG_HEAP
    //|  je >5

    // Okay, both LREGL an RREGL are not heap tag, so we can assert
    // they are *not* equal due to they are primitive type
    //|  mov dword [STK+ARG1F*8+4], F
    //|  jmp <2

    // Primitive type are *identical* return TRUE
    //|4:
    //|  mov dword [STK+ARG1F*8+4], T
    //|  jmp <2

    // When we reach 5, we know at least one of the operand is a *HEAP*
    // object. We can try to inline a SSO check here or just go back to
    // InterpCompareVV to do the job
    //|5:
    //|  CheckSSORaw T0,->InterpCompareHH
    //|  CheckSSORaw T1,->InterpCompareHH
    //|  cmp T0,T1
    //|  jne >6
    //|  mov dword [STK+ARG1F*8+4], T
    //|6:
    //|  mov dword [STK+ARG1F*8+4], F
    //|  jmp <2
    //|.endmacro


    case BC_EQVV:
      //|=>bc:
      //|  comp_eqne_vv BC_EQVV,Value::FLAG_TRUE,Value::FLAG_FALSE
      dasm_put(Dst, 3591, bc, Value::FLAG_REAL, Value::FLAG_REAL, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 3666, Value::FLAG_FALSE, Value::FLAG_HEAP, Value::FLAG_HEAP, Value::FLAG_FALSE, Value::FLAG_TRUE);
      dasm_put(Dst, 3772, -HOH_TYPE_OFFSET, SSO_BIT_PATTERN, -HOH_TYPE_OFFSET, SSO_BIT_PATTERN, Value::FLAG_TRUE, Value::FLAG_FALSE);
#line 1953 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_NEVV:
      //|=>bc:
      //|  comp_eqne_vv BC_NEVV,Value::FLAG_FALSE,Value::FLAG_TRUE
      dasm_put(Dst, 3591, bc, Value::FLAG_REAL, Value::FLAG_REAL, Value::FLAG_FALSE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 3666, Value::FLAG_TRUE, Value::FLAG_HEAP, Value::FLAG_HEAP, Value::FLAG_TRUE, Value::FLAG_FALSE);
      dasm_put(Dst, 3772, -HOH_TYPE_OFFSET, SSO_BIT_PATTERN, -HOH_TYPE_OFFSET, SSO_BIT_PATTERN, Value::FLAG_FALSE, Value::FLAG_TRUE);
#line 1957 "src/interpreter/bytecode-interpreter.dasc"
      break;

    // For string equality comparison , we inline SSO comparison since
    // they are just checking the address are equal or not
    //|.macro eq_sv,BC,SlowPath,instr,false_jmp
    //|  instr_D
    //|  LdStr LREG,ARG2F
    //|  mov RREG,qword [STK+ARG3F*8]
    //|  CheckSSO LREG,1
    //|  CheckSSOV RREG,1

    //|.if USE_CMOV_COMP
    //|  mov ARG2, Value::FLAG_FALSE
    //|  cmp LREG,RREG
    //|  instr ARG2,dword [->FlagTrueConst]
    //|  mov dword [STK+ARG1F*8+4],ARG2
    //|.else
    //|  cmp LREG,RREG
    //|  false_jmp >8
    //|  mov dword [STK+ARG1F*8+4],Value::FLAG_TRUE
    //|  jmp >7
    //|8:
    //|  mov dword [STK+ARG1F*8+4],Value::FLAG_FALSE
    //|.endif

    //|7:
    //|  Dispatch

    //|1:
    //|  jmp ->SlowPath
    //|.endmacro

    //|.macro eq_vs,BC,SlowPath,instr,false_jmp
    //|  instr_D
    //|  mov LREG, qword [STK+ARG2F*8]
    //|  LdStr RREG,ARG3F
    //|  CheckSSOV LREG,1
    //|  CheckSSO  RREG,1

    //|.if USE_CMOV_COMP
    //|  mov ARG2, Value::FLAG_FALSE
    //|  cmp LREG,RREG
    //|  instr ARG2,dword [->FlagTrueConst]
    //|  mov dword [STK+ARG1F*8+4],ARG2
    //|.else
    //|  cmp LREG,RREG
    //|  false_jmp >8
    //|  mov dword [STK+ARG1F*8+4],Value::FLAG_TRUE
    //|  jmp >7
    //|8:
    //|  mov dword [STK+ARG1F*8+4],Value::FLAG_FALSE
    //|.endif

    //|7:
    //|  Dispatch

    //|1:
    //|  jmp ->SlowPath
    //|.endmacro

    case BC_EQSV:
      //|=> bc:
      //|  eq_sv BC_EQSV,InterpCompareSV,cmove,jne
      dasm_put(Dst, 3831,  bc, PrototypeLayout::kStringTableOffset, -HOH_TYPE_OFFSET, SSO_BIT_PATTERN, Value::FLAG_HEAP, -HOH_TYPE_OFFSET, SSO_BIT_PATTERN, Value::FLAG_TRUE);
      dasm_put(Dst, 3934, Value::FLAG_FALSE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 3953);
#line 2020 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_EQVS:
      //|=> bc:
      //|  eq_vs BC_EQVS,InterpCompareVS,cmove,jne
      dasm_put(Dst, 3979,  bc, PrototypeLayout::kStringTableOffset, Value::FLAG_HEAP, -HOH_TYPE_OFFSET, SSO_BIT_PATTERN, -HOH_TYPE_OFFSET, SSO_BIT_PATTERN, Value::FLAG_TRUE);
      dasm_put(Dst, 3934, Value::FLAG_FALSE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 4083);
#line 2024 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_NESV:
      //|=>bc:
      //|  eq_sv BC_NESV,InterpCompareSV,cmovne,je
      dasm_put(Dst, 4109, bc, PrototypeLayout::kStringTableOffset, -HOH_TYPE_OFFSET, SSO_BIT_PATTERN, Value::FLAG_HEAP, -HOH_TYPE_OFFSET, SSO_BIT_PATTERN, Value::FLAG_TRUE);
      dasm_put(Dst, 3934, Value::FLAG_FALSE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 3953);
#line 2028 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_NEVS:
      //|=>bc:
      //|  eq_vs BC_NEVS,InterpCompareVS,cmovne,je
      dasm_put(Dst, 4212, bc, PrototypeLayout::kStringTableOffset, Value::FLAG_HEAP, -HOH_TYPE_OFFSET, SSO_BIT_PATTERN, -HOH_TYPE_OFFSET, SSO_BIT_PATTERN, Value::FLAG_TRUE);
      dasm_put(Dst, 3934, Value::FLAG_FALSE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 4083);
#line 2032 "src/interpreter/bytecode-interpreter.dasc"
      break;

    /* -------------------------------------------------
     * Unary                                           |
     * ------------------------------------------------*/

    // inline integers and reals inlined to be processed
    // and other types are throwed away to the slower
    // functions to help since we may need to support meta
    // function in the future
    case BC_NEGATE:
      //|=> bc:
      //|  instr_E
      //|  cmp dword [STK+ARG2F*8+4], Value::FLAG_REAL
      //|  jnb >8
      dasm_put(Dst, 4316,  bc, Value::FLAG_REAL);
#line 2047 "src/interpreter/bytecode-interpreter.dasc"

      //|  movsd, xmm0, qword [STK+ARG2F*8]
      //|  LdRConst_sign xmm1
      //|  xorpd xmm0, xmm1
      //|  movsd qword [STK+ARG1F*8], xmm0
      //|  Dispatch
      dasm_put(Dst, 4335, (unsigned int)((static_cast<std::uint64_t>(0x80000000)<<32)), (unsigned int)(((static_cast<std::uint64_t>(0x80000000)<<32))>>32));
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 2053 "src/interpreter/bytecode-interpreter.dasc"

      //|8:
      //|  mov CARG1, RUNTIME
      //|  mov CARG2, qword [STK+ARG2F*8]
      //|  lea CARG3, [STK+ARG1F*8]
      //|  fcall InterpreterDoNegate
      dasm_put(Dst, 4373);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoNegate))) {
      dasm_put(Dst, 4387);
       } else {
         lava_warn("%s","Function InterpreterDoNegate address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoNegate)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoNegate))>>32));
       }
#line 2059 "src/interpreter/bytecode-interpreter.dasc"
      //|  retbool
      dasm_put(Dst, 650);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 2060 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_NOT:
      //|=> bc:
      //|  instr_E
      //|  mov ARG3, Value::FLAG_FALSE
      dasm_put(Dst, 4392,  bc, Value::FLAG_FALSE);
#line 2066 "src/interpreter/bytecode-interpreter.dasc"
      // check if the value is a heap object
      //|  cmp word [STK+ARG2F*8+6], Value::FLAG_HEAP
      //|  je >1
      //|  cmp dword [STK+ARG2F*8+4], Value::FLAG_FALSECOND
      //|  cmova ARG3, dword [->FlagTrueConst]
      //|1:
      //|  mov dword [STK+ARG1F*8+4], ARG3
      //|  Dispatch
      dasm_put(Dst, 4402, Value::FLAG_HEAP, Value::FLAG_FALSECOND);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 2074 "src/interpreter/bytecode-interpreter.dasc"
      break;

    // ------------------------------------------------
    // Branch                                         |
    // -----------------------------------------------*/

    // branch PC
    //|.macro branch_to,where,TEMP
    //|  mov TEMP,qword SAVED_PC
    //|  lea PC,[TEMP+where*4]
    //|.endmacro

    case BC_JMPT:
      //|=>bc:
      //|  instr_B
      //|  cmp word  [STK+ARG1F*8+6], Value::FLAG_HEAP
      //|  je >2
      //|  cmp dword [STK+ARG1F*8+4], Value::FLAG_FALSECOND
      //|  ja >1
      //|2:
      //|  branch_to ARG2F,ARG3F
      //|1:  // fallthrough
      //|  Dispatch
      dasm_put(Dst, 4444, bc, Value::FLAG_HEAP, Value::FLAG_FALSECOND);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 2097 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_JMPF:
      //|=>bc:
      //|  instr_B
      //|  cmp word  [STK+ARG1F*8+6], Value::FLAG_HEAP
      //|  je >2
      //|  cmp dword [STK+ARG1F*8+4], Value::FLAG_FALSECOND
      //|  jbe >2
      //|  branch_to ARG2F,ARG3F
      //|2: // fallthrough
      //|  Dispatch
      dasm_put(Dst, 4498, bc, Value::FLAG_HEAP, Value::FLAG_FALSECOND);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 2109 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_AND:
      //|=>bc:
      //|  instr_B
      //|  cmp word  [STK+ARG1F*8+6], Value::FLAG_HEAP
      //|  je >1
      //|  cmp dword [STK+ARG1F*8+4], Value::FLAG_FALSECOND
      //|  jbe >1
      //|  branch_to ARG2F,ARG3F
      //|1: // fallthrough
      //|  Dispatch
      dasm_put(Dst, 4550, bc, Value::FLAG_HEAP, Value::FLAG_FALSECOND);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 2121 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_OR:
      //|=>bc:
      //|  instr_B
      //|  cmp word  [STK+ARG1F*8+6], Value::FLAG_HEAP
      //|  je >2
      //|  cmp dword [STK+ARG1F*8+4], Value::FLAG_FALSECOND
      //|  ja >1
      //|2:
      //|  branch_to ARG2F,ARG3F
      //|1: // fallthrough
      //|  Dispatch
      dasm_put(Dst, 4444, bc, Value::FLAG_HEAP, Value::FLAG_FALSECOND);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 2134 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_JMP:
      //|=>bc:
      //|  instr_G
      //|  branch_to ARG1F,ARG3F
      //|  Dispatch
      dasm_put(Dst, 4602, bc);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 2141 "src/interpreter/bytecode-interpreter.dasc"
      break;

    // ----------------------------------------------------------
    // Property/Upvalue/Global
    // ---------------------------------------------------------*/

    // A better way to implement property get/set is via IC (inline cache)
    // but currently we don't have IC states at bytecode level and we will
    // add it guide the JIT compilation phase. For interpreter, we will use
    // something simpler but good to capture most of the cases. Since we force
    // our internal hash use SSO's default hash value to serve as hash value
    // of the key, so we will use a trick that is used in LuaJIT here. For
    // key that is SSO, we will retrieve its hash value out and directly
    // generate index to anchor its main position inside of the *chain* and
    // if we miss we will fallback to the slow path

    // LREG --> Object*
    // RREG --> SSO*
    //|.macro find_sso_pos,output
    //|  mov T2,LREG

    //|  mov T2L, dword [T2+MapLayout::kCapacityOffset]

    //|  mov T1L, dword [RREG+SSOLayout::kHashOffset]

    // T2L capacity , T1L size
    //|  sub T2L,1

    // T1L -> index
    //|  and T1L,T2L

    // index into the array
    //|  lea T2L, [LREG+(MapLayout::kArrayOffset)]
    //|  lea T1L, [T1L+T1L*2]
    //|  lea T2L, [T2L+T1L*8]

    // save map's starting address
    //|  lea T1 , [LREG+MapLayout::kArrayOffset]

    // 1. check if this entry is *deleted* or *used*
    //|  mov LREGL, dword [T2L+MapEntryLayout::kFlagOffset]
    //|  test LREGL,((1<<30))
    //|  je >7  // not found main position is empty

    //|1:
    // 2. check if it is deleted
    //|  test LREGL,((1<<31))
    //|  jne >6 // deleted slots

    // 3. check if the key is a SSO
    //|  mov LREG, qword [T2L]
    //|  CheckSSO LREG,6
    //|  mov LREG, qword [LREG]
    //|  cmp LREG, RREG
    //|  jne >6 // string not identical

    // now we find the entry
    //|  mov T2,qword [T2L+MapEntryLayout::kValueOffset]
    //|  mov qword [STK+output*8], T2
    //|  Dispatch

    // move to next iteration
    //|6:
    //|  mov LREGL, dword [T2L+MapEntryLayout::kFlagOffset]
    //|  test LREGL,((1<<29))
    //|  je >7
    //|  and LREGL, ((bits::BitOn<std::uint32_t,0,29>::value))
    //|  lea T2L  , [LREGL+LREGL*2]
    //|  lea T2L  , [T1+T2L*8]
    //|  mov LREGL, dword [T2L*8+MapEntryLayout::kFlagOffset]
    //|  jmp <1

    //|7:
    //|  jmp ->InterpPropGetNotFound

    //|.endmacro

    case BC_PROPGET:
      //|=>bc:
      //|  instr_B
      dasm_put(Dst, 4626, bc);
#line 2221 "src/interpreter/bytecode-interpreter.dasc"

      //|.if 1
      //|  cmp word [STK+ARG1F*8+6],Value::FLAG_HEAP
      //|  jne >8
      //|  mov LREG, qword [STK+ARG1F*8]
      //|  CheckObj LREG,9
      //|.else
      //|  mov LREG, qword [STK+ARG1F*8]
      //|  CheckObjV LREG,9
      //|.endif
      dasm_put(Dst, 4635, Value::FLAG_HEAP, -HOH_TYPE_OFFSET, OBJECT_BIT_PATTERN);
#line 2231 "src/interpreter/bytecode-interpreter.dasc"

      //|  LdStr RREG,ARG2F
      dasm_put(Dst, 540, PrototypeLayout::kStringTableOffset);
#line 2233 "src/interpreter/bytecode-interpreter.dasc"
      // assume RREG is an heap object since this should be guaranteed
      // by front-end
      //|  DerefPtrFromV RREG
      //|  CheckSSO RREG,8
      dasm_put(Dst, 4668, -HOH_TYPE_OFFSET, SSO_BIT_PATTERN);
#line 2237 "src/interpreter/bytecode-interpreter.dasc"

      // find sso's position
      //|  find_sso_pos ACCIDX
      dasm_put(Dst, 4688, MapLayout::kCapacityOffset, SSOLayout::kHashOffset, (MapLayout::kArrayOffset), MapLayout::kArrayOffset, MapEntryLayout::kFlagOffset, ((1<<30)), ((1<<31)), -HOH_TYPE_OFFSET, SSO_BIT_PATTERN, MapEntryLayout::kValueOffset, 2040*8);
      dasm_put(Dst, 96);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 4782, MapEntryLayout::kFlagOffset, ((1<<29)), ((bits::BitOn<std::uint32_t,0,29>::value)), MapEntryLayout::kFlagOffset);
#line 2240 "src/interpreter/bytecode-interpreter.dasc"

      //|8: // should be done by slow path of InterpPorpGet
      //|  jmp ->InterpPropGet
      dasm_put(Dst, 4842);
#line 2243 "src/interpreter/bytecode-interpreter.dasc"

      //|9: // failed at *object*
      //|  jmp ->InterpPropGetNotObject
      dasm_put(Dst, 4849);
#line 2246 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_PROPSET:
      // propset is directly yielded back to C++ functions
      //|=>bc:
      //|  instr_B
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  mov CARG2, qword [ACC]
      //|  mov CARG3, qword [STK+ARG1F*8]
      //|  LdStr CARG4,ARG2F
      //|  fcall InterpreterDoPropSet
      dasm_put(Dst, 4856, bc, RuntimeLayout::kCurPCOffset, PrototypeLayout::kStringTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoPropSet))) {
      dasm_put(Dst, 4898);
       } else {
         lava_warn("%s","Function InterpreterDoPropSet address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoPropSet)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoPropSet))>>32));
       }
#line 2258 "src/interpreter/bytecode-interpreter.dasc"
      //|  retbool
      dasm_put(Dst, 650);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 2259 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_IDXGET:
      //|=>bc:
      //|  instr_E
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  mov CARG2, qword [STK+ARG1F*8]
      //|  mov CARG3, qword [STK+ARG2F*8]
      //|  fcall InterpreterDoIdxGet
      dasm_put(Dst, 4903, bc, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoIdxGet))) {
      dasm_put(Dst, 4928);
       } else {
         lava_warn("%s","Function InterpreterDoIdxGet address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoIdxGet)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoIdxGet))>>32));
       }
#line 2269 "src/interpreter/bytecode-interpreter.dasc"
      //|  retbool
      dasm_put(Dst, 650);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 2270 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_IDXSET:
      //|=>bc:
      //|  instr_D
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  mov CARG2, qword [STK+ARG1F*8]
      //|  mov CARG3, qword [STK+ARG2F*8]
      //|  mov CARG4, qword [STK+ARG3F*8]
      //|  fcall InterpreterDoIdxSet
      dasm_put(Dst, 4933, bc, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoIdxSet))) {
      dasm_put(Dst, 4972);
       } else {
         lava_warn("%s","Function InterpreterDoIdxSet address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoIdxSet)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoIdxSet))>>32));
       }
#line 2280 "src/interpreter/bytecode-interpreter.dasc"
      //|  retbool
      dasm_put(Dst, 650);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 2281 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_UVGET:
      //|=>bc:
      //|  instr_B
      //|  LdUV LREG,ARG2F
      //|  mov  qword [STK+ARG1F*8], LREG
      //|  Dispatch
      dasm_put(Dst, 4977, bc, RuntimeLayout::kCurClsOffset, ClosureLayout::kUpValueOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 2289 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_UVSET:
      //|=>bc:
      //|  instr_C
      //|  mov RREG, qword [STK+ARG2F*8]
      //|  StUV ARG1F,RREG
      //|  Dispatch
      dasm_put(Dst, 5014, bc, RuntimeLayout::kCurClsOffset, ClosureLayout::kUpValueOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 2297 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_GSET:
      //|=>bc:
      //|  instr_C
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  LdStr CARG2, ARG1F
      //|  mov CARG3, qword [STK+ARG2F*8]
      //|  fcall InterpreterDoGSet
      dasm_put(Dst, 5051, bc, RuntimeLayout::kCurPCOffset, PrototypeLayout::kStringTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoGSet))) {
      dasm_put(Dst, 5085);
       } else {
         lava_warn("%s","Function InterpreterDoGSet address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoGSet)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoGSet))>>32));
       }
#line 2307 "src/interpreter/bytecode-interpreter.dasc"
      //|  retbool
      dasm_put(Dst, 650);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 2308 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_GGET:
      //|=>bc:
      //|  instr_B
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  lea CARG2, [STK+ARG1F*8]
      //|  LdStr CARG3, ARG2F
      //|  fcall InterpreterDoGGet
      dasm_put(Dst, 5090, bc, RuntimeLayout::kCurPCOffset, PrototypeLayout::kStringTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoGGet))) {
      dasm_put(Dst, 5124);
       } else {
         lava_warn("%s","Function InterpreterDoGGet address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoGGet)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoGGet))>>32));
       }
#line 2318 "src/interpreter/bytecode-interpreter.dasc"
      //|  retbool
      dasm_put(Dst, 650);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 2319 "src/interpreter/bytecode-interpreter.dasc"
      break;

    /* ========================================================
     * Loop instructions
     *
     * Loop is optimized for situation that condition/step and
     * induction variable are all *integer* value
     * =======================================================*/
    case BC_FSTART:
      //|=>bc:
      //|  instr_B
      dasm_put(Dst, 4626, bc);
#line 2330 "src/interpreter/bytecode-interpreter.dasc"
      // must be boolean flag here
      //|  cmp dword [STK+ACCFIDX], Value::FLAG_FALSE
      //|  je >1
      //|2:
      //|  Dispatch
      dasm_put(Dst, 5129, Value::FLAG_FALSE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
#line 2335 "src/interpreter/bytecode-interpreter.dasc"
      //|1:
      //|  branch_to ARG2F,ARG3F
      //|  jmp <2
      dasm_put(Dst, 5156);
#line 2338 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_FEND1:
      //|=>bc:
      //|  instr_E // actually TYPE_H instruction
      //|  cmp dword [STK+ARG1F*8+4], Value::FLAG_REAL
      //|  jnb ->InterpreterForEnd1
      dasm_put(Dst, 5190, bc, Value::FLAG_REAL);
#line 2345 "src/interpreter/bytecode-interpreter.dasc"

      //|  cmp dword [STK+ARG2F*8+4], Value::FLAG_REAL
      //|  jnb ->InterpreterForEnd1
      dasm_put(Dst, 5209, Value::FLAG_REAL);
#line 2348 "src/interpreter/bytecode-interpreter.dasc"

      //|  movsd xmm0, qword [STK+ARG1F*8]
      //|  ucomisd xmm0, qword [STK+ARG2F*8]
      //|  jae >8 // loop exit
      dasm_put(Dst, 5221);
#line 2352 "src/interpreter/bytecode-interpreter.dasc"

      //|  mov ARG1, dword [PC]
      //|  branch_to ARG1F,ARG3F
      //|7:
      //|  Dispatch
      dasm_put(Dst, 5239);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
#line 2357 "src/interpreter/bytecode-interpreter.dasc"
      //|8:
      //|  // skip the 4th argument
      //|  add PC,4
      //|  jmp <8
      dasm_put(Dst, 5263);
#line 2361 "src/interpreter/bytecode-interpreter.dasc"

      //|6: // fallback for situation that is not integer
      //|  mov CARG1, RUNTIME
      //|  mov CARG2, qword [STK+ARG1F*8]
      //|  mov CARG3, qword [STK+ARG2F*8]
      //|  mov CARG4L, dword [PC]
      //|  fcall InterpreterForEnd1
      dasm_put(Dst, 5293);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterForEnd1))) {
      dasm_put(Dst, 5310);
       } else {
         lava_warn("%s","Function InterpreterForEnd1 address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterForEnd1)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterForEnd1))>>32));
       }
#line 2368 "src/interpreter/bytecode-interpreter.dasc"
      // handle return value
      //|  test eax,eax
      //|  je ->InterpFail
      //|  mov PC, qword [RUNTIME+RuntimeLayout::kCurPCOffset]
      //|  Dispatch
      dasm_put(Dst, 5315, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 2373 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_FEND2:
      //|=>bc:
      //|  instr_D
      //|  cmp dword [STK+ARG1F*8+4], Value::FLAG_REAL
      //|  jnb >6
      //|  cmp dword [STK+ARG2F*8+4], Value::FLAG_REAL
      //|  jnb >6
      //|  cmp dword [STK+ARG3F*8+4], Value::FLAG_REAL
      //|  jnb >6
      dasm_put(Dst, 5338, bc, Value::FLAG_REAL, Value::FLAG_REAL, Value::FLAG_REAL);
#line 2384 "src/interpreter/bytecode-interpreter.dasc"

      //|  movsd xmm0, qword [STK+ARG1F*8]
      //|  addsd xmm0, qword [STK+ARG3F*8]
      //|  ucomisd xmm0, qword [STK+ARG2F*8]
      //|  movsd qword [STK+ARG1F*8], xmm0 // need to write back
      //|  jae >8 // loop exit
      dasm_put(Dst, 5389);
#line 2390 "src/interpreter/bytecode-interpreter.dasc"

      // fallthrough
      //|  mov ARG1, dword [PC]
      //|  branch_to ARG1F,ARG3F
      //|7:
      //|  Dispatch
      dasm_put(Dst, 5239);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
#line 2396 "src/interpreter/bytecode-interpreter.dasc"
      //|8:
      //|  add PC,4
      //|  jmp <7
      dasm_put(Dst, 5421);
#line 2399 "src/interpreter/bytecode-interpreter.dasc"

      //|6:
      //|  mov CARG1, RUNTIME
      //|  lea CARG2, [STK+ARG1F*8]
      //|  mov CARG3, qword [STK+ARG2F*8]
      //|  mov CARG4, qword [STK+ARG3F*8]
      //|  mov CARG5L, dword [PC]
      //|  fcall InterpreterForEnd2
      dasm_put(Dst, 5451);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterForEnd2))) {
      dasm_put(Dst, 5473);
       } else {
         lava_warn("%s","Function InterpreterForEnd2 address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterForEnd2)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterForEnd2))>>32));
       }
#line 2407 "src/interpreter/bytecode-interpreter.dasc"
      //|  test eax,eax
      //|  je ->InterpFail
      //|  mov PC, qword [RUNTIME+RuntimeLayout::kCurPCOffset]
      //|  Dispatch
      dasm_put(Dst, 5315, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 2411 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_FEVRSTART:
      //|=>bc:
      //|  instr_X
      //|  Dispatch
      dasm_put(Dst, 5478, bc);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 2417 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_FEVREND:
      //|=>bc:
      //|  instr_G
      //|  branch_to ARG1F,ARG3F
      //|  Dispatch
      dasm_put(Dst, 4602, bc);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 107);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 112, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 120);
#line 2424 "src/interpreter/bytecode-interpreter.dasc"
      break;

    default:
      //|=> bc:
      //|  Break
      dasm_put(Dst, 5490,  bc);
#line 2429 "src/interpreter/bytecode-interpreter.dasc"
      break;
  }
}

// Help Dasm to resolve external address via Index idx
int ResolveExternAddress( void** ctx , unsigned char* addr ,
                                       int idx,
                                       int type ) {
  (void)ctx;

  ExternSymbolTable* t = GetExternSymbolTable();
  ExternSymbolTable::iterator itr = t->find(extnames[idx]);

  lava_verify( itr != t->end() );

  void* ptr = itr->second;
  lava_verify(CheckAddress(reinterpret_cast<std::uintptr_t>(ptr)));

  int iptr = HorribleCast(ptr);
  lava_verify(reinterpret_cast<void*>(iptr) == ptr);

  if(type) {
    int end = HorribleCast(addr+4);

    // Check whether the address is overflowed or not. I think this is
    // not needed but just in cases we have a bug so we don't end up
    // calling into some wired places into our code
    std::int64_t ptr64 = static_cast<std::int64_t>(iptr);
    std::int64_t end64 = static_cast<std::int64_t>(end);

    lava_verify( (ptr64-end64) >= std::numeric_limits<int>::min() &&
                 (ptr64-end64) <= std::numeric_limits<int>::max() );

    return iptr - HorribleCast(addr+4);
  } else {
    return iptr;
  }
}

} // namespace

AssemblyInterpreter::AssemblyInterpreter():
  dispatch_interp_(),
  dispatch_record_(),
  dispatch_jit_   (),
  interp_helper_  (),
  interp_entry_   (),
  code_buffer_    (),
  code_size_      (),
  buffer_size_    ()
{}

AssemblyInterpreter::~AssemblyInterpreter() {
  if(interp_entry_) OS::FreeCodePage(code_buffer_,buffer_size_);
}

std::shared_ptr<AssemblyInterpreter> AssemblyInterpreter::Generate() {
  static std::shared_ptr<AssemblyInterpreter> interp;
  if(interp) return interp; // return interp if we already have a interpreter pointer

  // create a new interp object since this is our first time
  interp.reset( new AssemblyInterpreter() );

  // create a build context
  BuildContext bctx;

  // initialize dasm_State object
  dasm_init(&(bctx.dasm_ctx),2);

  // setup the freaking global
  void* glb_arr[GLBNAME__MAX];
  dasm_setupglobal(&(bctx.dasm_ctx),glb_arr,GLBNAME__MAX);

  // setup the dasm
  dasm_setup(&(bctx.dasm_ctx),actions);

  // initialize the tag value needed , at least for each BC we need one
  bctx.tag = DASM_GROWABLE_PC_SIZE;
  dasm_growpc(&(bctx.dasm_ctx), DASM_GROWABLE_PC_SIZE );

  // ----------------------------------------------------------
  // Order matters, it may change profile of our icache
  // ----------------------------------------------------------

  // build the helper
  GenerateHelper(&bctx);

  // build the prolog
  GenerateInterpMisc(&bctx);

  // generate all bytecode's routine
  for( int i = 0 ; i < SIZE_OF_BYTECODE ; ++i ) {
    GenerateOneBytecode(&bctx,static_cast<Bytecode>(i));
  }

  std::size_t code_size;

  // we should never fail at *linking* if our code is *correct*
  lava_verify(dasm_link(&(bctx.dasm_ctx),&code_size) ==0);

  // generate a buffer and set the proper protection field for that piece of
  // memory to make our code *work*
  std::size_t new_size;

  void* buffer = OS::CreateCodePage(code_size,&new_size);
  if(!buffer) {
    return std::shared_ptr<AssemblyInterpreter>();
  }

  // encode the assembly code into the buffer
  dasm_encode(&(bctx.dasm_ctx),buffer);

  // get all pc labels for entry of bytecode routine
  for( int i = 0 ; i < SIZE_OF_BYTECODE ; ++i ) {
    int off = dasm_getpclabel(&(bctx.dasm_ctx),i);
    interp->dispatch_interp_[i] =
      reinterpret_cast<void*>(static_cast<char*>(buffer) + off);
  }

  // get all pc labels for helper routines
  for( int i = INTERP_HELPER_START ; i < DASM_GROWABLE_PC_SIZE ; ++i ) {
    int off = dasm_getpclabel(&(bctx.dasm_ctx),i);
    interp->interp_helper_.push_back(
        reinterpret_cast<void*>(static_cast<char*>(buffer)+off));
  }

  // start of the code buffer
  interp->code_buffer_  = buffer;

  // get the *interpreter's* entry
  int off = dasm_getpclabel(&(bctx.dasm_ctx),INTERP_START);
  interp->interp_entry_ = reinterpret_cast<void*>(
      static_cast<char*>(buffer) + off);

  interp->buffer_size_  = new_size;
  interp->code_size_    = code_size;
  return interp;
}

Bytecode AssemblyInterpreter::CheckBytecodeRoutine( void* pc ) const {
  for( int i = 0 ; i < SIZE_OF_BYTECODE ; ++i ) {
    void* p = reinterpret_cast<void*>(pc);
    if(p == dispatch_interp_[i]) {
      return static_cast<Bytecode>(i);
    }
  }
  return SIZE_OF_BYTECODE;
}

int AssemblyInterpreter::CheckHelperRoutine( void* pc ) const {
  std::vector<void*>::const_iterator itr =
    std::find( interp_helper_.begin() , interp_helper_.end() , pc );
  if(itr != interp_helper_.end()) {
    return (static_cast<int>(std::distance(interp_helper_.begin(),itr))+INTERP_HELPER_START);
  } else {
    return -1;
  }
}

void AssemblyInterpreter::Dump( DumpWriter* writer ) const {
  ZydisDecoder decoder;
  ZydisDecoderInit( &decoder, ZYDIS_MACHINE_MODE_LONG_64,
                              ZYDIS_ADDRESS_WIDTH_64);

  ZydisFormatter formatter;
  ZydisFormatterInit(&formatter,ZYDIS_FORMATTER_STYLE_INTEL);

  std::uint64_t pc = reinterpret_cast<std::uint64_t>(code_buffer_);
  std::uint8_t* rp = static_cast<std::uint8_t*>(code_buffer_);
  std::size_t size = code_size_;

  writer->WriteL("CodeSize:%zu",code_size_);
  ZydisDecodedInstruction instr;
  while(ZYDIS_SUCCESS(
        ZydisDecoderDecodeBuffer(&decoder,rp,size,pc,&instr))) {

    char buffer[256];
    ZydisFormatterFormatInstruction(
        &formatter,&instr,buffer,sizeof(buffer));
    // check labels
    {
      Bytecode bc = CheckBytecodeRoutine(reinterpret_cast<void*>(pc));
      if(bc != SIZE_OF_BYTECODE) {
        writer->WriteL("Bytecode ===========> %s:",GetBytecodeName(bc));
      } else {
        int idx = CheckHelperRoutine(reinterpret_cast<void*>(pc));
        if(idx >= 0) {
          writer->WriteL("Helper ===========> %s:",GetInterpHelperName(idx));
        }
      }
    }
    writer->WriteL("%016" PRIX64 " (%d) %s",pc,instr.length,buffer);
    rp += instr.length;
    size -= instr.length;
    pc += instr.length;
  }
}

AssemblyInterpreter::Instance::Instance( const std::shared_ptr<AssemblyInterpreter>& interp ):
  dispatch_interp_(),
  dispatch_record_(),
  dispatch_jit_   (),
  interp_         (interp) {

  memcpy(dispatch_interp_,interp->dispatch_interp_,sizeof(dispatch_interp_));
  memcpy(dispatch_record_,interp->dispatch_record_,sizeof(dispatch_record_));
  memcpy(dispatch_jit_   ,interp->dispatch_jit_   ,sizeof(dispatch_jit_   ));
}

bool AssemblyInterpreter::Instance::Run( Context* context , const Handle<Script>& script ,
                                                            const Handle<Object>& globals,
                                                            std::string* error,
                                                            Value* rval ) {

  Runtime* rt = context->gc()->GetInterpreterRuntime(script.ref(), globals.ref(), error);
  // Entry of our assembly interpreter
  Main m = reinterpret_cast<Main>(interp_->interp_entry_);

  // Interpret the bytecode
  bool ret = m(rt,rt->cur_proto,reinterpret_cast<void*>(rt->stack),
                                const_cast<void*>(
                                  reinterpret_cast<const void*>((*(rt->cur_proto))->code_buffer())),
                                dispatch_interp_);
  // Check return
  if(ret) {
    *rval = rt->ret;
  }

  context->gc()->ReturnInterpreterRuntime(rt);
  return ret;
}

} // namespace interpreter
} // namespace lavascript
