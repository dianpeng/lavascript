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

#include <math.h>
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


/**
 * The protocol of this function is carefully arranged and have dependency
 * of how we orgnize our assembly code. So do not modify the protocol unless
 * you know what is going on
 */
Value InterpreterDoArithmetic( Sandbox* sandbox ,
                               Value left ,  // rsi
                               Value right , // rdx
                               Bytecode bc ) {
  (void)sandbox;
  (void)left;
  (void)right;
  (void)bc;
  return Value();
}

Value InterpreterPow         ( Sandbox* sandbox ,
                               Value left,
                               Value right,
                               Bytecode bc ) {
  (void)sandbox;
  (void)left;
  (void)right;
  (void)bc;
  return Value();
}

void InterpreterModByReal    ( Sandbox* sandbox , std::uint32_t* pc ) {
  (void)sandbox;
  (void)pc;
}

void InterpreterDivByZero    ( Sandbox* sandbox , std::uint32_t* pc ) {
  (void)sandbox;
  (void)pc;
}

double Pow( double a , double b ) {
  return pow(a,b);
}

// A frame object that is used to record the function's runtime information
struct Frame {
  void* caller;
  std::int32_t offset;
};

struct BuildContext {
  dasm_State* dasm_ctx;
  int tag;
};

//|.arch x64
#if DASM_VERSION != 10300
#error "Version mismatch between DynASM and included encoding engine"
#endif
#line 74 "src/interpreter/bytecode-interpreter.dasc"
//|.actionlist actions
static const unsigned char actions[3990] = {
  248,10,76,137,231,77,139,93,0,77,139,155,233,65,139,52,155,255,232,251,1,
  0,255,72,184,237,237,252,255,208,255,73,137,195,73,193,252,235,32,73,129,
  252,251,239,15,132,244,11,73,137,134,252,252,3,0,0,139,3,72,15,182,216,72,
  131,195,4,65,252,255,36,223,255,248,12,76,137,231,77,139,93,0,77,139,155,
  233,65,139,20,131,255,248,13,76,137,231,77,139,93,0,77,139,155,233,252,242,
  65,15,16,4,219,102,72,15,126,198,255,248,14,76,137,231,77,139,93,0,77,139,
  155,233,252,242,65,15,16,4,195,102,72,15,126,194,255,248,15,76,137,231,73,
  139,52,222,73,139,20,198,255,248,16,255,232,251,1,1,255,252,242,65,15,17,
  134,252,252,3,0,0,139,3,72,15,182,216,72,131,195,4,65,252,255,36,223,255,
  248,17,76,137,231,255,232,251,1,2,255,248,18,76,137,231,72,139,115,252,252,
  255,232,251,1,3,255,252,233,244,11,255,248,19,76,137,231,72,139,115,252,252,
  255,232,251,1,4,255,248,11,72,49,192,195,255,248,20,73,139,134,252,252,3,
  0,0,73,137,132,253,36,233,72,199,192,1,0,0,0,195,255,249,65,199,134,0,4,0,
  0,237,252,233,244,20,255,249,72,15,182,220,193,232,16,73,139,20,198,73,137,
  20,222,139,3,72,15,182,216,72,131,195,4,65,252,255,36,223,255,249,72,15,182,
  220,193,232,16,77,139,93,0,77,139,155,233,65,139,52,131,65,137,52,222,65,
  199,68,222,4,237,139,3,72,15,182,216,72,131,195,4,65,252,255,36,223,255,249,
  72,15,182,220,193,232,16,65,199,4,222,0,0,0,0,65,199,68,222,4,237,139,3,72,
  15,182,216,72,131,195,4,65,252,255,36,223,255,249,72,15,182,220,193,232,16,
  65,199,4,222,1,0,0,0,65,199,68,222,4,237,139,3,72,15,182,216,72,131,195,4,
  65,252,255,36,223,255,249,72,15,182,220,193,232,16,65,199,4,222,252,255,252,
  255,252,255,252,255,65,199,68,222,4,237,139,3,72,15,182,216,72,131,195,4,
  65,252,255,36,223,255,249,72,15,182,220,193,232,16,77,139,93,0,77,139,155,
  233,252,242,65,15,16,4,195,252,242,65,15,17,4,222,139,3,72,15,182,216,72,
  131,195,4,65,252,255,36,223,255,249,72,15,182,220,65,199,68,222,4,237,139,
  3,72,15,182,216,72,131,195,4,65,252,255,36,223,255,249,193,232,8,72,15,183,
  216,193,232,16,73,139,20,198,65,129,124,253,198,4,239,15,130,244,247,65,129,
  124,253,198,4,239,15,132,244,248,185,237,252,233,244,10,248,1,77,139,93,0,
  77,139,155,233,252,242,65,15,42,4,155,102,72,15,110,202,252,242,15,88,193,
  252,242,65,15,17,134,252,252,3,0,0,139,3,72,15,182,216,72,131,195,4,65,252,
  255,36,223,248,2,77,139,93,0,77,139,155,233,65,139,52,155,255,137,252,240,
  133,210,15,132,244,18,252,247,252,250,65,137,134,252,252,3,0,0,65,199,134,
  0,4,0,0,237,255,137,252,240,133,210,15,132,244,18,252,247,252,250,65,137,
  150,252,252,3,0,0,65,199,134,0,4,0,0,237,255,1,214,65,137,182,252,252,3,0,
  0,65,199,134,0,4,0,0,237,255,249,193,232,8,72,15,183,216,193,232,16,73,139,
  20,198,65,129,124,253,198,4,239,15,130,244,247,65,129,124,253,198,4,239,15,
  132,244,248,185,237,252,233,244,13,248,1,77,139,93,0,77,139,155,233,252,242,
  65,15,16,4,219,102,72,15,110,202,252,242,15,88,193,252,242,65,15,17,134,252,
  252,3,0,0,139,3,72,15,182,216,72,131,195,4,65,252,255,36,223,248,2,77,139,
  93,0,77,139,155,233,252,242,65,15,16,4,219,252,242,15,42,202,252,242,15,88,
  193,252,242,65,15,17,134,252,252,3,0,0,139,3,72,15,182,216,72,131,195,4,65,
  252,255,36,223,255,249,193,232,8,72,15,183,216,193,232,16,73,139,20,198,65,
  129,124,253,198,4,239,15,130,244,247,65,129,124,253,198,4,239,15,132,244,
  248,185,237,252,233,244,10,248,1,77,139,93,0,77,139,155,233,252,242,65,15,
  42,4,155,102,72,15,110,202,252,242,15,92,193,252,242,65,15,17,134,252,252,
  3,0,0,139,3,72,15,182,216,72,131,195,4,65,252,255,36,223,248,2,77,139,93,
  0,77,139,155,233,65,139,52,155,255,41,214,65,137,182,252,252,3,0,0,65,199,
  134,0,4,0,0,237,255,249,193,232,8,72,15,183,216,193,232,16,73,139,20,198,
  65,129,124,253,198,4,239,15,130,244,247,65,129,124,253,198,4,239,15,132,244,
  248,185,237,252,233,244,13,248,1,77,139,93,0,77,139,155,233,252,242,65,15,
  16,4,219,102,72,15,110,202,252,242,15,92,193,252,242,65,15,17,134,252,252,
  3,0,0,139,3,72,15,182,216,72,131,195,4,65,252,255,36,223,248,2,77,139,93,
  0,77,139,155,233,252,242,65,15,16,4,219,252,242,15,42,202,252,242,15,92,193,
  252,242,65,15,17,134,252,252,3,0,0,139,3,72,15,182,216,72,131,195,4,65,252,
  255,36,223,255,249,193,232,8,72,15,183,216,193,232,16,73,139,20,198,65,129,
  124,253,198,4,239,15,130,244,247,65,129,124,253,198,4,239,15,132,244,248,
  185,237,252,233,244,10,248,1,77,139,93,0,77,139,155,233,252,242,65,15,42,
  4,155,102,72,15,110,202,252,242,15,89,193,252,242,65,15,17,134,252,252,3,
  0,0,139,3,72,15,182,216,72,131,195,4,65,252,255,36,223,248,2,77,139,93,0,
  77,139,155,233,65,139,52,155,255,15,175,252,242,65,137,182,252,252,3,0,0,
  65,199,134,0,4,0,0,237,255,249,193,232,8,72,15,183,216,193,232,16,73,139,
  20,198,65,129,124,253,198,4,239,15,130,244,247,65,129,124,253,198,4,239,15,
  132,244,248,185,237,252,233,244,13,248,1,77,139,93,0,77,139,155,233,252,242,
  65,15,16,4,219,102,72,15,110,202,252,242,15,89,193,252,242,65,15,17,134,252,
  252,3,0,0,139,3,72,15,182,216,72,131,195,4,65,252,255,36,223,248,2,77,139,
  93,0,77,139,155,233,252,242,65,15,16,4,219,252,242,15,42,202,252,242,15,89,
  193,252,242,65,15,17,134,252,252,3,0,0,139,3,72,15,182,216,72,131,195,4,65,
  252,255,36,223,255,249,193,232,8,72,15,183,216,193,232,16,73,139,20,198,65,
  129,124,253,198,4,239,15,130,244,247,65,129,124,253,198,4,239,15,132,244,
  248,185,237,252,233,244,13,248,1,77,139,93,0,77,139,155,233,252,242,65,15,
  16,4,219,102,72,15,110,202,252,242,15,94,193,252,242,65,15,17,134,252,252,
  3,0,0,139,3,72,15,182,216,72,131,195,4,65,252,255,36,223,248,2,77,139,93,
  0,77,139,155,233,252,242,65,15,16,4,219,252,242,15,42,202,252,242,15,94,193,
  252,242,65,15,17,134,252,252,3,0,0,139,3,72,15,182,216,72,131,195,4,65,252,
  255,36,223,255,249,193,232,8,72,15,183,216,193,232,16,73,139,20,198,65,129,
  124,253,198,4,239,15,130,244,247,65,129,124,253,198,4,239,15,132,244,248,
  185,237,252,233,244,10,248,1,252,233,244,19,248,2,77,139,93,0,77,139,155,
  233,65,139,52,155,255,249,72,15,182,220,193,232,16,73,139,52,222,65,129,124,
  253,222,4,239,15,130,244,247,65,129,124,253,222,4,239,15,132,244,248,185,
  237,252,233,244,12,248,1,77,139,93,0,77,139,155,233,252,242,65,15,42,12,131,
  102,72,15,110,198,252,242,15,88,193,252,242,65,15,17,134,252,252,3,0,0,139,
  3,72,15,182,216,72,131,195,4,65,252,255,36,223,248,2,77,139,93,0,77,139,155,
  233,65,139,20,131,255,137,252,242,133,210,252,233,244,18,252,247,252,250,
  65,137,134,252,252,3,0,0,65,199,134,0,4,0,0,237,255,137,252,242,133,210,252,
  233,244,18,252,247,252,250,65,137,150,252,252,3,0,0,65,199,134,0,4,0,0,237,
  255,249,72,15,182,220,193,232,16,73,139,52,222,65,129,124,253,222,4,239,15,
  130,244,247,65,129,124,253,222,4,239,15,132,244,248,185,237,252,233,244,14,
  248,1,77,139,93,0,77,139,155,233,252,242,65,15,16,12,195,102,72,15,110,198,
  252,242,15,88,193,252,242,65,15,17,134,252,252,3,0,0,139,3,72,15,182,216,
  72,131,195,4,65,252,255,36,223,248,2,77,139,93,0,77,139,155,233,252,242,65,
  15,16,12,195,252,242,15,42,198,252,242,15,88,193,252,242,65,15,17,134,252,
  252,3,0,0,139,3,72,15,182,216,72,131,195,4,65,252,255,36,223,255,249,72,15,
  182,220,193,232,16,73,139,52,222,65,129,124,253,222,4,239,15,130,244,247,
  65,129,124,253,222,4,239,15,132,244,248,185,237,252,233,244,12,248,1,77,139,
  93,0,77,139,155,233,252,242,65,15,42,12,131,102,72,15,110,198,252,242,15,
  92,193,252,242,65,15,17,134,252,252,3,0,0,139,3,72,15,182,216,72,131,195,
  4,65,252,255,36,223,248,2,77,139,93,0,77,139,155,233,65,139,20,131,255,249,
  72,15,182,220,193,232,16,73,139,52,222,65,129,124,253,222,4,239,15,130,244,
  247,65,129,124,253,222,4,239,15,132,244,248,185,237,252,233,244,14,248,1,
  77,139,93,0,77,139,155,233,252,242,65,15,16,12,195,102,72,15,110,198,252,
  242,15,92,193,252,242,65,15,17,134,252,252,3,0,0,139,3,72,15,182,216,72,131,
  195,4,65,252,255,36,223,248,2,77,139,93,0,77,139,155,233,252,242,65,15,16,
  12,195,252,242,15,42,198,252,242,15,92,193,252,242,65,15,17,134,252,252,3,
  0,0,139,3,72,15,182,216,72,131,195,4,65,252,255,36,223,255,249,72,15,182,
  220,193,232,16,73,139,52,222,65,129,124,253,222,4,239,15,130,244,247,65,129,
  124,253,222,4,239,15,132,244,248,185,237,252,233,244,12,248,1,77,139,93,0,
  77,139,155,233,252,242,65,15,42,12,131,102,72,15,110,198,252,242,15,89,193,
  252,242,65,15,17,134,252,252,3,0,0,139,3,72,15,182,216,72,131,195,4,65,252,
  255,36,223,248,2,77,139,93,0,77,139,155,233,65,139,20,131,255,249,72,15,182,
  220,193,232,16,73,139,52,222,65,129,124,253,222,4,239,15,130,244,247,65,129,
  124,253,222,4,239,15,132,244,248,185,237,252,233,244,14,248,1,77,139,93,0,
  77,139,155,233,252,242,65,15,16,12,195,102,72,15,110,198,252,242,15,89,193,
  252,242,65,15,17,134,252,252,3,0,0,139,3,72,15,182,216,72,131,195,4,65,252,
  255,36,223,248,2,77,139,93,0,77,139,155,233,252,242,65,15,16,12,195,102,72,
  15,110,198,252,242,15,89,193,252,242,65,15,17,134,252,252,3,0,0,139,3,72,
  15,182,216,72,131,195,4,65,252,255,36,223,255,249,255,72,15,182,220,193,232,
  16,73,139,52,222,65,129,124,253,222,4,239,15,130,244,247,65,129,124,253,222,
  4,239,15,132,244,248,185,237,252,233,244,12,248,1,77,139,93,0,77,139,155,
  233,252,242,65,15,42,12,131,102,72,15,110,198,252,242,15,94,193,252,242,65,
  15,17,134,252,252,3,0,0,139,3,72,15,182,216,72,131,195,4,65,252,255,36,223,
  248,2,77,139,93,0,77,139,155,233,65,139,20,131,255,249,72,15,182,220,193,
  232,16,73,139,52,222,65,129,124,253,222,4,239,15,130,244,247,65,129,124,253,
  222,4,239,15,132,244,248,185,237,252,233,244,14,248,1,77,139,93,0,77,139,
  155,233,252,242,65,15,16,12,195,102,72,15,110,198,252,242,15,94,193,252,242,
  65,15,17,134,252,252,3,0,0,139,3,72,15,182,216,72,131,195,4,65,252,255,36,
  223,248,2,77,139,93,0,77,139,155,233,252,242,65,15,16,12,195,252,242,15,42,
  198,252,242,15,94,193,252,242,65,15,17,134,252,252,3,0,0,139,3,72,15,182,
  216,72,131,195,4,65,252,255,36,223,255,249,72,15,182,220,193,232,16,73,139,
  52,222,65,129,124,253,222,4,239,15,130,244,247,65,129,124,253,222,4,239,15,
  132,244,248,185,237,252,233,244,12,248,1,252,233,244,19,248,2,77,139,93,0,
  77,139,155,233,65,139,20,131,255,249,72,15,182,220,193,232,16,72,15,182,204,
  193,232,8,65,139,116,222,4,65,139,84,198,4,129,252,254,239,15,132,244,247,
  129,252,250,239,15,130,244,248,252,233,244,252,248,1,129,252,250,239,15,132,
  244,250,129,252,250,239,15,131,244,252,252,242,65,15,42,4,222,252,242,65,
  15,88,4,198,252,242,65,15,17,134,252,252,3,0,0,139,3,72,15,182,216,72,131,
  195,4,65,252,255,36,223,248,2,129,252,250,239,15,130,244,251,255,129,252,
  250,239,15,133,244,252,252,242,65,15,42,12,198,252,242,65,15,16,4,222,252,
  242,65,15,17,134,252,252,3,0,0,139,3,72,15,182,216,72,131,195,4,65,252,255,
  36,223,248,4,255,65,139,20,222,65,252,247,60,198,65,137,134,252,252,3,0,0,
  65,199,134,0,4,0,0,237,255,65,139,20,222,65,252,247,60,198,65,137,150,252,
  252,3,0,0,65,199,134,0,4,0,0,237,255,65,139,52,222,65,3,52,198,65,137,182,
  252,252,3,0,0,65,199,134,0,4,0,0,237,255,139,3,72,15,182,216,72,131,195,4,
  65,252,255,36,223,248,5,255,252,233,244,19,255,252,242,65,15,16,4,222,252,
  242,65,15,88,4,198,252,242,65,15,17,134,252,252,3,0,0,255,139,3,72,15,182,
  216,72,131,195,4,65,252,255,36,223,248,6,185,237,252,233,244,15,255,249,72,
  15,182,220,193,232,16,72,15,182,204,193,232,8,65,139,116,222,4,65,139,84,
  198,4,129,252,254,239,15,132,244,247,129,252,250,239,15,130,244,248,252,233,
  244,252,248,1,129,252,250,239,15,132,244,250,129,252,250,239,15,131,244,252,
  252,242,65,15,42,4,222,252,242,65,15,92,4,198,252,242,65,15,17,134,252,252,
  3,0,0,139,3,72,15,182,216,72,131,195,4,65,252,255,36,223,248,2,129,252,250,
  239,15,130,244,251,255,65,139,52,222,65,43,52,198,65,137,182,252,252,3,0,
  0,65,199,134,0,4,0,0,237,255,252,242,65,15,16,4,222,252,242,65,15,92,4,198,
  252,242,65,15,17,134,252,252,3,0,0,255,249,72,15,182,220,193,232,16,72,15,
  182,204,193,232,8,65,139,116,222,4,65,139,84,198,4,129,252,254,239,15,132,
  244,247,129,252,250,239,15,130,244,248,252,233,244,252,248,1,129,252,250,
  239,15,132,244,250,129,252,250,239,15,131,244,252,252,242,65,15,42,4,222,
  252,242,65,15,89,4,198,252,242,65,15,17,134,252,252,3,0,0,139,3,72,15,182,
  216,72,131,195,4,65,252,255,36,223,248,2,129,252,250,239,15,130,244,251,255,
  65,139,52,222,65,15,175,52,198,65,137,182,252,252,3,0,0,65,199,134,0,4,0,
  0,237,255,252,242,65,15,16,4,222,252,242,65,15,89,4,198,252,242,65,15,17,
  134,252,252,3,0,0,255,249,72,15,182,220,193,232,16,72,15,182,204,193,232,
  8,65,139,116,222,4,65,139,84,198,4,129,252,254,239,15,132,244,247,129,252,
  250,239,15,130,244,248,252,233,244,252,248,1,129,252,250,239,15,132,244,250,
  129,252,250,239,15,131,244,252,252,242,65,15,42,4,222,252,242,65,15,94,4,
  198,252,242,65,15,17,134,252,252,3,0,0,139,3,72,15,182,216,72,131,195,4,65,
  252,255,36,223,248,2,129,252,250,239,15,130,244,251,255,252,242,65,15,16,
  4,222,252,242,65,15,94,4,198,252,242,65,15,17,134,252,252,3,0,0,255,249,193,
  232,8,72,15,183,216,193,232,16,77,139,93,0,77,139,155,233,252,242,65,15,42,
  4,155,65,139,84,198,4,129,252,250,239,15,130,244,247,129,252,250,239,15,133,
  244,248,252,242,73,15,42,12,198,248,1,252,233,244,16,248,2,77,139,93,0,77,
  139,155,233,72,199,198,237,72,193,230,32,65,11,52,155,73,139,20,198,72,199,
  193,237,252,233,244,17,255,249,72,15,182,220,193,232,16,77,139,93,0,77,139,
  155,233,252,242,65,15,42,12,131,65,139,116,222,4,129,252,254,239,15,130,244,
  247,129,252,254,239,15,133,244,248,252,242,73,15,42,4,222,248,1,252,233,244,
  16,248,2,73,139,52,222,77,139,93,0,77,139,155,233,72,199,194,237,72,193,226,
  32,65,11,20,131,72,199,193,237,252,233,244,17,255,249,193,232,8,72,15,183,
  216,193,232,16,77,139,93,0,77,139,155,233,252,242,65,15,16,4,219,65,139,84,
  198,4,129,252,250,239,15,130,244,247,129,252,250,239,15,133,244,248,252,242,
  73,15,42,12,198,248,1,252,233,244,16,248,2,77,139,93,0,77,139,155,233,73,
  139,52,219,73,139,20,198,72,199,193,237,252,233,244,17,255,249,193,232,8,
  72,15,183,216,193,232,16,77,139,93,0,77,139,155,233,252,242,65,15,16,12,195,
  65,139,116,222,4,129,252,254,239,15,130,244,247,129,252,254,239,15,133,244,
  248,252,242,73,15,42,4,222,248,1,252,233,244,16,248,2,77,139,93,0,77,139,
  155,233,73,139,20,195,73,139,52,222,72,199,193,237,252,233,244,17,255,249,
  193,232,8,72,15,183,216,193,232,16,73,139,52,222,73,139,20,198,72,199,193,
  237,252,233,244,17,255,249,144,255
};

#line 75 "src/interpreter/bytecode-interpreter.dasc"
//|.globals GLBNAME_
enum {
  GLBNAME_InterpArithIntL,
  GLBNAME_InterpFail,
  GLBNAME_InterpArithIntR,
  GLBNAME_InterpArithRealL,
  GLBNAME_InterpArithRealR,
  GLBNAME_InterpArithVV,
  GLBNAME_InterpPowFast,
  GLBNAME_InterpPowSlow,
  GLBNAME_DivByZero,
  GLBNAME_ModByReal,
  GLBNAME_InterpReturn,
  GLBNAME__MAX
};
#line 76 "src/interpreter/bytecode-interpreter.dasc"
//|.globalnames glbnames
static const char *const glbnames[] = {
  "InterpArithIntL",
  "InterpFail",
  "InterpArithIntR",
  "InterpArithRealL",
  "InterpArithRealR",
  "InterpArithVV",
  "InterpPowFast",
  "InterpPowSlow",
  "DivByZero",
  "ModByReal",
  "InterpReturn",
  (const char *)0
};
#line 77 "src/interpreter/bytecode-interpreter.dasc"
//|.externnames extnames
static const char *const extnames[] = {
  "InterpreterDoArithmetic",
  "Pow",
  "InterpreterPow",
  "InterpreterDivByZero",
  "InterpreterModByReal",
  (const char *)0
};
#line 78 "src/interpreter/bytecode-interpreter.dasc"

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
//||   lava_info("%s","Function FUNC address is not in 0-2GB");
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
// Sandbox pointer
//|.define SANDBOX,               r12   // callee saved

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
//|.define PC,                    rbx  // callee saved

// Hold the decoded unit
//|.define INSTR,                 eax
//|.define INSTR_OP,              al
//|.define INSTR_A8,              ah
//|.define INSTR_A16,             ax
//|.define OP,                    rbx

// Instruction's argument
//|.define ARG1_8,                bl
//|.define ARG1_16,               bx
//|.define ARG1,                  rbx

//|.define ARG2_8,                al
//|.define ARG2_16,               ax
//|.define ARG2,                  rax

//|.define ARG3_8,                cl
//|.define ARG3_16,               cx
//|.define ARG3,                  rcx

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

/* ---------------------------------------------------------------
 * dispatch table                                                |
 * --------------------------------------------------------------*/
//|.macro Dispatch
//|  mov INSTR,dword [PC]
//|  movzx OP,INSTR_OP
//|  add PC,4
//|  jmp aword [DISPATCH+OP*8]
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

/* -----------------------------------------------------------
 * Macro Interfaces for Dynasm                               |
 * ----------------------------------------------------------*/
#define Dst (&(bctx->dasm_ctx))

/* -----------------------------------------------------------
 * Interpreter Prolog                                        |
 * ----------------------------------------------------------*/
void GenerateProlog( BuildContext* bctx ) {
  (void)bctx;
}

/* -----------------------------------------------------------
 * helper functions/routines generation                      |
 * ----------------------------------------------------------*/
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

  //|->InterpArithIntL:
  //|  mov CARG1,SANDBOX
  //|  LdInt CARG2L,ARG1
  //|  fcall InterpreterDoArithmetic
  dasm_put(Dst, 0, PrototypeLayout::kIntTableOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic))) {
  dasm_put(Dst, 18);
   } else {
     lava_info("%s","Function InterpreterDoArithmetic address is not in 0-2GB");
  dasm_put(Dst, 23, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic))>>32));
   }
#line 376 "src/interpreter/bytecode-interpreter.dasc"
  //|  arith_handle_ret
  dasm_put(Dst, 31, Value::TAG_NULL);
#line 377 "src/interpreter/bytecode-interpreter.dasc"

  //|->InterpArithIntR:
  //|  mov CARG1,SANDBOX
  //|  LdInt CARG3L,ARG2
  //|  fcall InterpreterDoArithmetic
  dasm_put(Dst, 72, PrototypeLayout::kIntTableOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic))) {
  dasm_put(Dst, 18);
   } else {
     lava_info("%s","Function InterpreterDoArithmetic address is not in 0-2GB");
  dasm_put(Dst, 23, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic))>>32));
   }
#line 382 "src/interpreter/bytecode-interpreter.dasc"
  //|  arith_handle_ret
  dasm_put(Dst, 31, Value::TAG_NULL);
#line 383 "src/interpreter/bytecode-interpreter.dasc"

  //|->InterpArithRealL:
  //|  mov CARG1,SANDBOX
  //|  LdReal xmm0,ARG1
  //|  movd CARG2,xmm0
  //|  fcall InterpreterDoArithmetic
  dasm_put(Dst, 90, PrototypeLayout::kRealTableOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic))) {
  dasm_put(Dst, 18);
   } else {
     lava_info("%s","Function InterpreterDoArithmetic address is not in 0-2GB");
  dasm_put(Dst, 23, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic))>>32));
   }
#line 389 "src/interpreter/bytecode-interpreter.dasc"
  //|  arith_handle_ret
  dasm_put(Dst, 31, Value::TAG_NULL);
#line 390 "src/interpreter/bytecode-interpreter.dasc"

  //|->InterpArithRealR:
  //|  mov CARG1,SANDBOX
  //|  LdReal xmm0,ARG2
  //|  movd CARG3,xmm0
  //|  fcall InterpreterDoArithmetic
  dasm_put(Dst, 116, PrototypeLayout::kRealTableOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic))) {
  dasm_put(Dst, 18);
   } else {
     lava_info("%s","Function InterpreterDoArithmetic address is not in 0-2GB");
  dasm_put(Dst, 23, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic))>>32));
   }
#line 396 "src/interpreter/bytecode-interpreter.dasc"
  //|  arith_handle_ret
  dasm_put(Dst, 31, Value::TAG_NULL);
#line 397 "src/interpreter/bytecode-interpreter.dasc"

  //|->InterpArithVV:
  //|  mov CARG1, SANDBOX
  //|  mov CARG2, qword [STK+ARG1*8]
  //|  mov CARG3, qword [STK+ARG2*8]
  //|  fcall InterpreterDoArithmetic
  dasm_put(Dst, 142);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic))) {
  dasm_put(Dst, 18);
   } else {
     lava_info("%s","Function InterpreterDoArithmetic address is not in 0-2GB");
  dasm_put(Dst, 23, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDoArithmetic))>>32));
   }
#line 403 "src/interpreter/bytecode-interpreter.dasc"
  //|  arith_handle_ret
  dasm_put(Dst, 31, Value::TAG_NULL);
#line 404 "src/interpreter/bytecode-interpreter.dasc"

  //|->InterpPowFast:
  //|  fcall Pow 
  dasm_put(Dst, 156);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(Pow))) {
  dasm_put(Dst, 159);
   } else {
     lava_info("%s","Function Pow address is not in 0-2GB");
  dasm_put(Dst, 23, (unsigned int)(reinterpret_cast<std::uintptr_t>(Pow)), (unsigned int)((reinterpret_cast<std::uintptr_t>(Pow))>>32));
   }
#line 407 "src/interpreter/bytecode-interpreter.dasc"
  //|  movsd qword [ACC],xmm0
  //|  Dispatch
  dasm_put(Dst, 164);
#line 409 "src/interpreter/bytecode-interpreter.dasc"

  //|->InterpPowSlow:
  //|  mov CARG1, SANDBOX
  //|  fcall InterpreterPow
  dasm_put(Dst, 191);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterPow))) {
  dasm_put(Dst, 197);
   } else {
     lava_info("%s","Function InterpreterPow address is not in 0-2GB");
  dasm_put(Dst, 23, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterPow)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterPow))>>32));
   }
#line 413 "src/interpreter/bytecode-interpreter.dasc"
  //|  arith_handle_ret
  dasm_put(Dst, 31, Value::TAG_NULL);
#line 414 "src/interpreter/bytecode-interpreter.dasc"

  /* -------------------------------------------
   * Interp Arithmetic Exception               |
   * ------------------------------------------*/
  //|->DivByZero:
  //|  mov CARG1,SANDBOX
  //|  mov CARG2,[PC-4]
  //|  fcall InterpreterDivByZero
  dasm_put(Dst, 202);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterDivByZero))) {
  dasm_put(Dst, 213);
   } else {
     lava_info("%s","Function InterpreterDivByZero address is not in 0-2GB");
  dasm_put(Dst, 23, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterDivByZero)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterDivByZero))>>32));
   }
#line 422 "src/interpreter/bytecode-interpreter.dasc"
  //|  jmp ->InterpFail
  dasm_put(Dst, 218);
#line 423 "src/interpreter/bytecode-interpreter.dasc"

  //|->ModByReal:
  //|  mov CARG1,SANDBOX
  //|  mov CARG2,[PC-4]
  //|  fcall InterpreterModByReal
  dasm_put(Dst, 223);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterModByReal))) {
  dasm_put(Dst, 234);
   } else {
     lava_info("%s","Function InterpreterModByReal address is not in 0-2GB");
  dasm_put(Dst, 23, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterModByReal)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterModByReal))>>32));
   }
#line 428 "src/interpreter/bytecode-interpreter.dasc"
  //|  jmp ->InterpFail
  dasm_put(Dst, 218);
#line 429 "src/interpreter/bytecode-interpreter.dasc"


  /* -------------------------------------------
   * Interpreter exit handler                  |
   * ------------------------------------------*/
  //|->InterpFail:
  //|  xor rax,rax
  //|  epilog
  //|  ret
  dasm_put(Dst, 239);
#line 438 "src/interpreter/bytecode-interpreter.dasc"

  //|->InterpReturn:
  //|  mov rax, qword [ACC]
  //|  mov qword [SANDBOX+SandboxLayout::kRetOffset],rax
  //|  mov rax,1
  //|  epilog
  //|  ret
  dasm_put(Dst, 246, SandboxLayout::kRetOffset);
#line 445 "src/interpreter/bytecode-interpreter.dasc"
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
      dasm_put(Dst, 271,  bc, Value::FLAG_NULL);
#line 463 "src/interpreter/bytecode-interpreter.dasc"
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
      dasm_put(Dst, 285,  bc);
#line 473 "src/interpreter/bytecode-interpreter.dasc"
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
      dasm_put(Dst, 317,  bc, PrototypeLayout::kIntTableOffset, Value::FLAG_INTEGER);
#line 483 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_LOAD0:
      //|=> bc:
      //|  instr_B
      //|  StInt ARG1,0
      //|  Dispatch
      dasm_put(Dst, 363,  bc, Value::FLAG_INTEGER);
#line 490 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_LOAD1:
      //|=> bc:
      //|  instr_B
      //|  StInt ARG1,1
      //|  Dispatch
      dasm_put(Dst, 401,  bc, Value::FLAG_INTEGER);
#line 497 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_LOADN1:
      //|=> bc:
      //|  instr_B
      //|  StInt ARG1,-1
      //|  Dispatch
      dasm_put(Dst, 439,  bc, Value::FLAG_INTEGER);
#line 504 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_LOADR:
      //|=> bc:
      //|  instr_B
      //|  LdReal xmm0,ARG2
      //|  movsd qword [STK+ARG1*8],xmm0
      //|  Dispatch
      dasm_put(Dst, 481,  bc, PrototypeLayout::kRealTableOffset);
#line 512 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_LOADNULL:
      //|=> bc:
      //|  instr_F
      //|  mov dword [STK+ARG1*8+4],Value::FLAG_NULL
      //|  Dispatch
      dasm_put(Dst, 527,  bc, Value::FLAG_NULL);
#line 519 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_LOADTRUE:
      //|=> bc:
      //|  instr_F
      //|  mov dword [STK+ARG1*8+4],Value::FLAG_TRUE
      //|  Dispatch
      dasm_put(Dst, 527,  bc, Value::FLAG_TRUE);
#line 526 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_LOADFALSE:
      //|=> bc:
      //|  instr_F
      //|  mov dword [STK+ARG1*8+4],Value::FLAG_FALSE
      //|  Dispatch
      dasm_put(Dst, 527,  bc, Value::FLAG_FALSE);
#line 533 "src/interpreter/bytecode-interpreter.dasc"
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
      //|2:
      //|  arith_iv_int,add
      dasm_put(Dst, 554,  bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_ADDIV, PrototypeLayout::kIntTableOffset, PrototypeLayout::kIntTableOffset);
       if( arith_div ) {
      dasm_put(Dst, 665, Value::FLAG_INTEGER);
       } else if( arith_mod ) {
      dasm_put(Dst, 695, Value::FLAG_INTEGER);
       } else {
      dasm_put(Dst, 725, Value::FLAG_INTEGER);
       }
      dasm_put(Dst, 56);
#line 604 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_ADDRV:
      //|=>bc:
      //|  arith_xv_pre BC_ADDRV,InterpArithRealL
      //|1:
      //|  arith_rv_real,addsd
      //|2:
      //|  arith_rv_int,addsd
      dasm_put(Dst, 744, bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_ADDRV, PrototypeLayout::kRealTableOffset, PrototypeLayout::kRealTableOffset);
#line 613 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_SUBIV:
      //|=>bc:
      //|  arith_xv_pre BC_SUBIV,InterpArithIntL
      //|1:
      //|  arith_iv_real,subsd
      //|2:
      //|  arith_iv_int ,sub
      dasm_put(Dst, 894, bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_SUBIV, PrototypeLayout::kIntTableOffset, PrototypeLayout::kIntTableOffset);
       if( arith_div ) {
      dasm_put(Dst, 665, Value::FLAG_INTEGER);
       } else if( arith_mod ) {
      dasm_put(Dst, 695, Value::FLAG_INTEGER);
       } else {
      dasm_put(Dst, 1005, Value::FLAG_INTEGER);
       }
      dasm_put(Dst, 56);
#line 622 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_SUBRV:
      //|=>bc:
      //|  arith_xv_pre BC_SUBRV,InterpArithRealL
      //|1:
      //|  arith_rv_real,subsd
      //|2:
      //|  arith_rv_int ,subsd
      dasm_put(Dst, 1024, bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_SUBRV, PrototypeLayout::kRealTableOffset, PrototypeLayout::kRealTableOffset);
#line 631 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_MULIV:
      //|=>bc:
      //|  arith_xv_pre BC_MULIV,InterpArithIntL
      //|1:
      //|  arith_iv_real,mulsd
      //|2:
      //|  arith_iv_int,imul
      dasm_put(Dst, 1174, bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_MULIV, PrototypeLayout::kIntTableOffset, PrototypeLayout::kIntTableOffset);
       if( arith_div ) {
      dasm_put(Dst, 665, Value::FLAG_INTEGER);
       } else if( arith_mod ) {
      dasm_put(Dst, 695, Value::FLAG_INTEGER);
       } else {
      dasm_put(Dst, 1285, Value::FLAG_INTEGER);
       }
      dasm_put(Dst, 56);
#line 640 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_MULRV:
      //|=>bc:
      //|  arith_xv_pre BC_MULRV,InterpArithRealL
      //|1:
      //|  arith_rv_real,mulsd
      //|2:
      //|  arith_rv_int,mulsd
      dasm_put(Dst, 1306, bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_MULRV, PrototypeLayout::kRealTableOffset, PrototypeLayout::kRealTableOffset);
#line 649 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_DIVIV:
      arith_div = true;
      //|=>bc:
      //|  arith_xv_pre BC_DIVIV,InterpArithIntL
      //|1:
      //|  arith_iv_real,mulsd
      //|2:
      //|  arith_iv_int,imul
      dasm_put(Dst, 1174, bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_DIVIV, PrototypeLayout::kIntTableOffset, PrototypeLayout::kIntTableOffset);
       if( arith_div ) {
      dasm_put(Dst, 665, Value::FLAG_INTEGER);
       } else if( arith_mod ) {
      dasm_put(Dst, 695, Value::FLAG_INTEGER);
       } else {
      dasm_put(Dst, 1285, Value::FLAG_INTEGER);
       }
      dasm_put(Dst, 56);
#line 659 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_DIVRV:
      arith_div = true;
      //|=>bc:
      //|  arith_xv_pre BC_DIVRV,InterpArithRealL
      //|1:
      //|  arith_rv_real,divsd
      //|2:
      //|  arith_rv_int,divsd
      dasm_put(Dst, 1456, bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_DIVRV, PrototypeLayout::kRealTableOffset, PrototypeLayout::kRealTableOffset);
#line 669 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_MODIV:
      arith_mod = true;
      //|=>bc:
      //|  arith_xv_pre BC_MODIV,InterpArithIntL
      //|1:
      //|  jmp ->ModByReal
      //|2:
      //|  arith_iv_int,imul
      dasm_put(Dst, 1606, bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_MODIV, PrototypeLayout::kIntTableOffset);
       if( arith_div ) {
      dasm_put(Dst, 665, Value::FLAG_INTEGER);
       } else if( arith_mod ) {
      dasm_put(Dst, 695, Value::FLAG_INTEGER);
       } else {
      dasm_put(Dst, 1285, Value::FLAG_INTEGER);
       }
      dasm_put(Dst, 56);
#line 679 "src/interpreter/bytecode-interpreter.dasc"
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
      //|2:
      //|  arith_vi_int  add
      dasm_put(Dst, 1670,  bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_ADDVI, PrototypeLayout::kIntTableOffset, PrototypeLayout::kIntTableOffset);
       if( arith_div ) {
      dasm_put(Dst, 1778, Value::FLAG_INTEGER);
       } else if(arith_mod) {
      dasm_put(Dst, 1808, Value::FLAG_INTEGER);
       } else {
      dasm_put(Dst, 725, Value::FLAG_INTEGER);
       }
      dasm_put(Dst, 56);
#line 752 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_ADDVR:
      //|=> bc:
      //|  arith_vx_pre BC_ADDVR,InterpArithRealR
      //|1:
      //|  arith_vr_real addsd
      //|2:
      //|  arith_vr_int addsd
      dasm_put(Dst, 1838,  bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_ADDVR, PrototypeLayout::kRealTableOffset, PrototypeLayout::kRealTableOffset);
#line 761 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_SUBVI:
      //|=> bc:
      //|  arith_vx_pre BC_SUBVI,InterpArithIntR
      //|1:
      //|  arith_vi_real subsd
      //|2:
      //|  arith_vi_int sub
      dasm_put(Dst, 1985,  bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_SUBVI, PrototypeLayout::kIntTableOffset, PrototypeLayout::kIntTableOffset);
       if( arith_div ) {
      dasm_put(Dst, 1778, Value::FLAG_INTEGER);
       } else if(arith_mod) {
      dasm_put(Dst, 1808, Value::FLAG_INTEGER);
       } else {
      dasm_put(Dst, 1005, Value::FLAG_INTEGER);
       }
      dasm_put(Dst, 56);
#line 770 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_SUBVR:
      //|=> bc:
      //|  arith_vx_pre BC_SUBVR,InterpArithRealR
      //|1:
      //|  arith_vr_real subsd
      //|2:
      //|  arith_vr_int subsd
      dasm_put(Dst, 2093,  bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_SUBVR, PrototypeLayout::kRealTableOffset, PrototypeLayout::kRealTableOffset);
#line 779 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_MULVI:
      //|=> bc:
      //|  arith_vx_pre BC_MULVI,InterpArithIntR
      //|1:
      //|  arith_vi_real mulsd
      //|2:
      //|  arith_vi_int imul
      dasm_put(Dst, 2240,  bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_MULVI, PrototypeLayout::kIntTableOffset, PrototypeLayout::kIntTableOffset);
       if( arith_div ) {
      dasm_put(Dst, 1778, Value::FLAG_INTEGER);
       } else if(arith_mod) {
      dasm_put(Dst, 1808, Value::FLAG_INTEGER);
       } else {
      dasm_put(Dst, 1285, Value::FLAG_INTEGER);
       }
      dasm_put(Dst, 56);
#line 788 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_MULVR:
      //|=> bc:
      //|  arith_vx_pre BC_MULVR,InterpArithRealR
      //|1:
      //|  arith_vr_real mulsd
      //|2:
      //|  arith_vr_real mulsd
      dasm_put(Dst, 2348,  bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_MULVR, PrototypeLayout::kRealTableOffset, PrototypeLayout::kRealTableOffset);
#line 797 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_DIVVI:
      //|=> bc:
      dasm_put(Dst, 2495,  bc);
#line 801 "src/interpreter/bytecode-interpreter.dasc"
      arith_div = true;
      //|  arith_vx_pre BC_DIVVI,InterpArithIntR
      //|1:
      //|  arith_vi_real divsd
      //|2:
      //|  arith_vi_int sub
      dasm_put(Dst, 2497, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_DIVVI, PrototypeLayout::kIntTableOffset, PrototypeLayout::kIntTableOffset);
       if( arith_div ) {
      dasm_put(Dst, 1778, Value::FLAG_INTEGER);
       } else if(arith_mod) {
      dasm_put(Dst, 1808, Value::FLAG_INTEGER);
       } else {
      dasm_put(Dst, 1005, Value::FLAG_INTEGER);
       }
      dasm_put(Dst, 56);
#line 807 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_DIVVR:
      //|=> bc:
      //|  arith_vx_pre BC_DIVVR,InterpArithRealR
      //|1:
      //|  arith_vr_real divsd
      //|2:
      //|  arith_vr_int divsd
      dasm_put(Dst, 2604,  bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_DIVVR, PrototypeLayout::kRealTableOffset, PrototypeLayout::kRealTableOffset);
#line 816 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_MODVI:
      arith_mod = true;
      //|=> bc:
      //|  arith_vx_pre BC_MODVI,InterpArithIntR
      //|1:
      //|  jmp ->ModByReal
      //|2:
      //|  arith_vi_int imul
      dasm_put(Dst, 2751,  bc, Value::FLAG_REAL, Value::FLAG_INTEGER, BC_MODVI, PrototypeLayout::kIntTableOffset);
       if( arith_div ) {
      dasm_put(Dst, 1778, Value::FLAG_INTEGER);
       } else if(arith_mod) {
      dasm_put(Dst, 1808, Value::FLAG_INTEGER);
       } else {
      dasm_put(Dst, 1285, Value::FLAG_INTEGER);
       }
      dasm_put(Dst, 56);
#line 826 "src/interpreter/bytecode-interpreter.dasc"
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
      dasm_put(Dst, 2812,  bc, Value::FLAG_INTEGER, Value::FLAG_REAL, Value::FLAG_INTEGER, Value::FLAG_REAL, Value::FLAG_REAL);
      dasm_put(Dst, 2926, Value::FLAG_INTEGER);
       if( arith_div ) {
      dasm_put(Dst, 2977, Value::FLAG_INTEGER);
       } else if (arith_mod) {
      dasm_put(Dst, 3003, Value::FLAG_INTEGER);
       } else {
      dasm_put(Dst, 3029, Value::FLAG_INTEGER);
       }
      dasm_put(Dst, 3054);
       if( arith_mod ) {
      dasm_put(Dst, 3072);
       } else {
      dasm_put(Dst, 3077);
       }
      dasm_put(Dst, 3103, BC_ADDVV);
#line 911 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_SUBVV:
      //|=> bc:
      //|  arith_vv BC_SUBVV,sub,StIntACC,subsd,StRealACC
      dasm_put(Dst, 3127,  bc, Value::FLAG_INTEGER, Value::FLAG_REAL, Value::FLAG_INTEGER, Value::FLAG_REAL, Value::FLAG_REAL);
      dasm_put(Dst, 2926, Value::FLAG_INTEGER);
       if( arith_div ) {
      dasm_put(Dst, 2977, Value::FLAG_INTEGER);
       } else if (arith_mod) {
      dasm_put(Dst, 3003, Value::FLAG_INTEGER);
       } else {
      dasm_put(Dst, 3241, Value::FLAG_INTEGER);
       }
      dasm_put(Dst, 3054);
       if( arith_mod ) {
      dasm_put(Dst, 3072);
       } else {
      dasm_put(Dst, 3266);
       }
      dasm_put(Dst, 3103, BC_SUBVV);
#line 915 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_MULVV:
      //|=> bc:
      //|  arith_vv BC_MULVV,imul,StIntACC,mulsd,StRealACC
      dasm_put(Dst, 3292,  bc, Value::FLAG_INTEGER, Value::FLAG_REAL, Value::FLAG_INTEGER, Value::FLAG_REAL, Value::FLAG_REAL);
      dasm_put(Dst, 2926, Value::FLAG_INTEGER);
       if( arith_div ) {
      dasm_put(Dst, 2977, Value::FLAG_INTEGER);
       } else if (arith_mod) {
      dasm_put(Dst, 3003, Value::FLAG_INTEGER);
       } else {
      dasm_put(Dst, 3406, Value::FLAG_INTEGER);
       }
      dasm_put(Dst, 3054);
       if( arith_mod ) {
      dasm_put(Dst, 3072);
       } else {
      dasm_put(Dst, 3432);
       }
      dasm_put(Dst, 3103, BC_MULVV);
#line 919 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_DIVVV:
      arith_div = true;
      //|=> bc:
      //|  arith_vv BC_DIVVV,imul,StIntACC,divsd,StRealACC
      dasm_put(Dst, 3458,  bc, Value::FLAG_INTEGER, Value::FLAG_REAL, Value::FLAG_INTEGER, Value::FLAG_REAL, Value::FLAG_REAL);
      dasm_put(Dst, 2926, Value::FLAG_INTEGER);
       if( arith_div ) {
      dasm_put(Dst, 2977, Value::FLAG_INTEGER);
       } else if (arith_mod) {
      dasm_put(Dst, 3003, Value::FLAG_INTEGER);
       } else {
      dasm_put(Dst, 3406, Value::FLAG_INTEGER);
       }
      dasm_put(Dst, 3054);
       if( arith_mod ) {
      dasm_put(Dst, 3072);
       } else {
      dasm_put(Dst, 3572);
       }
      dasm_put(Dst, 3103, BC_DIVVV);
#line 924 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_MODVV:
      arith_mod = true;
      //|=> bc:
      //|  arith_vv BC_MODVV,imul,StIntACC,divsd,StRealACC
      dasm_put(Dst, 3458,  bc, Value::FLAG_INTEGER, Value::FLAG_REAL, Value::FLAG_INTEGER, Value::FLAG_REAL, Value::FLAG_REAL);
      dasm_put(Dst, 2926, Value::FLAG_INTEGER);
       if( arith_div ) {
      dasm_put(Dst, 2977, Value::FLAG_INTEGER);
       } else if (arith_mod) {
      dasm_put(Dst, 3003, Value::FLAG_INTEGER);
       } else {
      dasm_put(Dst, 3406, Value::FLAG_INTEGER);
       }
      dasm_put(Dst, 3054);
       if( arith_mod ) {
      dasm_put(Dst, 3072);
       } else {
      dasm_put(Dst, 3572);
       }
      dasm_put(Dst, 3103, BC_MODVV);
#line 929 "src/interpreter/bytecode-interpreter.dasc"
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
      dasm_put(Dst, 3598,  bc, PrototypeLayout::kIntTableOffset, Value::FLAG_REAL, Value::FLAG_INTEGER, PrototypeLayout::kIntTableOffset, Value::FLAG_INTEGER, BC_POWIV);
#line 960 "src/interpreter/bytecode-interpreter.dasc"
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
      dasm_put(Dst, 3693,  bc, PrototypeLayout::kIntTableOffset, Value::FLAG_REAL, Value::FLAG_INTEGER, PrototypeLayout::kIntTableOffset, Value::FLAG_INTEGER, BC_POWVI);
#line 974 "src/interpreter/bytecode-interpreter.dasc"
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
      dasm_put(Dst, 3785,  bc, PrototypeLayout::kRealTableOffset, Value::FLAG_REAL, Value::FLAG_INTEGER, PrototypeLayout::kRealTableOffset, BC_POWRV);
#line 988 "src/interpreter/bytecode-interpreter.dasc"
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
      dasm_put(Dst, 3872,  bc, PrototypeLayout::kRealTableOffset, Value::FLAG_REAL, Value::FLAG_INTEGER, PrototypeLayout::kRealTableOffset, BC_POWVR);
#line 1002 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_POWVV:
      //|=> bc:
      //|  instr_C
      //|  mov CARG2,qword [STK+ARG1*8]
      //|  mov CARG3,qword [STK+ARG2*8]
      //|  mov CARG4,BC_POWVV
      //|  jmp ->InterpPowSlow
      dasm_put(Dst, 3959,  bc, BC_POWVV);
#line 1011 "src/interpreter/bytecode-interpreter.dasc"
      break;
    default:
      //|=> bc:
      //|  nop
      dasm_put(Dst, 3987,  bc);
#line 1015 "src/interpreter/bytecode-interpreter.dasc"
      break;
  }
}

// Help Dasm to resolve external address via Index idx
int ResolveExternAddress( void** ctx , unsigned char* addr ,
                                       int idx,
                                       int type ) {
  (void)ctx;
  void* ptr;
  if(strcmp(extnames[idx],"Pow")==0) {
    ptr = reinterpret_cast<void*>(&Pow);
  } else if(strcmp(extnames[idx],"InterpreterModByReal")==0) {
    ptr = reinterpret_cast<void*>(&InterpreterModByReal);
  } else if(strcmp(extnames[idx],"InterpreterDivByZero")==0) {
    ptr = reinterpret_cast<void*>(&InterpreterDivByZero);
  } else if(strcmp(extnames[idx],"InterpreterPow")==0) {
    ptr = reinterpret_cast<void*>(&InterpreterPow);
  } else if(strcmp(extnames[idx],"InterpreterDoArithmetic")==0) {
    ptr = reinterpret_cast<void*>(&InterpreterDoArithmetic);
  } else {
    lava_unreach("WHY!");
  }

  lava_verify(CheckAddress(reinterpret_cast<std::uintptr_t>(ptr)));

  if(type) {
    return (int)((reinterpret_cast<unsigned char*>(ptr) - (addr+4)));
  } else {
    return (int)(reinterpret_cast<std::uintptr_t>(ptr));
  }
}

void Disassemble( Interpreter* interp ) {
  DumpWriter writer;
  std::size_t length = interp->code_size;
  void* buffer = reinterpret_cast<void*>(interp->entry);

  ZydisDecoder decoder;
  ZydisDecoderInit(
      &decoder,
      ZYDIS_MACHINE_MODE_LONG_64,
      ZYDIS_ADDRESS_WIDTH_64);

  ZydisFormatter formatter;
  ZydisFormatterInit(&formatter,ZYDIS_FORMATTER_STYLE_INTEL);

  std::uint64_t pc = reinterpret_cast<std::uint64_t>(buffer);
  std::uint8_t* rp = static_cast<std::uint8_t*>(buffer);
  std::size_t size = length;

  ZydisDecodedInstruction instr;
  while(ZYDIS_SUCCESS(
        ZydisDecoderDecodeBuffer(&decoder,rp,size,pc,&instr))) {
    char buffer[256];
    ZydisFormatterFormatInstruction(
        &formatter,&instr,buffer,sizeof(buffer));

    for( std::size_t i = 0 ; i < SIZE_OF_BYTECODE ; ++i ) {
      void* p = reinterpret_cast<void*>(pc);
      if(p == interp->dispatch_interp[i]) {
        writer.WriteL("===> Bytecode : %s",GetBytecodeName(
              static_cast<Bytecode>(i)));
        break;
      }
    }

    writer.WriteL("%016" PRIX64 " " "%s",pc,buffer);
    rp += instr.length;
    size -= instr.length;
    pc += instr.length;
  }

}

// We directly allocate a chunk of memory and *let it leak*
void BuildInterpreter( Interpreter* interp ) {
  BuildContext bctx;
  // initialize dasm_State object
  dasm_init(&(bctx.dasm_ctx),1);

  // setup the freaking global
  void* glb_arr[GLBNAME__MAX];
  dasm_setupglobal(&(bctx.dasm_ctx),glb_arr,GLBNAME__MAX);

  // setup the dasm
  dasm_setup(&(bctx.dasm_ctx),actions);

  // initialize the tag value needed , at least for each BC we need one
  dasm_growpc(&(bctx.dasm_ctx), SIZE_OF_BYTECODE);

  // build the prolog
  GenerateProlog(&bctx);

  // build the helper
  GenerateHelper(&bctx);

  // generate all bytecode's routine
  for( int i = static_cast<int>(BC_ADDIV) ; i < SIZE_OF_BYTECODE ; ++i ) {
    GenerateOneBytecode(&bctx,static_cast<Bytecode>(i));
  }

  std::size_t code_size;
  lava_verify(dasm_link(&(bctx.dasm_ctx),&code_size) ==0);

  // generate a buffer and set the proper protection field for that piece of
  // memory to make our code *work*
  std::size_t new_size;
  void* buffer = OS::CreateCodePage(code_size,&new_size);
  lava_verify(buffer);

  // encode the assembly code into the buffer
  dasm_encode(&(bctx.dasm_ctx),buffer);

  // get all pc labels for entry of bytecode routine
  for( int i = static_cast<int>(BC_ADDIV) ; i < SIZE_OF_BYTECODE ; ++i ) {
    int off = dasm_getpclabel(&(bctx.dasm_ctx),static_cast<Bytecode>(i));
    interp->dispatch_interp[i] =
      reinterpret_cast<void*>(static_cast<char*>(buffer) + off);
  }

  interp->entry = reinterpret_cast<Interpret>(buffer);
  interp->buffer_size = new_size;
  interp->code_size   = code_size;

  // now we try a disassemble to see whether our shit actually looks Okay
  {
    DumpWriter writer(NULL);
    Disassemble(interp);
  }

  dasm_free(&(bctx.dasm_ctx));
}

} // namespace

void GenerateInterpreter( Interpreter* interp ) {
  BuildInterpreter(interp);
}

} // namespace lavascript
} // namespace interpreter
