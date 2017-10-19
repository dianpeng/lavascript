/*
** This file has been pre-processed with DynASM.
** http://luajit.org/dynasm.html
** DynASM version 1.3.0, DynASM x64 version 1.3.0
** DO NOT EDIT! The original file is in "src/interpreter/bytecode-interpreter.dasc".
*/

#line 1 "src/interpreter/bytecode-interpreter.dasc"
#include "bytecode-interpreter.h"
#include "src/trace.h"
#include "src/os.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <cassert>
#include <climits>

#include <Zydis/Zydis.h>

namespace lavascript {
namespace interpreter{
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

// Make GCC bitch less
template< typename T >
int HorribleCast( T* ptr ) {
  std::uint64_t iptr = reinterpret_cast<std::uint64_t>(ptr);
  int ret = static_cast<int>(iptr);
  lava_verify(reinterpret_cast<T*>(ret) == ptr);
  return ret;
}

// --------------------------------------------------------------
// Runtime
//
// Used in the interpretation phase for the interpreter
// --------------------------------------------------------------
struct Runtime {
  // TODO:: This is for test purpose , no going into production
  Prototype** cur;
  Script** script;
  Object** global;
  Value ret;
  std::string* error;
  std::uint64_t stack[1024];

  Runtime( const Handle<Script>& script , const Handle<Object>& glb , std::string* err ):
    cur     (script->main().ref()),
    script  (script.ref()),
    global  (glb.ref()),
    ret     (),
    error   (err),
    stack   ()
  {}
};

static_assert( std::is_standard_layout<Runtime>::value );

struct RuntimeLayout {
  static const std::uint32_t kRetOffset = offsetof(Runtime,ret);
};

// prototype for the interpreter's main
//                    runtime  proto     stk    pc    dispatch
typedef bool (*Main)(Runtime*,Prototype**,void*,void*,void*);

// ------------------------------------------------------------------
// Helper function/macros to register its literal name into a global
// table to help resolve the function's address during assembly link
// phase
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

Value InterpreterDoArithmetic( Runtime* sandbox ,
                               Value left ,  // rsi
                               Value right , // rdx
                               Bytecode bc ) {
  (void)sandbox;
  (void)left;
  (void)right;
  (void)bc;
  return Value();
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterDoArithmetic)

Value InterpreterPow         ( Runtime* sandbox ,
                               Value left,
                               Value right,
                               Bytecode bc ) {
  (void)sandbox;
  (void)left;
  (void)right;
  (void)bc;
  return Value();
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterPow)

void InterpreterModByReal    ( Runtime* sandbox , std::uint32_t* pc ) {
  (void)sandbox;
  (void)pc;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterModByReal)

void InterpreterDivByZero    ( Runtime* sandbox , std::uint32_t* pc ) {
  (void)sandbox;
  (void)pc;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterDivByZero)

double Pow( double a , double b ) {
  return std::pow(a,b);
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(Pow)

//|.arch x64
#if DASM_VERSION != 10300
#error "Version mismatch between DynASM and included encoding engine"
#endif
#line 152 "src/interpreter/bytecode-interpreter.dasc"
//|.actionlist actions
static const unsigned char actions[3939] = {
  249,248,10,73,137,252,252,73,137,252,245,73,137,214,72,137,205,77,137,199,
  255,139,69,0,76,15,182,216,76,137,223,255,232,251,1,0,255,72,184,237,237,
  252,255,208,255,139,69,0,76,15,182,216,72,131,197,4,67,252,255,36,223,255,
  249,248,11,72,49,192,195,255,249,248,12,73,139,134,252,252,3,0,0,73,137,132,
  253,36,233,72,199,192,1,0,0,0,195,255,249,248,13,76,137,231,77,139,93,0,77,
  139,155,233,65,139,52,155,255,232,251,1,1,255,73,137,195,73,193,252,235,32,
  73,129,252,251,239,15,132,244,11,73,137,134,252,252,3,0,0,139,69,0,76,15,
  182,216,76,137,223,255,249,248,14,76,137,231,77,139,93,0,77,139,155,233,65,
  139,20,131,255,249,248,15,76,137,231,77,139,93,0,77,139,155,233,252,242,65,
  15,16,4,219,102,72,15,126,198,255,249,248,16,76,137,231,77,139,93,0,77,139,
  155,233,252,242,65,15,16,4,195,102,72,15,126,194,255,249,248,17,76,137,231,
  73,139,52,222,73,139,20,198,255,249,248,18,255,232,251,1,2,255,252,242,65,
  15,17,134,252,252,3,0,0,139,69,0,76,15,182,216,76,137,223,255,249,248,19,
  76,137,231,255,232,251,1,3,255,249,248,20,76,137,231,72,139,117,252,252,255,
  232,251,1,4,255,252,233,244,11,255,249,248,21,76,137,231,72,139,117,252,252,
  255,232,251,1,5,255,249,65,199,134,0,4,0,0,237,252,233,244,12,255,249,15,
  182,220,193,232,16,73,139,20,198,73,137,20,222,139,69,0,76,15,182,216,76,
  137,223,255,249,15,182,220,193,232,16,77,139,93,0,77,139,155,233,65,139,52,
  131,65,137,52,222,65,199,68,222,4,237,139,69,0,76,15,182,216,76,137,223,255,
  249,15,182,220,193,232,16,65,199,4,222,0,0,0,0,65,199,68,222,4,237,139,69,
  0,76,15,182,216,76,137,223,255,249,15,182,220,193,232,16,65,199,4,222,1,0,
  0,0,65,199,68,222,4,237,139,69,0,76,15,182,216,76,137,223,255,249,15,182,
  220,193,232,16,65,199,4,222,252,255,252,255,252,255,252,255,65,199,68,222,
  4,237,139,69,0,76,15,182,216,76,137,223,255,249,15,182,220,193,232,16,77,
  139,93,0,77,139,155,233,252,242,65,15,16,4,195,252,242,65,15,17,4,222,139,
  69,0,76,15,182,216,76,137,223,255,249,15,182,220,65,199,68,222,4,237,139,
  69,0,76,15,182,216,76,137,223,255,249,193,232,8,15,183,216,193,232,16,73,
  139,20,198,65,129,124,253,198,4,239,15,130,244,247,65,129,124,253,198,4,239,
  15,132,244,248,185,237,252,233,244,13,248,1,77,139,93,0,77,139,155,233,252,
  242,65,15,42,4,155,102,72,15,110,202,252,242,15,88,193,252,242,65,15,17,134,
  252,252,3,0,0,139,69,0,76,15,182,216,76,137,223,255,139,69,0,76,15,182,216,
  72,131,197,4,67,252,255,36,223,248,2,77,139,93,0,77,139,155,233,65,139,52,
  155,255,137,252,240,133,210,15,132,244,20,252,247,252,250,65,137,134,252,
  252,3,0,0,65,199,134,0,4,0,0,237,255,137,252,240,133,210,15,132,244,20,252,
  247,252,250,65,137,150,252,252,3,0,0,65,199,134,0,4,0,0,237,255,1,214,65,
  137,182,252,252,3,0,0,65,199,134,0,4,0,0,237,255,249,193,232,8,15,183,216,
  193,232,16,73,139,20,198,65,129,124,253,198,4,239,15,130,244,247,65,129,124,
  253,198,4,239,15,132,244,248,185,237,252,233,244,15,248,1,77,139,93,0,77,
  139,155,233,252,242,65,15,16,4,219,102,72,15,110,202,252,242,15,88,193,252,
  242,65,15,17,134,252,252,3,0,0,139,69,0,76,15,182,216,76,137,223,255,139,
  69,0,76,15,182,216,72,131,197,4,67,252,255,36,223,248,2,77,139,93,0,77,139,
  155,233,252,242,65,15,16,4,219,252,242,15,42,202,252,242,15,88,193,252,242,
  65,15,17,134,252,252,3,0,0,139,69,0,76,15,182,216,76,137,223,255,249,193,
  232,8,15,183,216,193,232,16,73,139,20,198,65,129,124,253,198,4,239,15,130,
  244,247,65,129,124,253,198,4,239,15,132,244,248,185,237,252,233,244,13,248,
  1,77,139,93,0,77,139,155,233,252,242,65,15,42,4,155,102,72,15,110,202,252,
  242,15,92,193,252,242,65,15,17,134,252,252,3,0,0,139,69,0,76,15,182,216,76,
  137,223,255,41,214,65,137,182,252,252,3,0,0,65,199,134,0,4,0,0,237,255,249,
  193,232,8,15,183,216,193,232,16,73,139,20,198,65,129,124,253,198,4,239,15,
  130,244,247,65,129,124,253,198,4,239,15,132,244,248,185,237,252,233,244,15,
  248,1,77,139,93,0,77,139,155,233,252,242,65,15,16,4,219,102,72,15,110,202,
  252,242,15,92,193,252,242,65,15,17,134,252,252,3,0,0,139,69,0,76,15,182,216,
  76,137,223,255,139,69,0,76,15,182,216,72,131,197,4,67,252,255,36,223,248,
  2,77,139,93,0,77,139,155,233,252,242,65,15,16,4,219,252,242,15,42,202,252,
  242,15,92,193,252,242,65,15,17,134,252,252,3,0,0,139,69,0,76,15,182,216,76,
  137,223,255,249,193,232,8,15,183,216,193,232,16,73,139,20,198,65,129,124,
  253,198,4,239,15,130,244,247,65,129,124,253,198,4,239,15,132,244,248,185,
  237,252,233,244,13,248,1,77,139,93,0,77,139,155,233,252,242,65,15,42,4,155,
  102,72,15,110,202,252,242,15,89,193,252,242,65,15,17,134,252,252,3,0,0,139,
  69,0,76,15,182,216,76,137,223,255,15,175,252,242,65,137,182,252,252,3,0,0,
  65,199,134,0,4,0,0,237,255,249,193,232,8,15,183,216,193,232,16,73,139,20,
  198,65,129,124,253,198,4,239,15,130,244,247,65,129,124,253,198,4,239,15,132,
  244,248,185,237,252,233,244,15,248,1,77,139,93,0,77,139,155,233,252,242,65,
  15,16,4,219,102,72,15,110,202,252,242,15,89,193,252,242,65,15,17,134,252,
  252,3,0,0,139,69,0,76,15,182,216,76,137,223,255,139,69,0,76,15,182,216,72,
  131,197,4,67,252,255,36,223,248,2,77,139,93,0,77,139,155,233,252,242,65,15,
  16,4,219,252,242,15,42,202,252,242,15,89,193,252,242,65,15,17,134,252,252,
  3,0,0,139,69,0,76,15,182,216,76,137,223,255,249,193,232,8,15,183,216,193,
  232,16,73,139,20,198,65,129,124,253,198,4,239,15,130,244,247,65,129,124,253,
  198,4,239,15,132,244,248,185,237,252,233,244,15,248,1,77,139,93,0,77,139,
  155,233,252,242,65,15,16,4,219,102,72,15,110,202,252,242,15,94,193,252,242,
  65,15,17,134,252,252,3,0,0,139,69,0,76,15,182,216,76,137,223,255,139,69,0,
  76,15,182,216,72,131,197,4,67,252,255,36,223,248,2,77,139,93,0,77,139,155,
  233,252,242,65,15,16,4,219,252,242,15,42,202,252,242,15,94,193,252,242,65,
  15,17,134,252,252,3,0,0,139,69,0,76,15,182,216,76,137,223,255,249,193,232,
  8,15,183,216,193,232,16,73,139,20,198,65,129,124,253,198,4,239,15,130,244,
  247,65,129,124,253,198,4,239,15,132,244,248,185,237,252,233,244,13,248,1,
  252,233,244,21,248,2,77,139,93,0,77,139,155,233,65,139,52,155,255,249,15,
  182,220,193,232,16,73,139,52,222,65,129,124,253,222,4,239,15,130,244,247,
  65,129,124,253,222,4,239,15,132,244,248,185,237,252,233,244,14,248,1,77,139,
  93,0,77,139,155,233,252,242,65,15,42,12,131,102,72,15,110,198,252,242,15,
  88,193,252,242,65,15,17,134,252,252,3,0,0,139,69,0,76,15,182,216,76,137,223,
  255,139,69,0,76,15,182,216,72,131,197,4,67,252,255,36,223,248,2,77,139,93,
  0,77,139,155,233,65,139,20,131,255,137,252,242,133,210,252,233,244,20,252,
  247,252,250,65,137,134,252,252,3,0,0,65,199,134,0,4,0,0,237,255,137,252,242,
  133,210,252,233,244,20,252,247,252,250,65,137,150,252,252,3,0,0,65,199,134,
  0,4,0,0,237,255,249,15,182,220,193,232,16,73,139,52,222,65,129,124,253,222,
  4,239,15,130,244,247,65,129,124,253,222,4,239,15,132,244,248,185,237,252,
  233,244,16,248,1,77,139,93,0,77,139,155,233,252,242,65,15,16,12,195,102,72,
  15,110,198,252,242,15,88,193,252,242,65,15,17,134,252,252,3,0,0,139,69,0,
  76,15,182,216,76,137,223,255,139,69,0,76,15,182,216,72,131,197,4,67,252,255,
  36,223,248,2,77,139,93,0,77,139,155,233,252,242,65,15,16,12,195,252,242,15,
  42,198,252,242,15,88,193,252,242,65,15,17,134,252,252,3,0,0,139,69,0,76,15,
  182,216,76,137,223,255,249,15,182,220,193,232,16,73,139,52,222,65,129,124,
  253,222,4,239,15,130,244,247,65,129,124,253,222,4,239,15,132,244,248,185,
  237,252,233,244,14,248,1,77,139,93,0,77,139,155,233,252,242,65,15,42,12,131,
  102,72,15,110,198,252,242,15,92,193,252,242,65,15,17,134,252,252,3,0,0,139,
  69,0,76,15,182,216,76,137,223,255,249,15,182,220,193,232,16,73,139,52,222,
  65,129,124,253,222,4,239,15,130,244,247,65,129,124,253,222,4,239,15,132,244,
  248,185,237,252,233,244,16,248,1,77,139,93,0,77,139,155,233,252,242,65,15,
  16,12,195,102,72,15,110,198,252,242,15,92,193,252,242,65,15,17,134,252,252,
  3,0,0,139,69,0,76,15,182,216,76,137,223,255,139,69,0,76,15,182,216,72,131,
  197,4,67,252,255,36,223,248,2,77,139,93,0,77,139,155,233,252,242,65,15,16,
  12,195,252,242,15,42,198,252,242,15,92,193,252,242,65,15,17,134,252,252,3,
  0,0,139,69,0,76,15,182,216,76,137,223,255,249,15,182,220,193,232,16,73,139,
  52,222,65,129,124,253,222,4,239,15,130,244,247,65,129,124,253,222,4,239,15,
  132,244,248,185,237,252,233,244,14,248,1,77,139,93,0,77,139,155,233,252,242,
  65,15,42,12,131,102,72,15,110,198,252,242,15,89,193,252,242,65,15,17,134,
  252,252,3,0,0,139,69,0,76,15,182,216,76,137,223,255,249,15,182,220,193,232,
  16,73,139,52,222,65,129,124,253,222,4,239,15,130,244,247,65,129,124,253,222,
  4,239,15,132,244,248,185,237,252,233,244,16,248,1,77,139,93,0,77,139,155,
  233,252,242,65,15,16,12,195,102,72,15,110,198,252,242,15,89,193,252,242,65,
  15,17,134,252,252,3,0,0,139,69,0,76,15,182,216,76,137,223,255,139,69,0,76,
  15,182,216,72,131,197,4,67,252,255,36,223,248,2,77,139,93,0,77,139,155,233,
  252,242,65,15,16,12,195,102,72,15,110,198,252,242,15,89,193,252,242,65,15,
  17,134,252,252,3,0,0,139,69,0,76,15,182,216,76,137,223,255,249,255,15,182,
  220,193,232,16,73,139,52,222,65,129,124,253,222,4,239,15,130,244,247,65,129,
  124,253,222,4,239,15,132,244,248,185,237,252,233,244,14,248,1,77,139,93,0,
  77,139,155,233,252,242,65,15,42,12,131,102,72,15,110,198,252,242,15,94,193,
  252,242,65,15,17,134,252,252,3,0,0,139,69,0,76,15,182,216,76,137,223,255,
  249,15,182,220,193,232,16,73,139,52,222,65,129,124,253,222,4,239,15,130,244,
  247,65,129,124,253,222,4,239,15,132,244,248,185,237,252,233,244,16,248,1,
  77,139,93,0,77,139,155,233,252,242,65,15,16,12,195,102,72,15,110,198,252,
  242,15,94,193,252,242,65,15,17,134,252,252,3,0,0,139,69,0,76,15,182,216,76,
  137,223,255,139,69,0,76,15,182,216,72,131,197,4,67,252,255,36,223,248,2,77,
  139,93,0,77,139,155,233,252,242,65,15,16,12,195,252,242,15,42,198,252,242,
  15,94,193,252,242,65,15,17,134,252,252,3,0,0,139,69,0,76,15,182,216,76,137,
  223,255,249,15,182,220,193,232,16,73,139,52,222,65,129,124,253,222,4,239,
  15,130,244,247,65,129,124,253,222,4,239,15,132,244,248,185,237,252,233,244,
  14,248,1,252,233,244,21,248,2,77,139,93,0,77,139,155,233,65,139,20,131,255,
  249,15,182,220,193,232,16,15,182,204,193,232,8,65,139,116,222,4,65,139,84,
  198,4,129,252,254,239,15,132,244,247,129,252,250,239,15,130,244,248,252,233,
  244,252,248,1,129,252,250,239,15,132,244,250,129,252,250,239,15,131,244,252,
  252,242,65,15,42,4,222,252,242,65,15,88,4,198,252,242,65,15,17,134,252,252,
  3,0,0,139,69,0,76,15,182,216,76,137,223,255,139,69,0,76,15,182,216,72,131,
  197,4,67,252,255,36,223,248,2,129,252,250,239,15,130,244,251,129,252,250,
  239,15,133,244,252,252,242,65,15,42,12,198,252,242,65,15,16,4,222,252,242,
  65,15,17,134,252,252,3,0,0,139,69,0,76,15,182,216,76,137,223,255,139,69,0,
  76,15,182,216,72,131,197,4,67,252,255,36,223,248,4,255,65,139,20,222,65,252,
  247,60,198,65,137,134,252,252,3,0,0,65,199,134,0,4,0,0,237,255,65,139,20,
  222,65,252,247,60,198,65,137,150,252,252,3,0,0,65,199,134,0,4,0,0,237,255,
  65,139,52,222,65,3,52,198,65,137,182,252,252,3,0,0,65,199,134,0,4,0,0,237,
  255,139,69,0,76,15,182,216,72,131,197,4,67,252,255,36,223,248,5,255,252,233,
  244,21,255,252,242,65,15,16,4,222,252,242,65,15,88,4,198,252,242,65,15,17,
  134,252,252,3,0,0,255,139,69,0,76,15,182,216,72,131,197,4,67,252,255,36,223,
  248,6,185,237,252,233,244,17,255,249,15,182,220,193,232,16,15,182,204,193,
  232,8,65,139,116,222,4,65,139,84,198,4,129,252,254,239,15,132,244,247,129,
  252,250,239,15,130,244,248,252,233,244,252,248,1,129,252,250,239,15,132,244,
  250,129,252,250,239,15,131,244,252,252,242,65,15,42,4,222,252,242,65,15,92,
  4,198,252,242,65,15,17,134,252,252,3,0,0,139,69,0,76,15,182,216,76,137,223,
  255,65,139,52,222,65,43,52,198,65,137,182,252,252,3,0,0,65,199,134,0,4,0,
  0,237,255,252,242,65,15,16,4,222,252,242,65,15,92,4,198,252,242,65,15,17,
  134,252,252,3,0,0,255,249,15,182,220,193,232,16,15,182,204,193,232,8,65,139,
  116,222,4,65,139,84,198,4,129,252,254,239,15,132,244,247,129,252,250,239,
  15,130,244,248,252,233,244,252,248,1,129,252,250,239,15,132,244,250,129,252,
  250,239,15,131,244,252,252,242,65,15,42,4,222,252,242,65,15,89,4,198,252,
  242,65,15,17,134,252,252,3,0,0,139,69,0,76,15,182,216,76,137,223,255,65,139,
  52,222,65,15,175,52,198,65,137,182,252,252,3,0,0,65,199,134,0,4,0,0,237,255,
  252,242,65,15,16,4,222,252,242,65,15,89,4,198,252,242,65,15,17,134,252,252,
  3,0,0,255,249,15,182,220,193,232,16,15,182,204,193,232,8,65,139,116,222,4,
  65,139,84,198,4,129,252,254,239,15,132,244,247,129,252,250,239,15,130,244,
  248,252,233,244,252,248,1,129,252,250,239,15,132,244,250,129,252,250,239,
  15,131,244,252,252,242,65,15,42,4,222,252,242,65,15,94,4,198,252,242,65,15,
  17,134,252,252,3,0,0,139,69,0,76,15,182,216,76,137,223,255,252,242,65,15,
  16,4,222,252,242,65,15,94,4,198,252,242,65,15,17,134,252,252,3,0,0,255,249,
  193,232,8,15,183,216,193,232,16,77,139,93,0,77,139,155,233,252,242,65,15,
  42,4,155,65,139,84,198,4,129,252,250,239,15,130,244,247,129,252,250,239,15,
  133,244,248,252,242,73,15,42,12,198,248,1,252,233,244,18,248,2,77,139,93,
  0,77,139,155,233,72,199,198,237,72,193,230,32,65,11,52,155,73,139,20,198,
  72,199,193,237,252,233,244,19,255,249,15,182,220,193,232,16,77,139,93,0,77,
  139,155,233,252,242,65,15,42,12,131,65,139,116,222,4,129,252,254,239,15,130,
  244,247,129,252,254,239,15,133,244,248,252,242,73,15,42,4,222,248,1,252,233,
  244,18,248,2,73,139,52,222,77,139,93,0,77,139,155,233,72,199,194,237,72,193,
  226,32,65,11,20,131,72,199,193,237,252,233,244,19,255,249,193,232,8,15,183,
  216,193,232,16,77,139,93,0,77,139,155,233,252,242,65,15,16,4,219,65,139,84,
  198,4,129,252,250,239,15,130,244,247,129,252,250,239,15,133,244,248,252,242,
  73,15,42,12,198,248,1,252,233,244,18,248,2,77,139,93,0,77,139,155,233,73,
  139,52,219,73,139,20,198,72,199,193,237,252,233,244,19,255,249,193,232,8,
  15,183,216,193,232,16,77,139,93,0,77,139,155,233,252,242,65,15,16,12,195,
  65,139,116,222,4,129,252,254,239,15,130,244,247,129,252,254,239,15,133,244,
  248,252,242,73,15,42,4,222,248,1,252,233,244,18,248,2,77,139,93,0,77,139,
  155,233,73,139,20,195,73,139,52,222,72,199,193,237,252,233,244,19,255,249,
  193,232,8,15,183,216,193,232,16,73,139,52,222,73,139,20,198,72,199,193,237,
  252,233,244,19,255,249,205,3,255
};

#line 153 "src/interpreter/bytecode-interpreter.dasc"
//|.globals GLBNAME_
enum {
  GLBNAME_InterpStart,
  GLBNAME_InterpFail,
  GLBNAME_InterpReturn,
  GLBNAME_InterpArithIntL,
  GLBNAME_InterpArithIntR,
  GLBNAME_InterpArithRealL,
  GLBNAME_InterpArithRealR,
  GLBNAME_InterpArithVV,
  GLBNAME_InterpPowFast,
  GLBNAME_InterpPowSlow,
  GLBNAME_DivByZero,
  GLBNAME_ModByReal,
  GLBNAME__MAX
};
#line 154 "src/interpreter/bytecode-interpreter.dasc"
//|.globalnames glbnames
static const char *const glbnames[] = {
  "InterpStart",
  "InterpFail",
  "InterpReturn",
  "InterpArithIntL",
  "InterpArithIntR",
  "InterpArithRealL",
  "InterpArithRealR",
  "InterpArithVV",
  "InterpPowFast",
  "InterpPowSlow",
  "DivByZero",
  "ModByReal",
  (const char *)0
};
#line 155 "src/interpreter/bytecode-interpreter.dasc"
//|.externnames extnames
static const char *const extnames[] = {
  "PrintOP",
  "InterpreterDoArithmetic",
  "Pow",
  "InterpreterPow",
  "InterpreterDivByZero",
  "InterpreterModByReal",
  (const char *)0
};
#line 156 "src/interpreter/bytecode-interpreter.dasc"

//|.macro prolog,reserve
//|.if 0
//|   push rbp
//|   mov rbp,rsp
//||  if(reserve) {
//|     sub rsp,reserve
//||  }
//|.endif
//|.endmacro

//|.macro epilog
//|.if 0
//|   mov rsp,rbp
//|   pop rbp
//|.endif
//|.endmacro

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
// this one is used in MoarVM. It uses memory address to workaraoud the
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
//|.define ACCIDX,                1020
//|.define ACCFIDX,               1024
//|.define ACC,                   STK+ACCIDX

// Dispatch table pointer
//|.define DISPATCH,              r15  // callee saved

// Bytecode array
//|.define PC,                    rbp  // callee saved

// Hold the decoded unit
//|.define INSTR,                 eax
//|.define INSTR_OP,              al
//|.define INSTR_A8,              ah
//|.define INSTR_A16,             ax

/** ------------------------------------------------------------
 *
 * NOTES: A notes on movzx/movsx instruction.
 *
 *        Based on documentation, when move 32bits register
 *        to 64 bits register, it *automatically* zero out
 *        the upper 32 bits , but if you directly move 16/8 bits
 *        register to 32/64 it won't zero out the rest bits. So
 *        you need to use movzx/movsx instruction.
 *
 *        But code like this :
 *           movzx eax,al
 *           mov rbx,[rcx+rax*8+disp]
 *
 *        will work. We assume that movzx's dest is 32 bits register,
 *        so it will only zero out its upper 24 bits. However
 *        this movzx actually gonna zero out *every* bits for
 *        the whole 64 bits register.
 *
 *        This is the actual behavior and *most* compiler will
 *        assume such behavior even documentation doesn't say so.
 *        I will rely on this behavior since it will save us
 *        one instruction during encoding.
 *
 *        The operand for each instruction will always be a
 *        32 bits register and whenever decode happened , we
 *        will use movzx to clear the *whole* register
 *        out.
 * --------------------------------------------------------------*/

// Instruction's argument
//|.define ARG1_8,                bl
//|.define ARG1_16,               bx
//|.define ARG1,                  ebx  // must *not* be 64 bits due to we use ah

//|.define ARG2_8,                al
//|.define ARG2_16,               ax
//|.define ARG2,                  eax

//|.define ARG3_8,                cl
//|.define ARG3_16,               cx
//|.define ARG3,                  ecx

// temporary register are r10 and r11
//|.define LREG,                  rsi
//|.define LREGL,                 esi
//|.define RREG,                  rdx
//|.define RREGL,                 edx

//|.define T1,                    r11
//|.define T1L,                   r11d

// registers for normal C function calling ABI
//|.define CARG1,                 rdi
//|.define CARG2,                 rsi
//|.define CARG3,                 rdx
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

/* ---------------------------------------------------------------
 * dispatch table                                                |
 * --------------------------------------------------------------*/
//|.macro Dispatch
//|  mov INSTR,dword [PC]
//|  movzx T1,INSTR_OP
//|.if 1
//|  mov CARG1,T1
//|  fcall PrintOP
//|.endif
//|  mov INSTR,dword [PC]
//|  movzx T1,INSTR_OP
//|  add PC,4
//|  jmp aword [DISPATCH+T1*8]
//|.endmacro

/* ---------------------------------------------------------------
 * decode each instruction's argument/operand                    |
 * --------------------------------------------------------------*/
//|.macro instr_B
//|  movzx ARG1,INSTR_A8
//|  shr INSTR,16
//|.endmacro

//|.macro instr_C
//|  shr INSTR,8
//|  movzx ARG1,INSTR_A16
//|  shr INSTR,16
//|.endmacro

//|.macro instr_D
//|  movzx ARG1,INSTR_A8
//|  shr INSTR,16
//|  movzx ARG3,INSTR_A8
//|  shr INSTR,8
//|.endmacro

//|.macro instr_E
//|  movzx ARG1,INSTR_A8
//|  shr INSTR,16
//|.endmacro

//|.macro instr_F
//|  movzx ARG1,INSTR_A8
//|.endmacro

//|.macro instr_G
//|  shr INSTR,8
//|  movzx ARG1,INSTR_A16
//|.endmacro

//|.macro instr_X
//|.endmacro

//|.macro instr_N
//|  instr_D
//|.endmacro


/* -----------------------------------------------------------
 * constant loading                                          |
 * ----------------------------------------------------------*/

// Currently our constant loading is *slow* due to the design of our GC
// and also the layout of each constant array. I think we have a way to
// optimize away one memory move. LuaJIT's constant loading is just one
// single instruction since they only get one constant array and they don't
// need to worry about GC move the reference
//|.macro LdInt,reg,index
//|  mov T1,qword [PROTO]
//|  mov T1,qword [T1+PrototypeLayout::kIntTableOffset]
//|  mov reg, [T1+index*4]
//|.endmacro

// TODO:: Optimize this piece of shit
//|.macro LdIntV,reg,regL,index
//|  mov T1,qword [PROTO]
//|  mov T1,qword [T1+PrototypeLayout::kIntTableOffset]

//|.if 1
//|  mov reg, Value::FLAG_INTEGER
//|  shl reg,32
//|  or regL,dword [T1+index*4]
//|.else
//|  mov64 reg, static_cast<std::uint64_t>(Value::TAG_INTEGER)
//|  or regL,dword [T1+index*4]
//|.endif

//|.endmacro

//|.macro LdReal,reg,index
//|  mov T1,qword [PROTO]
//|  mov T1,qword [T1+PrototypeLayout::kRealTableOffset]
//|  movsd reg,qword[T1+index*8]
//|.endmacro

//|.macro LdRealV,reg,index
//|  mov T1,qword [PROTO]
//|  mov T1,qword [T1+PrototypeLayout::kRealTableOffset]
// not a xmm register
//|  mov reg,qword[T1+index*8]
//|.endmacro

//|.macro LdInt2Real,reg,index
//|  mov T1,qword [PROTO]
//|  mov T1,qword [T1+PrototypeLayout::kIntTableOffset]
//|  cvtsi2sd reg, dword [T1+index*4]
//|.endmacro

//|.macro StIntACC,reg
//|  mov dword [STK+ACCIDX],reg
//|  mov dword [STK+ACCFIDX],Value::FLAG_INTEGER
//|.endmacro

//|.macro StInt,index,reg
//|  mov dword [STK+index*8],reg
//|  mov dword [STK+index*8+4],Value::FLAG_INTEGER
//|.endmacro

//|.macro StRealACC,reg
//|  movsd qword [ACC],reg
//|.endmacro

//|.macro CheckNum,index,val,real_label,int_label
//|.if 1
//|  cmp dword[STK+index*8+4],Value::FLAG_REAL
//|  jb >real_label
//|  cmp dword[STK+index*8+4],Value::FLAG_INTEGER
//|  je >int_label
//|.else
//|  mov T1,val
//|  shr T1,32
//|  cmp T1,Value::FLAG_REAL
//|  jb >real_label
//|  cmp T1,Value::FLAG_INTEGER
//|  je >int_label
//|.endif
//|.endmacro

#define INTERP_HELPER_LIST(__) \
  __(INTERP_START,InterpStart)               \
  __(INTERP_FAIL ,InterpFail)                \
  __(INTERP_RETURN,InterpReturn)             \
  __(INTERP_ARITH_INTL,InterpArithIntL)      \
  __(INTERP_ARITH_INTR,InterpArithIntR)      \
  __(INTERP_ARITH_REALL,InterpArithRealL)    \
  __(INTERP_ARITH_REALR,InterpArithRealR)    \
  __(INTERP_ARITH_VV,InterpArithVV)          \
  __(INTERP_POW_FAST,InterpPowFast)          \
  __(INTERP_POW_SLOW,InterpPowSlow)          \
  __(DIV_BY_ZERO,DivByZero)                  \
  __(MOD_BY_REAL,ModByReal)                  \
  /* ---- debug helper ---- */               \
  __(PRINT_OP,PrintOP)

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
void GenerateMisc( BuildContext* bctx ) {
  /* -------------------------------------------
   * Interpreter Prolog                        |
   * ------------------------------------------*/
  //|=> INTERP_START:
  //|->InterpStart:
  //|  prolog,0
  //|  mov RUNTIME ,CARG1   // runtime
  //|  mov PROTO   ,CARG2   // proto
  //|  mov STK     ,CARG3   // stack
  //|  mov PC      ,CARG4   // pc
  //|  mov DISPATCH,CARG5   // dispatch
  dasm_put(Dst, 0,  INTERP_START);
#line 521 "src/interpreter/bytecode-interpreter.dasc"
  // fire the run
  //|  Dispatch
  dasm_put(Dst, 21);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 32);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 45);
#line 523 "src/interpreter/bytecode-interpreter.dasc"

  /* -------------------------------------------
   * Interpreter exit handler                  |
   * ------------------------------------------*/
  //|=> INTERP_FAIL:
  //|->InterpFail:
  //|  xor rax,rax
  //|  epilog
  //|  ret
  dasm_put(Dst, 62,  INTERP_FAIL);
#line 532 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_RETURN:
  //|->InterpReturn:
  //|  mov rax, qword [ACC]
  //|  mov qword [RUNTIME+RuntimeLayout::kRetOffset],rax
  //|  mov rax,1
  //|  epilog
  //|  ret
  dasm_put(Dst, 70,  INTERP_RETURN, RuntimeLayout::kRetOffset);
#line 540 "src/interpreter/bytecode-interpreter.dasc"
}

/* ------------------------------------------
 * helper functions/routines generation     |
 * -----------------------------------------*/
void GenerateHelper( BuildContext* bctx ) {
  /* ----------------------------------------
   * InterpArithXXX                         |
   * ---------------------------------------*/
  //|.macro arith_handle_ret
  //|  mov T1,rax
  //|  shr T1,32
  //|  cmp T1,Value::TAG_NULL
  //|  je ->InterpFail
  //|  mov qword [ACC], rax
  //|  Dispatch
  //|.endmacro

  //|=> INTERP_ARITH_INTL:
  //|->InterpArithIntL:
  //|  mov CARG1,RUNTIME
  //|  LdInt CARG2L,ARG1
  //|  fcall InterpreterDoArithmetic
  dasm_put(Dst, 96,  INTERP_ARITH_INTL, PrototypeLayout::kIntTableOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic))) {
  dasm_put(Dst, 115);
   } else {
     lava_warn("%s","Function InterpreterDoArithmetic address is not in 0-2GB");
  dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic))>>32));
   }
#line 563 "src/interpreter/bytecode-interpreter.dasc"
  //|  arith_handle_ret
  dasm_put(Dst, 120, Value::TAG_NULL);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 32);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 45);
#line 564 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_ARITH_INTR:
  //|->InterpArithIntR:
  //|  mov CARG1,RUNTIME
  //|  LdInt CARG3L,ARG2
  //|  fcall InterpreterDoArithmetic
  dasm_put(Dst, 156,  INTERP_ARITH_INTR, PrototypeLayout::kIntTableOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic))) {
  dasm_put(Dst, 115);
   } else {
     lava_warn("%s","Function InterpreterDoArithmetic address is not in 0-2GB");
  dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic))>>32));
   }
#line 570 "src/interpreter/bytecode-interpreter.dasc"
  //|  arith_handle_ret
  dasm_put(Dst, 120, Value::TAG_NULL);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 32);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 45);
#line 571 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_ARITH_REALL:
  //|->InterpArithRealL:
  //|  mov CARG1,RUNTIME
  //|  LdReal xmm0,ARG1
  //|  movd CARG2,xmm0
  //|  fcall InterpreterDoArithmetic
  dasm_put(Dst, 175,  INTERP_ARITH_REALL, PrototypeLayout::kRealTableOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic))) {
  dasm_put(Dst, 115);
   } else {
     lava_warn("%s","Function InterpreterDoArithmetic address is not in 0-2GB");
  dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic))>>32));
   }
#line 578 "src/interpreter/bytecode-interpreter.dasc"
  //|  arith_handle_ret
  dasm_put(Dst, 120, Value::TAG_NULL);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 32);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 45);
#line 579 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_ARITH_REALR:
  //|->InterpArithRealR:
  //|  mov CARG1,RUNTIME
  //|  LdReal xmm0,ARG2
  //|  movd CARG3,xmm0
  //|  fcall InterpreterDoArithmetic
  dasm_put(Dst, 202,  INTERP_ARITH_REALR, PrototypeLayout::kRealTableOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic))) {
  dasm_put(Dst, 115);
   } else {
     lava_warn("%s","Function InterpreterDoArithmetic address is not in 0-2GB");
  dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic))>>32));
   }
#line 586 "src/interpreter/bytecode-interpreter.dasc"
  //|  arith_handle_ret
  dasm_put(Dst, 120, Value::TAG_NULL);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 32);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 45);
#line 587 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_ARITH_VV:
  //|->InterpArithVV:
  //|  mov CARG1, RUNTIME
  //|  mov CARG2, qword [STK+ARG1*8]
  //|  mov CARG3, qword [STK+ARG2*8]
  //|  fcall InterpreterDoArithmetic
  dasm_put(Dst, 229,  INTERP_ARITH_VV);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic))) {
  dasm_put(Dst, 115);
   } else {
     lava_warn("%s","Function InterpreterDoArithmetic address is not in 0-2GB");
  dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic))>>32));
   }
#line 594 "src/interpreter/bytecode-interpreter.dasc"
  //|  arith_handle_ret
  dasm_put(Dst, 120, Value::TAG_NULL);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 32);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 45);
#line 595 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_POW_FAST:
  //|->InterpPowFast:
  //|  fcall Pow
  dasm_put(Dst, 244,  INTERP_POW_FAST);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(Pow))) {
  dasm_put(Dst, 248);
   } else {
     lava_warn("%s","Function Pow address is not in 0-2GB");
  dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(Pow)), (unsigned int)((reinterpret_cast<std::uintptr_t>(Pow))>>32));
   }
#line 599 "src/interpreter/bytecode-interpreter.dasc"
  //|  movsd qword [ACC],xmm0
  //|  Dispatch
  dasm_put(Dst, 253);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 32);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 45);
#line 601 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_POW_SLOW:
  //|->InterpPowSlow:
  //|  mov CARG1, RUNTIME
  //|  fcall InterpreterPow
  dasm_put(Dst, 275,  INTERP_POW_SLOW);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterPow))) {
  dasm_put(Dst, 282);
   } else {
     lava_warn("%s","Function InterpreterPow address is not in 0-2GB");
  dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterPow)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterPow))>>32));
   }
#line 606 "src/interpreter/bytecode-interpreter.dasc"
  //|  arith_handle_ret
  dasm_put(Dst, 120, Value::TAG_NULL);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 32);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 45);
#line 607 "src/interpreter/bytecode-interpreter.dasc"

  /* -------------------------------------------
   * Interp Arithmetic Exception               |
   * ------------------------------------------*/
  //|=> DIV_BY_ZERO:
  //|->DivByZero:
  //|  mov CARG1,RUNTIME
  //|  mov CARG2,[PC-4]
  //|  fcall InterpreterDivByZero
  dasm_put(Dst, 287,  DIV_BY_ZERO);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDivByZero))) {
  dasm_put(Dst, 299);
   } else {
     lava_warn("%s","Function InterpreterDivByZero address is not in 0-2GB");
  dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDivByZero)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDivByZero))>>32));
   }
#line 616 "src/interpreter/bytecode-interpreter.dasc"
  //|  jmp ->InterpFail
  dasm_put(Dst, 304);
#line 617 "src/interpreter/bytecode-interpreter.dasc"

  //|=> MOD_BY_REAL:
  //|->ModByReal:
  //|  mov CARG1,RUNTIME
  //|  mov CARG2,[PC-4]
  //|  fcall InterpreterModByReal
  dasm_put(Dst, 309,  MOD_BY_REAL);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterModByReal))) {
  dasm_put(Dst, 321);
   } else {
     lava_warn("%s","Function InterpreterModByReal address is not in 0-2GB");
  dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterModByReal)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterModByReal))>>32));
   }
#line 623 "src/interpreter/bytecode-interpreter.dasc"
  //|  jmp ->InterpFail
  dasm_put(Dst, 304);
#line 624 "src/interpreter/bytecode-interpreter.dasc"

}

void GenerateOneBytecode( BuildContext* bctx, Bytecode bc ) {
  // hack around idiv operator which are used to implement the
  // BC_MODXX and BC_DIVXX instruction. It has a different format
  // and different result/output
  bool arith_div = false;
  bool arith_mod = false;

  switch(bc) {
    /** =====================================================
     *  Call handling                                       |
     *  ====================================================*/
    case BC_RETNULL:
      //|=> bc:
      //|  instr_X
      //|  mov dword [STK+ACCFIDX],Value::FLAG_NULL
      //|  jmp ->InterpReturn
      dasm_put(Dst, 326,  bc, Value::FLAG_NULL);
#line 643 "src/interpreter/bytecode-interpreter.dasc"
      break;
    /** =====================================================
     *  Register Move                                       |
     *  ====================================================*/
    case BC_MOVE:
      //|=> bc:
      //|  instr_E
      //|  mov RREG,qword [STK+ARG2*8]
      //|  mov qword [STK+ARG1*8],RREG
      //|  Dispatch
      dasm_put(Dst, 340,  bc);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 45);
#line 653 "src/interpreter/bytecode-interpreter.dasc"
      break;
    /** =====================================================
     *  Constant Loading                                    |
     *  ====================================================*/
    case BC_LOADI:
      //|=> bc:
      //|  instr_B
      //|  LdInt LREGL,ARG2
      //|  StInt ARG1,LREGL
      //|  Dispatch
      dasm_put(Dst, 366,  bc, PrototypeLayout::kIntTableOffset, Value::FLAG_INTEGER);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 45);
#line 663 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_LOAD0:
      //|=> bc:
      //|  instr_B
      //|  StInt ARG1,0
      //|  Dispatch
      dasm_put(Dst, 406,  bc, Value::FLAG_INTEGER);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 45);
#line 670 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_LOAD1:
      //|=> bc:
      //|  instr_B
      //|  StInt ARG1,1
      //|  Dispatch
      dasm_put(Dst, 438,  bc, Value::FLAG_INTEGER);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 45);
#line 677 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_LOADN1:
      //|=> bc:
      //|  instr_B
      //|  StInt ARG1,-1
      //|  Dispatch
      dasm_put(Dst, 470,  bc, Value::FLAG_INTEGER);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 45);
#line 684 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_LOADR:
      //|=> bc:
      //|  instr_B
      //|  LdReal xmm0,ARG2
      //|  movsd qword [STK+ARG1*8],xmm0
      //|  Dispatch
      dasm_put(Dst, 506,  bc, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 45);
#line 692 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_LOADNULL:
      //|=> bc:
      //|  instr_F
      //|  mov dword [STK+ARG1*8+4],Value::FLAG_NULL
      //|  Dispatch
      dasm_put(Dst, 546,  bc, Value::FLAG_NULL);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 45);
#line 699 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_LOADTRUE:
      //|=> bc:
      //|  instr_F
      //|  mov dword [STK+ARG1*8+4],Value::FLAG_TRUE
      //|  Dispatch
      dasm_put(Dst, 546,  bc, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 45);
#line 706 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_LOADFALSE:
      //|=> bc:
      //|  instr_F
      //|  mov dword [STK+ARG1*8+4],Value::FLAG_FALSE
      //|  Dispatch
      dasm_put(Dst, 546,  bc, Value::FLAG_FALSE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 45);
#line 713 "src/interpreter/bytecode-interpreter.dasc"
      break;

    /** =====================================================
     *  Arith XV                                            |
     *  ====================================================*/
    //|.macro arith_xv_pre,BC,SlowPath
    //|  instr_C
    //|  mov RREG,qword [STK+ARG2*8]
    //|  CheckNum ARG2,RREG,1,2
    //|  mov CARG4L,BC
    //|  jmp ->SlowPath
    //|.endmacro

    //|.macro arith_iv_real,instr
    //|  LdInt2Real xmm0,ARG1
    //|  movd xmm1,RREG
    //|  instr xmm0,xmm1
    //|  StRealACC xmm0
    //|  Dispatch
    //|.endmacro

    //|.macro arith_rv_real,instr
    //|  LdReal xmm0,ARG1
    //|  movd xmm1,RREG
    //|  instr xmm0,xmm1
    //|  StRealACC xmm0
    //|  Dispatch
    //|.endmacro

    //|.macro arith_iv_int,instr
    //|  LdInt LREGL,ARG1
    //|| if( arith_div ) {
    //|    mov eax,LREGL

    //|.if 1
    //|    test RREGL,RREGL
    //|    je ->DivByZero
    //|.endif

    //|    idiv RREGL
    //|    StIntACC eax
    //|| } else if( arith_mod ) {
    //|    mov eax,LREGL
    //|.if 1
    //|    test RREGL,RREGL
    //|    je ->DivByZero
    //|.endif
    //|    idiv RREGL
    //|    StIntACC edx
    //|| } else {
    //|    instr LREGL,RREGL
    //|    StIntACC LREGL
    //|| }
    //|  Dispatch
    //|.endmacro

    //|.macro arith_rv_int,instr
    //|  LdReal xmm0,ARG1
    //|  cvtsi2sd xmm1, RREGL
    //|  instr xmm0,xmm1
    //|  StRealACC xmm0
    //|  Dispatch
    //|.endmacro

    case BC_ADDIV:
      //|=> bc:
      //|  arith_xv_pre BC_ADDIV,InterpArithIntL
      //|1:
      //|  arith_iv_real,addsd
      dasm_put(Dst, 567,  bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_ADDIV, PrototypeLayout::kIntTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
#line 782 "src/interpreter/bytecode-interpreter.dasc"
      //|2:
      //|  arith_iv_int,add
      dasm_put(Dst, 658, PrototypeLayout::kIntTableOffset);
       if( arith_div ) {
      dasm_put(Dst, 689, Value::FLAG_INTEGER);
       } else if( arith_mod ) {
      dasm_put(Dst, 719, Value::FLAG_INTEGER);
       } else {
      dasm_put(Dst, 749, Value::FLAG_INTEGER);
       }
      dasm_put(Dst, 21);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 45);
#line 784 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_ADDRV:
      //|=>bc:
      //|  arith_xv_pre BC_ADDRV,InterpArithRealL
      //|1:
      //|  arith_rv_real,addsd
      dasm_put(Dst, 768, bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_ADDRV, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
#line 791 "src/interpreter/bytecode-interpreter.dasc"
      //|2:
      //|  arith_rv_int,addsd
      dasm_put(Dst, 859, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 45);
#line 793 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_SUBIV:
      //|=>bc:
      //|  arith_xv_pre BC_SUBIV,InterpArithIntL
      //|1:
      //|  arith_iv_real,subsd
      dasm_put(Dst, 924, bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_SUBIV, PrototypeLayout::kIntTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
#line 800 "src/interpreter/bytecode-interpreter.dasc"
      //|2:
      //|  arith_iv_int ,sub
      dasm_put(Dst, 658, PrototypeLayout::kIntTableOffset);
       if( arith_div ) {
      dasm_put(Dst, 689, Value::FLAG_INTEGER);
       } else if( arith_mod ) {
      dasm_put(Dst, 719, Value::FLAG_INTEGER);
       } else {
      dasm_put(Dst, 1015, Value::FLAG_INTEGER);
       }
      dasm_put(Dst, 21);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 45);
#line 802 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_SUBRV:
      //|=>bc:
      //|  arith_xv_pre BC_SUBRV,InterpArithRealL
      //|1:
      //|  arith_rv_real,subsd
      dasm_put(Dst, 1034, bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_SUBRV, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
#line 809 "src/interpreter/bytecode-interpreter.dasc"
      //|2:
      //|  arith_rv_int ,subsd
      dasm_put(Dst, 1125, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 45);
#line 811 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_MULIV:
      //|=>bc:
      //|  arith_xv_pre BC_MULIV,InterpArithIntL
      //|1:
      //|  arith_iv_real,mulsd
      dasm_put(Dst, 1190, bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_MULIV, PrototypeLayout::kIntTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
#line 818 "src/interpreter/bytecode-interpreter.dasc"
      //|2:
      //|  arith_iv_int,imul
      dasm_put(Dst, 658, PrototypeLayout::kIntTableOffset);
       if( arith_div ) {
      dasm_put(Dst, 689, Value::FLAG_INTEGER);
       } else if( arith_mod ) {
      dasm_put(Dst, 719, Value::FLAG_INTEGER);
       } else {
      dasm_put(Dst, 1281, Value::FLAG_INTEGER);
       }
      dasm_put(Dst, 21);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 45);
#line 820 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_MULRV:
      //|=>bc:
      //|  arith_xv_pre BC_MULRV,InterpArithRealL
      //|1:
      //|  arith_rv_real,mulsd
      dasm_put(Dst, 1302, bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_MULRV, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
#line 827 "src/interpreter/bytecode-interpreter.dasc"
      //|2:
      //|  arith_rv_int,mulsd
      dasm_put(Dst, 1393, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 45);
#line 829 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_DIVIV:
      arith_div = true;
      //|=>bc:
      //|  arith_xv_pre BC_DIVIV,InterpArithIntL
      //|1:
      //|  arith_iv_real,mulsd
      dasm_put(Dst, 1190, bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_DIVIV, PrototypeLayout::kIntTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
#line 837 "src/interpreter/bytecode-interpreter.dasc"
      //|2:
      //|  arith_iv_int,imul
      dasm_put(Dst, 658, PrototypeLayout::kIntTableOffset);
       if( arith_div ) {
      dasm_put(Dst, 689, Value::FLAG_INTEGER);
       } else if( arith_mod ) {
      dasm_put(Dst, 719, Value::FLAG_INTEGER);
       } else {
      dasm_put(Dst, 1281, Value::FLAG_INTEGER);
       }
      dasm_put(Dst, 21);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 45);
#line 839 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_DIVRV:
      arith_div = true;
      //|=>bc:
      //|  arith_xv_pre BC_DIVRV,InterpArithRealL
      //|1:
      //|  arith_rv_real,divsd
      dasm_put(Dst, 1458, bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_DIVRV, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
#line 847 "src/interpreter/bytecode-interpreter.dasc"
      //|2:
      //|  arith_rv_int,divsd
      dasm_put(Dst, 1549, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 45);
#line 849 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_MODIV:
      arith_mod = true;
      //|=>bc:
      //|  arith_xv_pre BC_MODIV,InterpArithIntL
      //|1:
      //|  jmp ->ModByReal
      //|2:
      //|  arith_iv_int,imul
      dasm_put(Dst, 1614, bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_MODIV, PrototypeLayout::kIntTableOffset);
       if( arith_div ) {
      dasm_put(Dst, 689, Value::FLAG_INTEGER);
       } else if( arith_mod ) {
      dasm_put(Dst, 719, Value::FLAG_INTEGER);
       } else {
      dasm_put(Dst, 1281, Value::FLAG_INTEGER);
       }
      dasm_put(Dst, 21);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 45);
#line 859 "src/interpreter/bytecode-interpreter.dasc"
      break;

    /* =========================================================
     * Arith VX                                                |
     * ========================================================*/
    //|.macro arith_vx_pre,BC,SlowPath
    //|  instr_B
    //|  mov LREG,qword [STK+ARG1*8]
    //|  CheckNum ARG1,LREG,1,2
    //|  mov CARG4L,BC
    //|  jmp ->SlowPath
    //|.endmacro

    //|.macro arith_vi_real,instr
    //|  LdInt2Real xmm1,ARG2
    //|  movd xmm0,LREG
    //|  instr xmm0,xmm1
    //|  StRealACC xmm0
    //|  Dispatch
    //|.endmacro

    //|.macro arith_vi_int,instr
    //|  LdInt RREGL,ARG2
    //|| if( arith_div ) {
    //|    mov edx,LREGL

    //|.if 1
    //|    test RREGL,RREGL
    //|    jmp ->DivByZero
    //|.endif

    //|    idiv RREGL
    //|    StIntACC eax
    //|| } else if(arith_mod) {
    //|    mov edx,LREGL

    //|.if 1
    //|    test RREGL,RREGL
    //|    jmp ->DivByZero
    //|.endif

    //|    idiv RREGL
    //|    StIntACC edx
    //|| } else {
    //|    instr LREGL,RREGL
    //|    StIntACC LREGL
    //|| }
    //|  Dispatch
    //|.endmacro

    //|.macro arith_vr_real,instr
    //|  LdReal xmm1, ARG2
    //|  movd xmm0,LREG
    //|  instr xmm0,xmm1
    //|  StRealACC xmm0
    //|  Dispatch
    //|.endmacro

    //|.macro arith_vr_int,instr
    //|  LdReal xmm1,ARG2
    //|  cvtsi2sd xmm0, LREGL
    //|  instr xmm0,xmm1
    //|  StRealACC xmm0
    //|  Dispatch
    //|.endmacro

    case BC_ADDVI:
      //|=> bc:
      //|  arith_vx_pre BC_ADDVI,InterpArithIntR
      //|1:
      //|  arith_vi_real addsd
      dasm_put(Dst, 1677,  bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_ADDVI, PrototypeLayout::kIntTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
#line 930 "src/interpreter/bytecode-interpreter.dasc"
      //|2:
      //|  arith_vi_int  add
      dasm_put(Dst, 1765, PrototypeLayout::kIntTableOffset);
       if( arith_div ) {
      dasm_put(Dst, 1796, Value::FLAG_INTEGER);
       } else if(arith_mod) {
      dasm_put(Dst, 1826, Value::FLAG_INTEGER);
       } else {
      dasm_put(Dst, 749, Value::FLAG_INTEGER);
       }
      dasm_put(Dst, 21);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 45);
#line 932 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_ADDVR:
      //|=> bc:
      //|  arith_vx_pre BC_ADDVR,InterpArithRealR
      //|1:
      //|  arith_vr_real addsd
      dasm_put(Dst, 1856,  bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_ADDVR, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
#line 939 "src/interpreter/bytecode-interpreter.dasc"
      //|2:
      //|  arith_vr_int addsd
      dasm_put(Dst, 1944, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 45);
#line 941 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_SUBVI:
      //|=> bc:
      //|  arith_vx_pre BC_SUBVI,InterpArithIntR
      //|1:
      //|  arith_vi_real subsd
      dasm_put(Dst, 2009,  bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_SUBVI, PrototypeLayout::kIntTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
#line 948 "src/interpreter/bytecode-interpreter.dasc"
      //|2:
      //|  arith_vi_int sub
      dasm_put(Dst, 1765, PrototypeLayout::kIntTableOffset);
       if( arith_div ) {
      dasm_put(Dst, 1796, Value::FLAG_INTEGER);
       } else if(arith_mod) {
      dasm_put(Dst, 1826, Value::FLAG_INTEGER);
       } else {
      dasm_put(Dst, 1015, Value::FLAG_INTEGER);
       }
      dasm_put(Dst, 21);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 45);
#line 950 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_SUBVR:
      //|=> bc:
      //|  arith_vx_pre BC_SUBVR,InterpArithRealR
      //|1:
      //|  arith_vr_real subsd
      dasm_put(Dst, 2097,  bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_SUBVR, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
#line 957 "src/interpreter/bytecode-interpreter.dasc"
      //|2:
      //|  arith_vr_int subsd
      dasm_put(Dst, 2185, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 45);
#line 959 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_MULVI:
      //|=> bc:
      //|  arith_vx_pre BC_MULVI,InterpArithIntR
      //|1:
      //|  arith_vi_real mulsd
      dasm_put(Dst, 2250,  bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_MULVI, PrototypeLayout::kIntTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
#line 966 "src/interpreter/bytecode-interpreter.dasc"
      //|2:
      //|  arith_vi_int imul
      dasm_put(Dst, 1765, PrototypeLayout::kIntTableOffset);
       if( arith_div ) {
      dasm_put(Dst, 1796, Value::FLAG_INTEGER);
       } else if(arith_mod) {
      dasm_put(Dst, 1826, Value::FLAG_INTEGER);
       } else {
      dasm_put(Dst, 1281, Value::FLAG_INTEGER);
       }
      dasm_put(Dst, 21);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 45);
#line 968 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_MULVR:
      //|=> bc:
      //|  arith_vx_pre BC_MULVR,InterpArithRealR
      //|1:
      //|  arith_vr_real mulsd
      dasm_put(Dst, 2338,  bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_MULVR, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
#line 975 "src/interpreter/bytecode-interpreter.dasc"
      //|2:
      //|  arith_vr_real mulsd
      dasm_put(Dst, 2426, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 45);
#line 977 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_DIVVI:
      //|=> bc:
      dasm_put(Dst, 2491,  bc);
#line 981 "src/interpreter/bytecode-interpreter.dasc"
      arith_div = true;
      //|  arith_vx_pre BC_DIVVI,InterpArithIntR
      //|1:
      //|  arith_vi_real divsd
      dasm_put(Dst, 2493, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_DIVVI, PrototypeLayout::kIntTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
#line 985 "src/interpreter/bytecode-interpreter.dasc"
      //|2:
      //|  arith_vi_int sub
      dasm_put(Dst, 1765, PrototypeLayout::kIntTableOffset);
       if( arith_div ) {
      dasm_put(Dst, 1796, Value::FLAG_INTEGER);
       } else if(arith_mod) {
      dasm_put(Dst, 1826, Value::FLAG_INTEGER);
       } else {
      dasm_put(Dst, 1015, Value::FLAG_INTEGER);
       }
      dasm_put(Dst, 21);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 45);
#line 987 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_DIVVR:
      //|=> bc:
      //|  arith_vx_pre BC_DIVVR,InterpArithRealR
      //|1:
      //|  arith_vr_real divsd
      dasm_put(Dst, 2580,  bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_DIVVR, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
#line 994 "src/interpreter/bytecode-interpreter.dasc"
      //|2:
      //|  arith_vr_int divsd
      dasm_put(Dst, 2668, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 45);
#line 996 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_MODVI:
      arith_mod = true;
      //|=> bc:
      //|  arith_vx_pre BC_MODVI,InterpArithIntR
      //|1:
      //|  jmp ->ModByReal
      //|2:
      //|  arith_vi_int imul
      dasm_put(Dst, 2733,  bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_MODVI, PrototypeLayout::kIntTableOffset);
       if( arith_div ) {
      dasm_put(Dst, 1796, Value::FLAG_INTEGER);
       } else if(arith_mod) {
      dasm_put(Dst, 1826, Value::FLAG_INTEGER);
       } else {
      dasm_put(Dst, 1281, Value::FLAG_INTEGER);
       }
      dasm_put(Dst, 21);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 45);
#line 1006 "src/interpreter/bytecode-interpreter.dasc"
      break;

    /* ========================================================
     * ArithVV
     *
     * The arithVV is also optimized for common path here.
     * We inline all numeric calculation cases, int/real.
     * Other cases will be pushed back to call C++ function
     * which may be extended to support meta function call
     * ========================================================*/

    // perform VV calaculation based on instruction
    //|.macro arith_vv,BC,instrI,setterI,instrR,setterR
    //|  instr_D
    //|  mov LREGL,dword[STK+ARG1*8+4]
    //|  mov RREGL,dword[STK+ARG2*8+4]

    // here we will do a type check and also promotion
    //|  cmp LREGL,Value::FLAG_INTEGER
    //|  je >1
    //|  cmp RREGL,Value::FLAG_REAL
    //|  jb >2
    //|  jmp >6 // cannot handle

    //|1:
    //|  cmp RREGL,Value::FLAG_INTEGER
    //|  je >4 // int && int
    //|  cmp RREGL,Value::FLAG_REAL
    //|  jnb >6 // cannot handle

    // promoting LHS->real
    //|  cvtsi2sd xmm0,dword [STK+ARG1*8]
    //|  instrR xmm0,qword [STK+ARG2*8]
    //|  setterR xmm0
    //|  Dispatch

    //|2:
    //|  cmp RREGL,Value::FLAG_REAL
    //|  jb >5  // real && real
    //|  cmp RREGL,Value::FLAG_INTEGER
    //|  jne >6 // cannot handle

    // promoting RHS->real
    //|  cvtsi2sd xmm1,dword [STK+ARG2*8]
    //|  movsd xmm0,qword [STK+ARG1*8]
    //|  setterR xmm0
    //|  Dispatch

    // int && int
    //|4:
    //|| if( arith_div ) {
    //|    mov edx,dword [STK+ARG1*8]
    //|    idiv dword [STK+ARG2*8]
    //|    setterI eax
    //|| } else if (arith_mod) {
    //|    mov edx,dword [STK+ARG1*8]
    //|    idiv dword [STK+ARG2*8]
    //|    setterI edx
    //|| } else {
    //|    mov LREGL,dword [STK+ARG1*8]
    //|    instrI LREGL,dword [STK+ARG2*8]
    //|    setterI LREGL
    //|| }
    //|  Dispatch

    // real && real
    //|5:
    //|| if( arith_mod ) {
    //|    jmp ->ModByReal
    //|| } else {
    //|    movsd xmm0,qword [STK+ARG1*8]
    //|    instrR xmm0,qword [STK+ARG2*8]
    //|    setterR xmm0
    //|| }
    //|  Dispatch

    // slow path
    //|6:
    //|  mov CARG4L,BC
    //|  jmp ->InterpArithVV

    //|.endmacro

    case BC_ADDVV:
      //|=> bc:
      //|  arith_vv BC_ADDVV,add,StIntACC,addsd,StRealACC
      dasm_put(Dst, 2793,  bc, Value::FLAG_INTEGER, Value::FLAG_REAL, Value::FLAG_INTEGER, Value::FLAG_REAL);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2890, Value::FLAG_REAL, Value::FLAG_INTEGER);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2960);
       if( arith_div ) {
      dasm_put(Dst, 2979, Value::FLAG_INTEGER);
       } else if (arith_mod) {
      dasm_put(Dst, 3005, Value::FLAG_INTEGER);
       } else {
      dasm_put(Dst, 3031, Value::FLAG_INTEGER);
       }
      dasm_put(Dst, 21);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 3056);
       if( arith_mod ) {
      dasm_put(Dst, 3075);
       } else {
      dasm_put(Dst, 3080);
       }
      dasm_put(Dst, 21);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 3106, BC_ADDVV);
#line 1092 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_SUBVV:
      //|=> bc:
      //|  arith_vv BC_SUBVV,sub,StIntACC,subsd,StRealACC
      dasm_put(Dst, 3131,  bc, Value::FLAG_INTEGER, Value::FLAG_REAL, Value::FLAG_INTEGER, Value::FLAG_REAL);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2890, Value::FLAG_REAL, Value::FLAG_INTEGER);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2960);
       if( arith_div ) {
      dasm_put(Dst, 2979, Value::FLAG_INTEGER);
       } else if (arith_mod) {
      dasm_put(Dst, 3005, Value::FLAG_INTEGER);
       } else {
      dasm_put(Dst, 3228, Value::FLAG_INTEGER);
       }
      dasm_put(Dst, 21);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 3056);
       if( arith_mod ) {
      dasm_put(Dst, 3075);
       } else {
      dasm_put(Dst, 3253);
       }
      dasm_put(Dst, 21);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 3106, BC_SUBVV);
#line 1096 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_MULVV:
      //|=> bc:
      //|  arith_vv BC_MULVV,imul,StIntACC,mulsd,StRealACC
      dasm_put(Dst, 3279,  bc, Value::FLAG_INTEGER, Value::FLAG_REAL, Value::FLAG_INTEGER, Value::FLAG_REAL);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2890, Value::FLAG_REAL, Value::FLAG_INTEGER);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2960);
       if( arith_div ) {
      dasm_put(Dst, 2979, Value::FLAG_INTEGER);
       } else if (arith_mod) {
      dasm_put(Dst, 3005, Value::FLAG_INTEGER);
       } else {
      dasm_put(Dst, 3376, Value::FLAG_INTEGER);
       }
      dasm_put(Dst, 21);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 3056);
       if( arith_mod ) {
      dasm_put(Dst, 3075);
       } else {
      dasm_put(Dst, 3402);
       }
      dasm_put(Dst, 21);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 3106, BC_MULVV);
#line 1100 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_DIVVV:
      arith_div = true;
      //|=> bc:
      //|  arith_vv BC_DIVVV,imul,StIntACC,divsd,StRealACC
      dasm_put(Dst, 3428,  bc, Value::FLAG_INTEGER, Value::FLAG_REAL, Value::FLAG_INTEGER, Value::FLAG_REAL);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2890, Value::FLAG_REAL, Value::FLAG_INTEGER);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2960);
       if( arith_div ) {
      dasm_put(Dst, 2979, Value::FLAG_INTEGER);
       } else if (arith_mod) {
      dasm_put(Dst, 3005, Value::FLAG_INTEGER);
       } else {
      dasm_put(Dst, 3376, Value::FLAG_INTEGER);
       }
      dasm_put(Dst, 21);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 3056);
       if( arith_mod ) {
      dasm_put(Dst, 3075);
       } else {
      dasm_put(Dst, 3525);
       }
      dasm_put(Dst, 21);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 3106, BC_DIVVV);
#line 1105 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_MODVV:
      arith_mod = true;
      //|=> bc:
      //|  arith_vv BC_MODVV,imul,StIntACC,divsd,StRealACC
      dasm_put(Dst, 3428,  bc, Value::FLAG_INTEGER, Value::FLAG_REAL, Value::FLAG_INTEGER, Value::FLAG_REAL);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2890, Value::FLAG_REAL, Value::FLAG_INTEGER);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2960);
       if( arith_div ) {
      dasm_put(Dst, 2979, Value::FLAG_INTEGER);
       } else if (arith_mod) {
      dasm_put(Dst, 3005, Value::FLAG_INTEGER);
       } else {
      dasm_put(Dst, 3376, Value::FLAG_INTEGER);
       }
      dasm_put(Dst, 21);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 3056);
       if( arith_mod ) {
      dasm_put(Dst, 3075);
       } else {
      dasm_put(Dst, 3525);
       }
      dasm_put(Dst, 21);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 32);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 37, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 3106, BC_MODVV);
#line 1110 "src/interpreter/bytecode-interpreter.dasc"
      break;

    /* ==========================================================
     * POW part
     *
     * Currently we directly use std::pow/pow in libc for simplicity.
     * For numeric type we will directly call pow for other types
     * we will fallback to slow C++ function
     * =========================================================*/

    //|.macro pow_promo,REGL,XREG,ARG
    //|  mov REGL,dword [STK+ARG*8+4]
    //|  cmp REGL,Value::FLAG_REAL
    //|  jb >1
    //|  cmp REGL,Value::FLAG_INTEGER
    //|  jne >2
    //|  cvtsi2sd XREG,qword [STK+ARG*8]
    //|.endmacro

    case BC_POWIV:
      //|=> bc:
      //|  instr_C
      //|  LdInt2Real,xmm0,ARG1
      //|  pow_promo,RREGL,xmm1,ARG2
      //|1:
      //|  jmp ->InterpPowFast
      //|2:
      //|  LdIntV CARG2,CARG2L,ARG1
      //|  mov CARG3, qword [STK+ARG2*8]
      //|  mov CARG4, BC_POWIV
      //|  jmp ->InterpPowSlow
      dasm_put(Dst, 3551,  bc, PrototypeLayout::kIntTableOffset, Value::FLAG_REAL, Value::FLAG_INTEGER, PrototypeLayout::kIntTableOffset, Value::FLAG_INTEGER, BC_POWIV);
#line 1141 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_POWVI:
      //|=> bc:
      //|  instr_B
      //|  LdInt2Real,xmm1,ARG2
      //|  pow_promo,LREGL,xmm0,ARG1
      //|1:
      //|  jmp ->InterpPowFast
      //|2:
      //|  mov CARG2, qword [STK+ARG1*8]
      //|  LdIntV CARG3,CARG3L,ARG2
      //|  mov CARG4, BC_POWVI
      //|  jmp ->InterpPowSlow
      dasm_put(Dst, 3645,  bc, PrototypeLayout::kIntTableOffset, Value::FLAG_REAL, Value::FLAG_INTEGER, PrototypeLayout::kIntTableOffset, Value::FLAG_INTEGER, BC_POWVI);
#line 1155 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_POWRV:
      //|=> bc:
      //|  instr_C
      //|  LdReal xmm0,ARG1
      //|  pow_promo RREGL,xmm1,ARG2
      //|1:
      //|  jmp ->InterpPowFast
      //|2:
      //|  LdRealV CARG2,ARG1
      //|  mov CARG3,qword [STK+ARG2*8]
      //|  mov CARG4,BC_POWRV
      //|  jmp ->InterpPowSlow
      dasm_put(Dst, 3736,  bc, PrototypeLayout::kRealTableOffset, Value::FLAG_REAL, Value::FLAG_INTEGER, PrototypeLayout::kRealTableOffset, BC_POWRV);
#line 1169 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_POWVR:
      //|=> bc:
      //|  instr_C
      //|  LdReal xmm1,ARG2
      //|  pow_promo LREGL,xmm0,ARG1
      //|1:
      //|  jmp ->InterpPowFast
      //|2:
      //|  LdRealV CARG3,ARG2
      //|  mov CARG2,qword [STK+ARG1*8]
      //|  mov CARG4,BC_POWVR
      //|  jmp ->InterpPowSlow
      dasm_put(Dst, 3822,  bc, PrototypeLayout::kRealTableOffset, Value::FLAG_REAL, Value::FLAG_INTEGER, PrototypeLayout::kRealTableOffset, BC_POWVR);
#line 1183 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_POWVV:
      //|=> bc:
      //|  instr_C
      //|  mov CARG2,qword [STK+ARG1*8]
      //|  mov CARG3,qword [STK+ARG2*8]
      //|  mov CARG4,BC_POWVV
      //|  jmp ->InterpPowSlow
      dasm_put(Dst, 3908,  bc, BC_POWVV);
#line 1192 "src/interpreter/bytecode-interpreter.dasc"
      break;
    default:
      //|=> bc:
      //|  Break
      dasm_put(Dst, 3935,  bc);
#line 1196 "src/interpreter/bytecode-interpreter.dasc"
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
  code_size_      (),
  buffer_size_    ()
{}

AssemblyInterpreter::~AssemblyInterpreter() {
  if(interp_entry_) OS::FreeCodePage(interp_entry_,buffer_size_);
}

std::shared_ptr<AssemblyInterpreter> AssemblyInterpreter::Generate() {
  std::shared_ptr<AssemblyInterpreter> interp( new AssemblyInterpreter() );

  // create a build context
  BuildContext bctx;

  // initialize dasm_State object
  dasm_init(&(bctx.dasm_ctx),1);

  // setup the freaking global
  void* glb_arr[GLBNAME__MAX];
  dasm_setupglobal(&(bctx.dasm_ctx),glb_arr,GLBNAME__MAX);

  // setup the dasm
  dasm_setup(&(bctx.dasm_ctx),actions);

  // initialize the tag value needed , at least for each BC we need one
  bctx.tag = DASM_GROWABLE_PC_SIZE;
  dasm_growpc(&(bctx.dasm_ctx), DASM_GROWABLE_PC_SIZE );

  // build the prolog
  GenerateMisc(&bctx);

  // build the helper
  GenerateHelper(&bctx);

  // generate all bytecode's routine
  for( int i = static_cast<int>(BC_ADDIV) ; i < SIZE_OF_BYTECODE ; ++i ) {
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
  for( int i = static_cast<int>(BC_ADDIV) ; i < SIZE_OF_BYTECODE ; ++i ) {
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

  interp->interp_entry_ = buffer;
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
  ZydisDecoderInit(
      &decoder,
      ZYDIS_MACHINE_MODE_LONG_64,
      ZYDIS_ADDRESS_WIDTH_64);

  ZydisFormatter formatter;
  ZydisFormatterInit(&formatter,ZYDIS_FORMATTER_STYLE_INTEL);

  std::uint64_t pc = reinterpret_cast<std::uint64_t>(interp_entry_);
  std::uint8_t* rp = static_cast<std::uint8_t*>(interp_entry_);
  std::size_t size = code_size_;

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
  interp_         (interp)
{
  memcpy(dispatch_interp_,interp->dispatch_interp_,sizeof(dispatch_interp_));
  memcpy(dispatch_record_,interp->dispatch_record_,sizeof(dispatch_record_));
  memcpy(dispatch_jit_   ,interp->dispatch_jit_   ,sizeof(dispatch_jit_   ));
}

bool AssemblyInterpreter::Instance::Run( Context* context , const Handle<Script>& script ,
                                                           const Handle<Object>& globals,
                                                           std::string* error,
                                                           Value* rval ) {
  Runtime runtime(script,globals,error);
  Main m = reinterpret_cast<Main>(interp_->interp_entry_);
  bool ret = m(&runtime,runtime.cur,reinterpret_cast<void*>(runtime.stack),
                                    const_cast<void*>(
                                      reinterpret_cast<const void*>((*runtime.cur)->code_buffer())),
                                    dispatch_interp_);
  if(ret) {
    *rval = runtime.ret;
  }
  return ret;
}

} // namespace lavascript
} // namespace interpreter
