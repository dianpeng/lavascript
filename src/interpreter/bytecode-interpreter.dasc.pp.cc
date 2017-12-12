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
namespace {

// Used in dynasm library
int ResolveExternalAddress( void**,unsigned char*,int,int );

// Workaround for ODR
#include "dep/dynasm/dasm_proto.h"

#define DASM_EXTERN_FUNC(a,b,c,d) ResolveExternalAddress((void**)a,b,c,d)
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

void ReportError( Runtime* sandbox , const char* fmt , ... ) {
  // TODO:: Add stack unwind and other stuff for report error
  va_list vl;
  va_start(vl,fmt);
  FormatV(sandbox->error,fmt,vl);
}

// ------------------------------------------------------------------
// Prototype for the main interpreter function
//
// @ARG1: runtime
// @ARG2: Prototype** of the function
// @ARG3: start of the stack
// @ARG4: start of the code buffer for the *Prototype*
// @ARG5: start of the dispatch table
typedef bool (*Main)(Runtime*,Closure**,Prototype**,void*,void*,void*);

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
inline std::uint32_t CurrentBytecode( Runtime* sandbox ) {
  /**
   * We do have instruction occupy 2 slots , *BUT* we don't need to consider
   * this because that byte is added on demand.
   *
   * When we reach to C++ function, our PC should still points to the added
   * byte for that instruction if our previous instruction is a 2 byte.
   *
   * So we only need to substract one of the *current* PC
   */
  return sandbox->cur_pc[-1];
}

inline Bytecode CurrentOpcode( Runtime* sandbox ) {
  std::uint32_t pbc = CurrentBytecode(sandbox);
  Bytecode bc = static_cast<Bytecode>(pbc&0xff);
  lava_error("BC:%s",GetBytecodeName(bc));
  return bc;
}

inline void BranchTo( Runtime* sandbox , std::uint32_t offset ) {
  Handle<Closure> cls(sandbox->cur_cls);
  const std::uint32_t* pc_start = cls->code_buffer();
  sandbox->cur_pc = pc_start + offset;
}

// Helper to skip the current offset arg. There're some BC has 2 dword encoding,
// which when calling from interpreter to C++ function, the PC is left to be the
// value pointed to the second dword of that BC. We need to bump cur_pc one dword
// forward if the jump is not taken
inline void BumpPC( Runtime* sandbox ) {
  sandbox->cur_pc++;
}

// --------------------------------------------------------------------------
// Arithmetic Helper
// --------------------------------------------------------------------------
void InterpreterModByZero( Runtime* sandbox ) {
  ReportError(sandbox,"\"%\"'s rhs value is 0");
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterModByZero)

bool InterpreterArithmetic( Runtime* sandbox , const Value& left ,
                                               const Value& right,
                                               Value* output ) {
  lava_error("Into arithmetic %s,%s",left.type_name(),right.type_name());
  if(left.IsExtension() || right.IsExtension()) {
    Handle<Extension> ext(left.IsExtension() ? left.GetExtension() :
        right.GetExtension());
    switch(CurrentOpcode(sandbox)) {
      case BC_ADDRV: case BC_ADDVR: case BC_ADDVV:
        return ext->Add(left,right,output,sandbox->error);
      case BC_SUBRV: case BC_SUBVR: case BC_SUBVV:
        return ext->Sub(left,right,output,sandbox->error);
      case BC_MULRV: case BC_MULVR: case BC_MULVV:
        return ext->Mul(left,right,output,sandbox->error);
      case BC_DIVRV: case BC_DIVVR: case BC_DIVVV:
        return ext->Div(left,right,output,sandbox->error);
      default:
        return ext->Mod(left,right,output,sandbox->error);
    }
  } else if(left.IsReal() && right.IsReal()) {
#define _DO(OP) output->SetReal(left.GetReal() OP right.GetReal()); break

    switch(CurrentOpcode(sandbox)) {
      case BC_ADDRV: case BC_ADDVR: case BC_ADDVV: _DO(+);
      case BC_SUBRV: case BC_SUBVR: case BC_SUBVV: _DO(-);
      case BC_MULRV: case BC_MULVR: case BC_MULVV: _DO(*);
      case BC_DIVRV: case BC_DIVVR: case BC_DIVVV: _DO(/);
      default:
        {
          std::int32_t l = static_cast<std::int32_t>(left.GetReal());
          std::int32_t r = static_cast<std::int32_t>(right.GetReal());
          if(r == 0) {
            InterpreterModByZero(sandbox);
            return false;
          }
          output->SetReal( static_cast<double>( l % r ) );
        }
        break;
    }

#undef _DO // _DO
  } else {
    ReportError(sandbox,"arithmetic operator cannot work between type %s and %s",
        left.type_name(),right.type_name());
    return false;
  }
  return true;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterArithmetic)

bool InterpreterPow( Runtime* sandbox , const Value& left,
                                        const Value& right ,
                                        Value* output ) {

  if(left.IsExtension())
    return left.GetExtension()->Pow(left,right,output,sandbox->error);
  else if(right.IsExtension())
    return right.GetExtension()->Pow(left,right,output,sandbox->error);
  else if(left.IsReal() && right.IsReal()) {
    output->SetReal( std::pow(left.GetReal(),right.GetReal()) );
  } else {
    ReportError(sandbox,"\"%\" operator cannot work between type %s and %s",
        left.type_name(),right.type_name());
    return false;
  }
  return true;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterPow)


// ---------------------------------------------------------------------------
// Comparison Helper
//
// I want to force a jmp table for the following switch, but not sure how to
// do it in portable C++
//
// ---------------------------------------------------------------------------
bool InterpreterCompare( Runtime* sandbox , const Value& left ,
                                            const Value& right,
                                            Value* output ) {
  if(left.IsString() && right.IsString()) {
#define _DO(OP) output->SetBoolean(*left.GetString() OP *right.GetString()); break

    switch(CurrentOpcode(sandbox)) {
      case BC_LTRV: case BC_LTVR: case BC_LTVV: _DO(<);
      case BC_LERV: case BC_LEVR: case BC_LEVV: _DO(<=);
      case BC_GTRV: case BC_GTVR: case BC_GTVV: _DO(>);
      case BC_GERV: case BC_GEVR: case BC_GEVV: _DO(>=);
      case BC_EQSV: case BC_EQVS: case BC_EQVV: _DO(==);
      default: _DO(!=);
    }

#undef _DO // _DO
  } else if(left.IsExtension() || right.IsExtension()) {
#define _DO(OP) return left.IsExtension() ? \
                       left.GetExtension()->OP(left,right,output,sandbox->error) : \
                       right.GetExtension()->OP(left,right,output,sandbox->error);
    switch(CurrentOpcode(sandbox)) {
      case BC_LTRV: case BC_LTVR: case BC_LTVV: _DO(Lt);
      case BC_LERV: case BC_LEVR: case BC_LEVV: _DO(Le);
      case BC_GTRV: case BC_GTVR: case BC_GTVV: _DO(Gt);
      case BC_GERV: case BC_GEVR: case BC_GEVV: _DO(Ge);
      case BC_EQRV: case BC_EQVR: case BC_EQSV: case BC_EQVS: case BC_EQVV: _DO(Eq);
      default: _DO(Ne);
    }
#undef _DO // _DO
  } else if(left.IsReal() && right.IsReal()) {
#define _DO(OP) output->SetBoolean(left.GetReal() OP right.GetReal()); break

    switch(CurrentOpcode(sandbox)) {
      case BC_LTRV: case BC_LTVR: case BC_LTVV: _DO(<);
      case BC_LERV: case BC_LEVR: case BC_LEVV: _DO(<=);
      case BC_GTRV: case BC_GTVR: case BC_GTVV: _DO(>);
      case BC_GERV: case BC_GEVR: case BC_GEVV: _DO(>=);
      case BC_EQRV: case BC_EQVR: case BC_EQVV: _DO(==);
      default: _DO(!=);
    }

#undef _DO // _DO
  } else {
    ReportError(sandbox,"comparison operator doesn't work between type %s and %s",
        left.type_name(),right.type_name());
    return false;
  }
  return true;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterCompare)

// ----------------------------------------------------------------------------
// Unary Helper
// ----------------------------------------------------------------------------
void InterpreterNegateFail( Runtime* sandbox , const Value& operand ) {
  ReportError(sandbox,"unary operator \"-\" can only work with real type, not type %s",
      operand.type_name());
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterNegateFail)

// ----------------------------------------------------------------------------
// Literal Loader Helper
// ----------------------------------------------------------------------------
void InterpreterLoadList0( Runtime* sandbox , Value* output ) {
  Handle<List> list(List::New(sandbox->context->gc()));
  output->SetList(list);
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterLoadList0)

void InterpreterLoadList1( Runtime* sandbox , Value* output , const Value& e1 ) {
  Handle<List> list(List::New(sandbox->context->gc(),2));
  list->Push(sandbox->context->gc(),e1);
  output->SetList(list);
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterLoadList1)

void InterpreterLoadList2( Runtime* sandbox , Value* output , const Value& e1,
                                                              const Value& e2 ) {
  Handle<List> list(List::New(sandbox->context->gc(),2));
  list->Push(sandbox->context->gc(),e1);
  list->Push(sandbox->context->gc(),e2);
  output->SetList(list);
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterLoadList2)

void InterpreterNewList( Runtime* sandbox , Value* output , std::uint32_t narg ) {
  Handle<List> list(List::New(sandbox->context->gc(),narg));
  output->SetList(list);
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterNewList)

void InterpreterAddList( Runtime* sandbox , Value* output , std::uint8_t base,
                                                            std::uint32_t narg ) {

  lava_debug(NORMAL,lava_verify(output->IsList()););
  Handle<List> l(output->GetList());

  for( std::uint32_t i = 0 ; i < narg ; ++i ) {
    l->Push(sandbox->context->gc(),sandbox->cur_stk[base+i]);
  }
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterAddList)

void InterpreterLoadObj0( Runtime* sandbox , Value* output ) {
  output->SetObject( Object::New(sandbox->context->gc()) );
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterLoadObj0)

bool InterpreterLoadObj1( Runtime* sandbox , Value* output ,
                                             const Value& key,
                                             const Value& val ) {
  Handle<Object> obj(Object::New(sandbox->context->gc(),2));
  if(key.IsString()) {
    obj->Put(sandbox->context->gc(),key.GetString(),val);
    output->SetObject(obj);
    return true;
  } else {
    ReportError(sandbox,"object's key must be string type, but get type %s",
        key.type_name());
    return false;
  }
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterLoadObj1)

void InterpreterNewObj( Runtime* sandbox , Value* output , std::uint32_t narg ) {
  Handle<Object> obj(Object::New(sandbox->context->gc(),narg));
  output->SetObject(obj);
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterNewObj)

bool InterpreterAddObj( Runtime* sandbox , Value* output , const Value& key,
                                                           const Value& val ) {
  lava_debug(NORMAL,lava_verify(output->IsObject()););
  if(key.IsString()) {
    output->GetObject()->Put(sandbox->context->gc(),key.GetString(),val);
    return true;
  } else {
    ReportError(sandbox,"object's key must be string type, but get type %s",
        key.type_name());
    return false;
  }
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterAddObj)

LAVA_ALWAYS_INLINE
Handle<Closure> NewClosure( Runtime* sandbox , std::uint32_t ref ,
                                               Handle<String>* name ) {
  Script* scp = *(sandbox->script);
  const Script::FunctionTableEntry& entry = scp->GetFunction(ref);
  Handle<Closure> cls(Closure::New(sandbox->context->gc(),entry.prototype));

  // Initialize the UpValue array
  {
    Value* stk = sandbox->cur_stk;
    Closure* cls = *(sandbox->cur_cls);  // We will *not* Have GC
    Value* uv_arr = cls->upvalue();
    const std::uint32_t len = entry.prototype->upvalue_size();
    for( std::uint32_t i = 0 ; i < len ; ++i ) {
      UpValueState st;
      std::uint8_t idx = entry.prototype->GetUpValue(i,&st);
      if(st == UV_EMBED) {
        uv_arr[i] = stk[idx];
      } else {
        uv_arr[i] = cls->GetUpValue(idx);
      }
    }
  }

  if(name) *name = entry.name;
  return cls;
}

void InterpreterLoadCls( Runtime* sandbox , std::uint32_t ref, Value* dest ) {
  dest->SetClosure(NewClosure(sandbox,ref,NULL));
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterLoadCls)

bool InterpreterInitCls( Runtime* sandbox , std::uint32_t ref ) {
  Handle<String> name;
  // get the function's closure and its name
  Handle<Closure> cls(NewClosure(sandbox,ref,&name));

  // set it up into the *global* table
  Handle<Object> glb(sandbox->global);

  lava_debug(NORMAL,lava_verify(name););

  // load it up into the *global table
  if(!glb->Set(sandbox->context->gc(),name,Value(cls))) {
    ReportError(sandbox,"global closure %s has already been defined!",
        name->ToStdString().c_str());
    return false;
  }
  return true;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterInitCls)

// ----------------------------------------------------------------------------
// Property Get/Set
// ----------------------------------------------------------------------------
void InterpreterPropNeedObject( Runtime* sandbox, const Value& obj ) {
  ReportError(sandbox,"type %s cannot work with operator \".\" or \"[]\"",obj.type_name());
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterPropNeedObject)

bool InterpreterPropGet( Runtime* sandbox , const Value& obj , String** key ,
                                                               Value* output ) {
  Handle<String> k(key);
  if(obj.IsObject()) {
    if(!obj.GetObject()->Get(k,output)) {
      ReportError(sandbox,"key %s not found in object",k->ToStdString().c_str());
      return false;
    }

  } else if(obj.IsExtension()) {
    return obj.GetExtension()->GetProp(obj,Value(k),output,sandbox->error);
  } else {
    ReportError(sandbox,"operator \".\" or \"[]\" cannot work between type %s and string",
        obj.type_name());
    return false;
  }
  return true;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterPropGet)

bool InterpreterPropGetSSO( Runtime* sandbox , const Value& obj , std::uint32_t index ,
                                                                  Value* output ) {
  if(obj.IsExtension()) {
    Value key(Handle<String>(sandbox->cur_proto()->GetSSO(index)->str));
    return obj.GetExtension()->GetProp(obj,key,output,sandbox->error);
  } else if(obj.IsObject()) {
    Handle<String> key(sandbox->cur_proto()->GetSSO(index)->str);
    if(!obj.GetObject()->Get(key,output)) {
      ReportError(sandbox,"key %s not found in object",key->ToStdString().c_str());
      return false;
    }
  } else {
    ReportError(sandbox,"operator \".\" or \"[]\" cannot work between type %s and string",
        obj.type_name());
    return false;
  }

  return true;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterPropGetSSO)


void InterpreterPropGetSSONotFound( Runtime* sandbox , SSO* key ) {
  ReportError(sandbox,"key %s not found in object",key->ToStdString().c_str());
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterPropGetSSONotFound)

bool InterpreterPropSet( Runtime* sandbox , const Value& obj , String** key ,
                                                               const Value& value ) {
  Handle<String> k(key);
  if(obj.IsObject()) {
    if(!obj.GetObject()->Update(sandbox->context->gc(),k,value)) {
      ReportError(sandbox,"key %s not found in object, cannot set",k->ToStdString().c_str());
      return false;
    }
  } else if(obj.IsExtension()) {
    return obj.GetExtension()->SetProp(obj,Value(k),value,sandbox->error);
  } else {
    ReportError(sandbox,"operator \".\" or \"[]\" cannot work between type %s and string",
        obj.type_name());
    return false;
  }
  return true;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterPropSet)

bool InterpreterPropSetSSO( Runtime* sandbox , Value obj , std::uint32_t index,
                                                           const Value& value ) {
  if(obj.IsExtension()) {
    Value key(Handle<String>(sandbox->cur_proto()->GetSSO(index)->str));
    return obj.GetExtension()->SetProp(obj,key,value,sandbox->error);
  } else if(obj.IsObject()) {
    Handle<String> key(sandbox->cur_proto()->GetSSO(index)->str);
    if(!obj.GetObject()->Update(sandbox->context->gc(),key,value)) {
      ReportError(sandbox,"key %s not found in object, cannot set",key->ToStdString().c_str());
      return false;
    }
  } else {
    ReportError(sandbox,"operator \".\" or \"[]\" cannot work between type %s and string",
        obj.type_name());
    return false;
  }
  return true;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterPropSetSSO)


void InterpreterPropSetSSONotFound( Runtime* sandbox , SSO* key ) {
  ReportError(sandbox,"key %s not found in object, cannot set",key->ToStdString().c_str());
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterPropSetSSONotFound)


void InterpreterIdxOutOfBound( Runtime* sandbox , const Value& obj ,
                                                  std::int32_t size ) {
  lava_debug(NORMAL,lava_verify(obj.IsList()););
  ReportError(sandbox,"index %d out of bound of list with size %d",size,
      obj.GetList()->size());
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterIdxOutOfBound)

bool InterpreterIdxGet( Runtime* sandbox , const Value& obj , const Value& key ,
                                                              Value* output ) {
  if(obj.IsExtension()) {
    return obj.GetExtension()->GetProp(obj,key,output,sandbox->error);
  } else if(obj.IsList() && key.IsReal()) {
    std::int32_t idx;
    Handle<List> l(obj.GetList());
    if(TryCastReal(key.GetReal(),&idx) && (idx >= 0 && idx < static_cast<std::int32_t>(l->size()))) {
      *output = l->Index(idx);
    } else {
      ReportError(sandbox,"index %f out of bound of list with size %d",key.GetReal(),l->size());
      return false;
    }
  } else if(obj.IsObject() && key.IsString()) {
    Handle<Object> o(obj.GetObject());
    if(!o->Get(key.GetString(),output)) {
      ReportError(sandbox,"key %s not found in object",key.GetString()->ToStdString().c_str());
      return false;
    }
  } else {
    ReportError(sandbox,"type %s cannot work with type %s by operator \".\" or \"[]\"",
        obj.type_name(),key.type_name());
    return false;
  }
  return true;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterIdxGet)

bool InterpreterIdxSet( Runtime* sandbox , const Value& obj , const Value& key ,
                                                              const Value& val ) {
  if(obj.IsExtension()) {
    return obj.GetExtension()->SetProp(obj,key,val,sandbox->error);
  } else if(obj.IsList() && key.IsReal()) {
    std::int32_t idx;
    Handle<List> l(obj.GetList());
    if(TryCastReal(key.GetReal(),&idx) && (idx >= 0 && idx < static_cast<std::int32_t>(l->size()))) {
      l->Index(idx) = val;
    } else {
      ReportError(sandbox,"index %f out of bound of list with size %d",key.GetReal(),l->size());
      return false;
    }
  } else if(obj.IsObject() && key.IsString()) {
    Handle<Object> o(obj.GetObject());
    if(!o->Update(sandbox->context->gc(),key.GetString(),val)) {
      ReportError(sandbox,"key %s not found in object, cannot set",
          key.GetString()->ToStdString().c_str());
      return false;
    }
  } else {
    ReportError(sandbox,"type %s cannot work with type %s by operator \".\" or \"[]\"",
        obj.type_name(),key.type_name());
    return false;
  }
  return true;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterIdxSet)

// ----------------------------------------------------------------------------
// Global
// ----------------------------------------------------------------------------
void InterpreterGGetNotFoundSSO( Runtime* sandbox , SSO* key ) {
  ReportError(sandbox,"global %s not found",key->ToStdString().c_str());
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterGGetNotFoundSSO)

bool InterpreterGGet( Runtime* sandbox , Value* output , String** key ) {
  Handle<Object> global(sandbox->global);
  Handle<String> k(key);
  if(!global->Get(k,output)) {
    ReportError(sandbox,"global %s not found",k->ToStdString().c_str());
    return false;
  }
  return true;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterGGet)

void InterpreterGSetNotFoundSSO( Runtime* sandbox , SSO* key ) {
  ReportError(sandbox,"global %s not found, cannot set",key->ToStdString().c_str());
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterGSetNotFoundSSO)

bool InterpreterGSet( Runtime* sandbox , String** key , const Value& value ) {
  Handle<Object> global(sandbox->global);
  Handle<String> k(key);
  if(!global->Update(sandbox->context->gc(),k,value)) {
    ReportError(sandbox,"global %s not found, cannot set",k->ToStdString().c_str());
    return false;
  }
  return true;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterGSet)

// ----------------------------------------------------------------------------
// Loop
// ----------------------------------------------------------------------------
bool InterpreterForEnd1( Runtime* sandbox , const Value& lhs , const Value& rhs ,
                                                               std::uint32_t offset ) {
  if(lhs.IsExtension() || rhs.IsExtension()) {
    Handle<Extension> ext( lhs.IsExtension() ? lhs.GetExtension() : rhs.GetExtension() );
    Value result;
    if(!ext->Lt(lhs,rhs,&result,sandbox->error))
      return false;
    lava_debug(NORMAL,lava_verify(result.IsBoolean()););
    if(result.IsFalse()) {
      BranchTo(sandbox,offset);
    }
  } else if(lhs.IsString() && rhs.IsString()) {
    if(!(*lhs.GetString() < *rhs.GetString())) {
      BranchTo(sandbox,offset);
    }
  } else if(lhs.IsReal() && rhs.IsReal()) {
    if(!(lhs.GetReal() < rhs.GetReal())) {
      BranchTo(sandbox,offset);
    } else {
      BumpPC(sandbox);
    }
  } else {
    ReportError(sandbox,"type %s and %s cannot be used for range for loop,"
                        "no \"<\" operation allowed",
                        lhs.type_name(),rhs.type_name());
    return false;

  }
  return true;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterForEnd1)

bool InterpreterForEnd2( Runtime* sandbox , const Value& lhs , const Value& rhs ,
                                                               const Value& step ,
                                                               std::uint32_t offset ) {
  Value new_induction;

  // 1. do the addtion part
  if(lhs.IsExtension() || step.IsExtension()) {
    Handle<Extension> ext(lhs.IsExtension() ? lhs.GetExtension() : step.GetExtension());
    if(!ext->Add(lhs,step,&new_induction,sandbox->error))
      return false;
  } else if(lhs.IsReal() && step.IsReal()) {
    new_induction.SetReal( lhs.GetReal() + step.GetReal() );
  } else {
    ReportError(sandbox,"type %s and %s cannot be used for range for loop,"
                        "no \"+\" operation allowed",
                        lhs.type_name(),step.type_name());
    return false;
  }

  // 2. do the comparison part
  return InterpreterForEnd1(sandbox,new_induction,rhs,offset);
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterForEnd2)

bool InterpreterFEStart( Runtime* sandbox , Value* expr , std::uint32_t offset ) {
  Handle<Iterator> itr;
  if(expr->IsList()) {
    itr = expr->GetList()->NewIterator(sandbox->context->gc(),expr->GetList());
  } else if(expr->IsObject()) {
    itr = expr->GetObject()->NewIterator(sandbox->context->gc(),expr->GetObject());
  } else if(expr->IsExtension()) {
    itr = expr->GetExtension()->NewIterator(sandbox->context->gc(),expr->GetExtension(),
                                                                   sandbox->error);
    if(!itr) return false; // Extension doesn't support iterator
  } else {
    ReportError(sandbox,"type %s doesn't support iterator",expr->type_name());
    return false;
  }

  expr->SetIterator(itr);
  if(!itr->HasNext()) {
    BranchTo(sandbox,offset);
  }
  return true;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterFEStart)

void InterpreterFEEnd( Runtime* sandbox , const Value& expr , std::uint32_t offset ) {
  Handle<Iterator> itr(expr.GetIterator());
  if(itr->Move()) {
    BranchTo(sandbox,offset); // Jump back if we have anything in iterator
  }
  // no need to bump pc since FEEnd doesn't use extra byte
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterFEEnd)

void InterpreterIDref( Runtime* sandbox , Value* key , Value* val , const Value& expr ) {
  Handle<Iterator> itr(expr.GetIterator());
  itr->Deref(key,val);
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterIDref)

/* ---------------------------------------------------------------------
 * Function call
 * --------------------------------------------------------------------*/
bool InterpreterCall( Runtime* sandbox , const Value& expr , std::uint8_t base ,
                                                             std::uint8_t narg ,
                                                             bool tcall ) {
  (void)sandbox;
  (void)expr;
  (void)base;
  (void)narg;
  (void)tcall;
  return false;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterCall)

void InterpreterArgumentMismatch( Runtime* sandbox , const Value& object,
                                                     std::uint8_t arg ) {
  (void)sandbox;
  (void)object;
  (void)arg;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterArgumentMismatch)

bool ResizeStack( Runtime* sandbox ) {
  (void)sandbox;
  return false;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(ResizeStack)

void InterpreterCallNeedObject( Runtime* sandbox , Value object ) {
  (void)sandbox;
  (void)object;
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(InterpreterCallNeedObject)


/* ---------------------------------------------------------------------
 *
 * Implementation of AssemblyInterpreter
 *
 * --------------------------------------------------------------------*/
//|.arch x64
#if DASM_VERSION != 10300
#error "Version mismatch between DynASM and included encoding engine"
#endif
#line 780 "src/interpreter/bytecode-interpreter.dasc"
//|.actionlist actions
static const unsigned char actions[7027] = {
  254,1,248,10,237,237,255,248,11,248,12,237,237,255,248,13,0,0,0,0,0,0,252,
  255,252,255,255,248,14,237,255,248,15,237,255,248,16,0,0,0,0,237,255,248,
  17,0,0,0,0,0,0,0,0,255,254,0,249,248,18,255,72,131,252,236,72,76,137,100,
  36,40,76,137,108,36,32,76,137,116,36,24,76,137,124,36,16,72,137,108,36,8,
  72,137,92,36,48,255,73,137,252,252,73,137,213,73,137,206,76,137,197,77,137,
  207,255,72,137,44,36,255,184,237,72,193,224,48,73,137,6,73,137,118,8,73,131,
  198,16,255,73,137,180,253,36,233,77,137,180,253,36,233,255,139,69,0,72,15,
  182,200,72,137,207,255,232,251,1,0,255,72,184,237,237,252,255,208,255,139,
  69,0,72,15,182,200,72,131,197,4,193,232,8,65,252,255,36,207,255,249,248,19,
  49,192,76,139,100,36,40,76,139,108,36,32,76,139,116,36,24,76,139,124,36,16,
  72,139,108,36,8,72,139,92,36,48,72,131,196,72,195,255,249,248,20,72,137,208,
  73,137,132,253,36,233,72,199,192,1,0,0,0,255,249,248,21,73,137,172,253,36,
  233,76,137,231,255,77,139,93,0,77,139,148,253,195,233,72,141,116,36,56,76,
  137,84,36,56,255,73,141,20,206,73,141,12,222,255,232,251,1,1,255,133,192,
  15,132,244,19,139,69,0,72,15,182,200,72,137,207,255,249,248,22,73,137,172,
  253,36,233,76,137,231,73,141,52,198,255,77,139,93,0,77,139,148,253,203,233,
  72,141,84,36,56,76,137,84,36,56,255,249,248,23,73,137,172,253,36,233,76,137,
  231,73,141,52,198,73,141,20,206,73,141,12,222,255,249,248,24,73,137,172,253,
  36,233,76,137,231,255,232,251,1,2,255,249,248,25,73,137,172,253,36,233,76,
  137,231,73,141,52,198,255,77,139,93,0,77,139,148,253,203,233,76,137,84,36,
  56,72,141,84,36,56,255,249,248,26,73,137,172,253,36,233,72,15,182,216,193,
  232,8,15,182,204,37,252,255,0,0,0,76,137,231,73,141,52,198,73,141,20,206,
  73,141,12,222,255,249,248,27,73,137,172,253,36,233,76,137,231,255,232,251,
  1,3,255,252,233,244,19,255,249,248,28,73,137,172,253,36,233,76,137,231,255,
  73,141,20,198,73,141,12,222,255,232,251,1,4,255,249,248,29,73,137,172,253,
  36,233,76,137,231,73,141,52,198,255,249,248,30,73,137,172,253,36,233,76,137,
  231,73,141,52,198,255,77,139,93,0,77,139,155,233,77,139,20,203,76,11,21,244,
  10,72,141,84,36,56,76,137,84,36,56,255,249,248,31,73,137,172,253,36,233,76,
  137,231,255,77,139,93,0,77,139,155,233,77,139,20,195,76,11,21,244,10,72,141,
  116,36,56,76,137,84,36,56,255,249,248,32,73,137,172,253,36,233,76,137,231,
  73,141,52,198,73,141,20,206,73,141,12,222,255,249,248,33,73,137,172,253,36,
  233,76,137,231,73,141,52,198,255,232,251,1,5,255,249,248,34,73,137,172,253,
  36,233,76,137,231,73,139,52,198,252,242,15,42,193,102,72,15,126,194,73,141,
  12,222,255,232,251,1,6,255,249,248,35,73,137,172,253,36,233,76,137,231,73,
  139,52,222,252,242,15,42,192,102,72,15,126,194,73,139,12,206,255,232,251,
  1,7,255,249,248,36,73,137,172,253,36,233,76,137,231,73,139,52,198,137,202,
  255,232,251,1,8,255,249,248,37,73,137,172,253,36,233,76,137,231,73,139,52,
  222,137,194,255,249,248,38,73,137,172,253,36,233,76,137,231,73,139,52,198,
  137,194,137,201,69,49,192,255,232,251,1,9,255,249,248,39,73,137,172,253,36,
  233,76,137,231,73,139,52,198,137,194,137,201,65,184,1,0,0,0,255,249,248,40,
  73,137,172,253,36,233,76,137,231,73,139,52,222,255,232,251,1,10,255,249,248,
  41,73,137,172,253,36,233,76,137,231,73,139,52,222,137,202,255,232,251,1,11,
  255,249,15,182,216,193,232,8,73,139,12,198,73,137,12,222,139,69,0,72,15,182,
  200,72,137,207,255,249,15,182,216,102,15,87,192,252,242,65,15,17,4,222,139,
  69,0,72,15,182,200,72,137,207,255,249,15,182,216,73,187,237,237,102,73,15,
  110,195,252,242,65,15,17,4,222,139,69,0,72,15,182,200,72,137,207,255,249,
  15,182,216,193,232,8,77,139,93,0,252,242,65,15,16,132,253,195,233,252,242,
  65,15,17,4,222,139,69,0,72,15,182,200,72,137,207,255,249,15,182,216,65,199,
  68,222,4,237,139,69,0,72,15,182,200,72,137,207,255,249,15,182,216,193,232,
  8,77,139,93,0,77,139,155,233,73,139,52,195,72,11,53,244,10,73,137,52,222,
  139,69,0,72,15,182,200,72,137,207,255,249,15,182,216,73,137,172,253,36,233,
  76,137,231,73,141,52,222,255,232,251,1,12,255,249,15,182,216,193,232,8,73,
  137,172,253,36,233,76,137,231,73,141,52,222,73,141,20,198,255,232,251,1,13,
  255,249,72,15,182,216,193,232,8,15,182,204,37,252,255,0,0,0,73,137,172,253,
  36,233,76,137,231,73,141,52,222,73,141,20,198,73,141,12,206,255,232,251,1,
  14,255,249,72,15,182,216,193,232,8,73,137,172,253,36,233,76,137,231,73,141,
  52,222,137,194,255,232,251,1,15,255,249,72,15,182,216,193,232,8,15,182,204,
  37,252,255,0,0,0,73,137,172,253,36,233,76,137,231,73,141,52,222,137,194,137,
  201,255,232,251,1,16,255,232,251,1,17,255,232,251,1,18,255,232,251,1,19,255,
  232,251,1,20,255,249,72,15,183,216,193,232,16,73,137,172,253,36,233,76,137,
  231,137,198,73,141,20,222,255,232,251,1,21,255,249,72,15,183,216,73,137,172,
  253,36,233,76,137,231,137,222,255,232,251,1,22,255,249,72,15,182,216,193,
  232,8,15,182,204,37,252,255,0,0,0,73,139,20,206,72,137,215,72,193,252,239,
  32,129,252,255,239,15,131,244,21,77,139,93,0,252,242,65,15,16,132,253,195,
  233,102,72,15,110,202,252,242,15,88,193,252,242,65,15,17,4,222,139,69,0,72,
  15,182,200,72,137,207,255,249,72,15,182,216,193,232,8,15,182,204,37,252,255,
  0,0,0,73,139,20,206,72,137,215,72,193,252,239,32,129,252,255,239,15,131,244,
  21,77,139,93,0,252,242,65,15,16,132,253,195,233,102,72,15,110,202,252,242,
  15,92,193,252,242,65,15,17,4,222,139,69,0,72,15,182,200,72,137,207,255,249,
  72,15,182,216,193,232,8,15,182,204,37,252,255,0,0,0,73,139,20,206,72,137,
  215,72,193,252,239,32,129,252,255,239,15,131,244,21,77,139,93,0,252,242,65,
  15,16,132,253,195,233,102,72,15,110,202,252,242,15,89,193,252,242,65,15,17,
  4,222,139,69,0,72,15,182,200,72,137,207,255,249,72,15,182,216,193,232,8,15,
  182,204,37,252,255,0,0,0,73,139,20,206,72,137,215,72,193,252,239,32,129,252,
  255,239,15,131,244,21,77,139,93,0,252,242,65,15,16,132,253,195,233,102,72,
  15,110,202,252,242,15,94,193,252,242,65,15,17,4,222,139,69,0,72,15,182,200,
  72,137,207,255,249,72,15,182,216,193,232,8,15,182,204,37,252,255,0,0,0,73,
  139,52,198,72,137,252,247,72,193,252,239,32,129,252,255,239,15,131,244,22,
  102,72,15,110,198,77,139,93,0,252,242,65,15,16,140,253,203,233,252,242,15,
  88,193,252,242,65,15,17,4,222,139,69,0,72,15,182,200,72,137,207,255,249,72,
  15,182,216,193,232,8,15,182,204,37,252,255,0,0,0,73,139,52,198,72,137,252,
  247,72,193,252,239,32,129,252,255,239,15,131,244,22,102,72,15,110,198,77,
  139,93,0,252,242,65,15,16,140,253,203,233,252,242,15,92,193,252,242,65,15,
  17,4,222,139,69,0,72,15,182,200,72,137,207,255,249,72,15,182,216,193,232,
  8,15,182,204,37,252,255,0,0,0,73,139,52,198,72,137,252,247,72,193,252,239,
  32,129,252,255,239,15,131,244,22,102,72,15,110,198,77,139,93,0,252,242,65,
  15,16,140,253,203,233,252,242,15,89,193,252,242,65,15,17,4,222,139,69,0,72,
  15,182,200,72,137,207,255,249,72,15,182,216,193,232,8,15,182,204,37,252,255,
  0,0,0,73,139,52,198,72,137,252,247,72,193,252,239,32,129,252,255,239,15,131,
  244,22,102,72,15,110,198,77,139,93,0,252,242,65,15,16,140,253,203,233,252,
  242,15,94,193,252,242,65,15,17,4,222,139,69,0,72,15,182,200,72,137,207,255,
  249,72,15,182,216,193,232,8,15,182,204,37,252,255,0,0,0,65,129,124,253,198,
  4,239,15,131,244,23,65,129,124,253,206,4,239,15,131,244,23,252,242,65,15,
  16,4,198,252,242,65,15,88,4,206,252,242,65,15,17,4,222,139,69,0,72,15,182,
  200,72,137,207,255,249,72,15,182,216,193,232,8,15,182,204,37,252,255,0,0,
  0,65,129,124,253,198,4,239,15,131,244,23,65,129,124,253,206,4,239,15,131,
  244,23,252,242,65,15,16,4,198,252,242,65,15,92,4,206,252,242,65,15,17,4,222,
  139,69,0,72,15,182,200,72,137,207,255,249,72,15,182,216,193,232,8,15,182,
  204,37,252,255,0,0,0,65,129,124,253,198,4,239,15,131,244,23,65,129,124,253,
  206,4,239,15,131,244,23,252,242,65,15,16,4,198,252,242,65,15,89,4,206,252,
  242,65,15,17,4,222,139,69,0,72,15,182,200,72,137,207,255,249,72,15,182,216,
  193,232,8,15,182,204,37,252,255,0,0,0,65,129,124,253,198,4,239,15,131,244,
  23,65,129,124,253,206,4,239,15,131,244,23,252,242,65,15,16,4,198,252,242,
  65,15,94,4,206,252,242,65,15,17,4,222,139,69,0,72,15,182,200,72,137,207,255,
  249,72,15,182,216,193,232,8,15,182,204,37,252,255,0,0,0,255,65,129,124,253,
  198,4,239,15,131,244,23,255,73,139,125,0,252,242,15,45,140,253,207,233,252,
  242,65,15,45,4,198,255,133,201,15,132,244,27,255,153,252,247,252,249,252,
  242,15,42,194,252,242,65,15,17,4,222,139,69,0,72,15,182,200,72,137,207,255,
  65,129,124,253,206,4,239,15,131,244,23,255,73,139,125,0,252,242,15,45,132,
  253,199,233,252,242,65,15,45,12,206,255,249,72,15,182,216,193,232,8,15,182,
  204,37,252,255,0,0,0,65,129,124,253,198,4,239,15,131,244,23,255,252,242,65,
  15,45,4,198,252,242,65,15,45,12,206,255,249,72,15,182,216,193,232,8,15,182,
  204,37,252,255,0,0,0,77,139,93,0,252,242,65,15,16,132,253,195,233,65,139,
  84,206,4,129,252,250,239,15,131,244,24,252,242,65,15,16,12,206,255,232,251,
  1,23,255,249,72,15,182,216,193,232,8,15,182,204,37,252,255,0,0,0,77,139,93,
  0,252,242,65,15,16,140,253,203,233,65,139,116,198,4,129,252,254,239,15,131,
  244,25,252,242,65,15,16,4,198,255,249,252,233,244,26,255,249,72,15,182,216,
  193,232,8,15,182,204,37,252,255,0,0,0,65,129,124,253,206,4,239,15,131,244,
  28,77,139,93,0,252,242,65,15,16,132,253,195,233,102,65,15,46,4,206,15,131,
  244,247,65,199,68,222,4,237,248,2,139,69,0,72,15,182,200,72,137,207,255,139,
  69,0,72,15,182,200,72,131,197,4,193,232,8,65,252,255,36,207,248,1,65,199,
  68,222,4,237,252,233,244,2,255,249,72,15,182,216,193,232,8,15,182,204,37,
  252,255,0,0,0,65,129,124,253,206,4,239,15,131,244,28,77,139,93,0,252,242,
  65,15,16,132,253,195,233,102,65,15,46,4,206,15,135,244,247,65,199,68,222,
  4,237,248,2,139,69,0,72,15,182,200,72,137,207,255,249,72,15,182,216,193,232,
  8,15,182,204,37,252,255,0,0,0,65,129,124,253,206,4,239,15,131,244,28,77,139,
  93,0,252,242,65,15,16,132,253,195,233,102,65,15,46,4,206,15,134,244,247,65,
  199,68,222,4,237,248,2,139,69,0,72,15,182,200,72,137,207,255,249,72,15,182,
  216,193,232,8,15,182,204,37,252,255,0,0,0,65,129,124,253,206,4,239,15,131,
  244,28,77,139,93,0,252,242,65,15,16,132,253,195,233,102,65,15,46,4,206,15,
  130,244,247,65,199,68,222,4,237,248,2,139,69,0,72,15,182,200,72,137,207,255,
  249,72,15,182,216,193,232,8,15,182,204,37,252,255,0,0,0,65,129,124,253,206,
  4,239,15,131,244,28,77,139,93,0,252,242,65,15,16,132,253,195,233,102,65,15,
  46,4,206,15,133,244,247,65,199,68,222,4,237,248,2,139,69,0,72,15,182,200,
  72,137,207,255,249,72,15,182,216,193,232,8,15,182,204,37,252,255,0,0,0,65,
  129,124,253,206,4,239,15,131,244,28,77,139,93,0,252,242,65,15,16,132,253,
  195,233,102,65,15,46,4,206,15,132,244,247,65,199,68,222,4,237,248,2,139,69,
  0,72,15,182,200,72,137,207,255,249,72,15,182,216,193,232,8,15,182,204,37,
  252,255,0,0,0,65,129,124,253,198,4,239,15,131,244,29,77,139,93,0,252,242,
  65,15,16,140,253,203,233,252,242,65,15,16,4,198,102,15,46,193,15,131,244,
  247,65,199,68,222,4,237,248,2,139,69,0,72,15,182,200,72,137,207,255,249,72,
  15,182,216,193,232,8,15,182,204,37,252,255,0,0,0,65,129,124,253,198,4,239,
  15,131,244,29,77,139,93,0,252,242,65,15,16,140,253,203,233,252,242,65,15,
  16,4,198,102,15,46,193,15,135,244,247,65,199,68,222,4,237,248,2,139,69,0,
  72,15,182,200,72,137,207,255,249,72,15,182,216,193,232,8,15,182,204,37,252,
  255,0,0,0,65,129,124,253,198,4,239,15,131,244,29,77,139,93,0,252,242,65,15,
  16,140,253,203,233,252,242,65,15,16,4,198,102,15,46,193,15,134,244,247,65,
  199,68,222,4,237,248,2,139,69,0,72,15,182,200,72,137,207,255,249,72,15,182,
  216,193,232,8,15,182,204,37,252,255,0,0,0,65,129,124,253,198,4,239,15,131,
  244,29,77,139,93,0,252,242,65,15,16,140,253,203,233,252,242,65,15,16,4,198,
  102,15,46,193,15,130,244,247,65,199,68,222,4,237,248,2,139,69,0,72,15,182,
  200,72,137,207,255,249,72,15,182,216,193,232,8,15,182,204,37,252,255,0,0,
  0,65,129,124,253,198,4,239,15,131,244,29,77,139,93,0,252,242,65,15,16,140,
  253,203,233,252,242,65,15,16,4,198,102,15,46,193,15,133,244,247,65,199,68,
  222,4,237,248,2,139,69,0,72,15,182,200,72,137,207,255,249,72,15,182,216,193,
  232,8,15,182,204,37,252,255,0,0,0,65,129,124,253,198,4,239,15,131,244,29,
  77,139,93,0,252,242,65,15,16,140,253,203,233,252,242,65,15,16,4,198,102,15,
  46,193,15,132,244,247,65,199,68,222,4,237,248,2,139,69,0,72,15,182,200,72,
  137,207,255,249,72,15,182,216,193,232,8,15,182,204,37,252,255,0,0,0,65,129,
  124,253,198,4,239,15,131,244,32,65,129,124,253,206,4,239,15,131,244,32,252,
  242,65,15,16,4,198,102,65,15,46,4,206,15,131,244,247,65,199,68,222,4,237,
  248,2,139,69,0,72,15,182,200,72,137,207,255,249,72,15,182,216,193,232,8,15,
  182,204,37,252,255,0,0,0,65,129,124,253,198,4,239,15,131,244,32,65,129,124,
  253,206,4,239,15,131,244,32,252,242,65,15,16,4,198,102,65,15,46,4,206,15,
  135,244,247,65,199,68,222,4,237,248,2,139,69,0,72,15,182,200,72,137,207,255,
  249,72,15,182,216,193,232,8,15,182,204,37,252,255,0,0,0,65,129,124,253,198,
  4,239,15,131,244,32,65,129,124,253,206,4,239,15,131,244,32,252,242,65,15,
  16,4,198,102,65,15,46,4,206,15,134,244,247,65,199,68,222,4,237,248,2,139,
  69,0,72,15,182,200,72,137,207,255,249,72,15,182,216,193,232,8,15,182,204,
  37,252,255,0,0,0,65,129,124,253,198,4,239,15,131,244,32,65,129,124,253,206,
  4,239,15,131,244,32,252,242,65,15,16,4,198,102,65,15,46,4,206,15,130,244,
  247,65,199,68,222,4,237,248,2,139,69,0,72,15,182,200,72,137,207,255,249,72,
  15,182,216,193,232,8,15,182,204,37,252,255,0,0,0,65,129,124,253,198,4,239,
  15,131,244,249,65,129,124,253,206,4,239,15,131,244,249,252,242,65,15,16,4,
  198,102,65,15,46,4,206,15,133,244,247,65,199,68,222,4,237,248,2,139,69,0,
  72,15,182,200,72,137,207,255,139,69,0,72,15,182,200,72,131,197,4,193,232,
  8,65,252,255,36,207,248,1,65,199,68,222,4,237,252,233,244,2,248,3,73,139,
  52,198,73,139,20,206,72,137,252,247,73,137,211,72,193,252,238,48,72,193,252,
  234,48,72,57,214,15,133,244,250,129,252,254,239,15,132,244,251,129,252,250,
  239,15,132,244,251,65,199,68,222,4,237,252,233,244,2,248,4,65,199,68,222,
  4,237,252,233,244,2,248,5,255,72,35,61,244,11,72,139,63,128,191,233,235,15,
  133,244,253,72,139,63,76,35,29,244,11,77,139,27,65,128,187,233,235,15,133,
  244,253,77,139,27,76,57,223,15,133,244,252,65,199,68,222,4,237,248,6,65,199,
  68,222,4,237,252,233,244,2,248,7,255,249,72,15,182,216,193,232,8,15,182,204,
  37,252,255,0,0,0,77,139,93,0,77,139,155,233,73,139,52,195,73,139,20,206,72,
  139,54,128,190,233,235,15,133,244,247,72,139,54,73,137,211,73,193,252,235,
  48,65,129,252,251,239,15,133,244,247,72,35,21,244,11,72,139,18,128,186,233,
  235,15,133,244,247,72,139,18,72,57,214,15,133,244,254,65,199,68,222,4,237,
  252,233,244,253,248,8,255,65,199,68,222,4,237,248,7,139,69,0,72,15,182,200,
  72,137,207,255,139,69,0,72,15,182,200,72,131,197,4,193,232,8,65,252,255,36,
  207,248,1,252,233,244,31,255,249,72,15,182,216,193,232,8,15,182,204,37,252,
  255,0,0,0,73,139,52,198,77,139,93,0,77,139,155,233,73,139,20,203,73,137,252,
  243,73,193,252,235,48,65,129,252,251,239,15,133,244,247,72,35,53,244,11,72,
  139,54,128,190,233,235,15,133,244,247,72,139,54,72,139,18,128,186,233,235,
  15,133,244,247,72,139,18,72,57,214,15,133,244,254,65,199,68,222,4,237,252,
  233,244,253,248,8,255,139,69,0,72,15,182,200,72,131,197,4,193,232,8,65,252,
  255,36,207,248,1,252,233,244,30,255,249,72,15,182,216,193,232,8,15,182,204,
  37,252,255,0,0,0,77,139,93,0,77,139,155,233,73,139,52,195,73,139,20,206,72,
  139,54,128,190,233,235,15,133,244,247,72,139,54,73,137,211,73,193,252,235,
  48,65,129,252,251,239,15,133,244,247,72,35,21,244,11,72,139,18,128,186,233,
  235,15,133,244,247,72,139,18,72,57,214,15,132,244,254,65,199,68,222,4,237,
  252,233,244,253,248,8,255,249,72,15,182,216,193,232,8,15,182,204,37,252,255,
  0,0,0,73,139,52,198,77,139,93,0,77,139,155,233,73,139,20,203,73,137,252,243,
  73,193,252,235,48,65,129,252,251,239,15,133,244,247,72,35,53,244,11,72,139,
  54,128,190,233,235,15,133,244,247,72,139,54,72,139,18,128,186,233,235,15,
  133,244,247,72,139,18,72,57,214,15,132,244,254,65,199,68,222,4,237,252,233,
  244,253,248,8,255,249,15,182,216,193,232,8,65,129,124,253,198,4,239,15,131,
  244,254,255,252,242,65,15,16,4,198,73,187,237,237,102,73,15,110,203,102,15,
  87,193,252,242,65,15,17,4,222,139,69,0,72,15,182,200,72,137,207,255,248,8,
  73,137,172,253,36,233,76,137,231,73,139,52,198,255,232,251,1,24,255,249,15,
  182,216,193,232,8,185,237,255,102,65,129,124,253,198,6,238,15,132,244,247,
  65,129,124,253,198,4,239,15,71,13,244,14,248,1,65,137,76,222,4,139,69,0,72,
  15,182,200,72,137,207,255,249,72,15,182,216,193,232,8,102,65,129,124,253,
  222,6,238,15,132,244,248,65,129,124,253,222,4,239,15,135,244,247,248,2,72,
  139,12,36,72,141,44,129,248,1,139,69,0,72,15,182,200,72,137,207,255,249,72,
  15,182,216,193,232,8,102,65,129,124,253,222,6,238,15,132,244,248,65,129,124,
  253,222,4,239,15,134,244,248,72,139,12,36,72,141,44,129,248,2,139,69,0,72,
  15,182,200,72,137,207,255,249,72,15,182,216,193,232,8,102,65,129,124,253,
  222,6,238,15,132,244,247,65,129,124,253,222,4,239,15,134,244,247,72,139,12,
  36,72,141,44,129,248,1,139,69,0,72,15,182,200,72,137,207,255,249,72,15,183,
  216,72,139,12,36,72,141,44,153,139,69,0,72,15,182,200,72,137,207,255,102,
  65,129,124,253,198,6,238,15,133,244,33,255,73,141,52,198,73,139,4,198,72,
  35,5,244,11,72,139,0,128,184,233,235,15,133,244,255,255,72,139,128,233,72,
  139,0,255,73,139,125,0,72,139,191,233,72,193,225,4,72,139,12,15,255,139,145,
  233,35,144,233,72,141,176,233,72,141,20,82,72,141,20,214,248,2,68,139,154,
  233,65,252,247,195,237,15,132,244,254,72,139,186,233,72,139,63,128,191,233,
  235,15,133,244,249,72,139,63,72,57,252,249,15,133,244,249,72,139,186,233,
  73,137,60,222,139,69,0,72,15,182,200,72,137,207,255,139,69,0,72,15,182,200,
  72,131,197,4,193,232,8,65,252,255,36,207,248,3,65,252,247,195,237,15,132,
  244,254,65,129,227,239,75,141,60,91,72,141,20,252,254,252,233,244,2,255,248,
  8,73,137,172,253,36,233,76,137,231,72,137,206,255,232,251,1,25,255,248,9,
  73,137,172,253,36,233,76,137,231,137,202,73,141,12,222,255,232,251,1,26,255,
  249,72,15,182,216,193,232,8,15,182,204,37,252,255,0,0,0,73,137,172,253,36,
  233,76,137,231,73,141,52,198,77,139,93,0,77,139,155,233,73,139,20,203,73,
  141,12,222,255,232,251,1,27,255,249,255,72,15,182,216,193,232,8,15,182,204,
  37,252,255,0,0,0,102,65,129,124,253,222,6,238,15,133,244,33,255,73,141,52,
  222,73,139,28,222,72,35,29,244,11,72,139,27,128,187,233,235,15,133,244,255,
  255,72,139,155,233,72,139,27,73,139,125,0,72,139,191,233,72,193,224,4,72,
  139,4,7,255,139,144,233,35,147,233,72,141,179,233,72,141,20,82,72,141,20,
  214,248,2,68,139,154,233,65,252,247,195,237,15,132,244,254,72,139,186,233,
  72,139,63,128,191,233,235,15,133,244,249,72,139,63,72,57,252,248,15,133,244,
  249,73,139,60,206,72,137,186,233,139,69,0,72,15,182,200,72,137,207,255,248,
  8,73,137,172,253,36,233,76,137,231,72,137,198,255,232,251,1,28,255,248,9,
  73,137,172,253,36,233,76,137,231,137,194,73,141,12,206,255,232,251,1,29,255,
  249,72,15,182,216,193,232,8,15,182,204,37,252,255,0,0,0,73,137,172,253,36,
  233,76,137,231,73,141,52,222,77,139,93,0,77,139,155,233,73,139,20,195,73,
  141,12,206,255,232,251,1,30,255,249,72,15,182,216,193,232,8,15,182,204,37,
  252,255,0,0,0,65,129,124,253,206,4,239,15,131,244,247,252,242,73,15,45,60,
  206,72,133,252,255,15,136,244,247,255,72,137,252,249,102,65,129,124,253,198,
  6,238,15,133,244,33,73,139,52,198,72,35,53,244,11,72,139,54,128,190,233,235,
  15,133,244,34,59,142,233,15,131,244,37,72,139,150,233,72,139,2,72,139,132,
  253,200,233,73,137,4,222,139,69,0,72,15,182,200,72,137,207,255,248,1,73,137,
  172,253,36,233,76,137,231,73,141,52,198,73,141,20,206,73,141,12,222,255,249,
  72,15,182,216,193,232,8,15,182,204,37,252,255,0,0,0,102,65,129,124,253,198,
  6,238,15,133,244,33,73,139,52,198,72,35,53,244,11,72,139,54,128,190,233,235,
  15,133,244,34,59,142,233,15,131,244,37,72,139,150,233,72,139,2,72,139,132,
  253,200,233,73,137,4,222,139,69,0,72,15,182,200,72,137,207,255,65,129,124,
  253,198,4,239,15,131,244,247,255,252,242,73,15,45,60,198,72,133,252,255,15,
  132,244,247,255,72,137,252,248,102,65,129,124,253,222,6,238,15,133,244,33,
  73,139,52,222,72,35,53,244,11,72,139,54,128,190,233,235,15,133,244,35,59,
  134,233,15,131,244,36,72,139,150,233,72,139,26,73,139,52,206,72,137,180,253,
  195,233,139,69,0,72,15,182,200,72,137,207,255,248,1,73,137,172,253,36,233,
  76,137,231,73,141,52,222,73,141,20,198,73,141,12,206,255,249,72,15,182,216,
  193,232,8,15,182,204,37,252,255,0,0,0,102,65,129,124,253,222,6,238,15,133,
  244,33,73,139,52,222,72,35,53,244,11,72,139,54,128,190,233,235,15,133,244,
  35,59,134,233,15,131,244,36,72,139,150,233,72,139,26,73,139,52,206,72,137,
  180,253,195,233,139,69,0,72,15,182,200,72,137,207,255,249,72,15,182,216,193,
  232,8,255,73,139,140,253,36,233,72,139,9,255,72,139,137,233,72,139,9,255,
  139,144,233,35,145,233,72,141,177,233,72,141,20,82,72,141,20,214,248,2,68,
  139,154,233,65,252,247,195,237,15,132,244,254,72,139,186,233,72,139,63,128,
  191,233,235,15,133,244,249,72,139,63,72,57,252,248,15,133,244,249,72,139,
  178,233,73,137,52,222,139,69,0,72,15,182,200,72,137,207,255,232,251,1,31,
  255,249,72,15,182,216,193,232,8,73,137,172,253,36,233,76,137,231,73,141,52,
  222,77,139,93,0,77,139,155,233,73,139,20,195,255,232,251,1,32,255,73,139,
  125,0,72,139,191,233,72,193,227,4,72,139,28,31,139,147,233,35,145,233,72,
  141,177,233,72,141,20,82,72,141,20,214,248,2,68,139,154,233,65,252,247,195,
  237,15,132,244,254,72,139,186,233,72,139,63,128,191,233,235,15,133,244,249,
  72,139,63,72,57,252,251,15,133,244,249,73,139,52,198,72,137,178,233,139,69,
  0,72,15,182,200,72,137,207,255,139,69,0,72,15,182,200,72,131,197,4,193,232,
  8,65,252,255,36,207,248,3,65,252,247,195,237,15,132,244,254,65,129,227,239,
  75,141,60,91,72,141,20,252,254,252,233,244,2,248,8,73,137,172,253,36,233,
  76,137,231,72,137,222,255,232,251,1,33,255,249,72,15,183,216,193,232,16,73,
  137,172,253,36,233,76,137,231,77,139,93,0,77,139,155,233,73,139,52,219,73,
  141,20,198,255,232,251,1,34,255,249,72,15,182,216,193,232,8,77,139,156,253,
  36,233,77,139,155,233,73,139,52,195,73,137,52,222,139,69,0,72,15,182,200,
  72,137,207,255,249,72,15,183,216,193,232,16,73,139,20,198,77,139,156,253,
  36,233,77,139,155,233,73,137,20,219,139,69,0,72,15,182,200,72,137,207,255,
  65,129,190,253,252,252,7,0,0,239,15,132,244,247,248,2,139,69,0,72,15,182,
  200,72,137,207,255,139,69,0,72,15,182,200,72,131,197,4,193,232,8,65,252,255,
  36,207,248,1,72,139,12,36,72,141,44,129,252,233,244,2,255,249,15,182,216,
  193,232,8,65,129,124,253,222,4,239,15,131,244,252,255,65,129,124,253,198,
  4,239,15,131,244,252,255,252,242,65,15,16,4,222,102,65,15,46,4,198,15,131,
  244,254,255,139,93,0,72,139,12,36,72,141,44,153,248,7,139,69,0,72,15,182,
  200,72,137,207,255,139,69,0,72,15,182,200,72,131,197,4,193,232,8,65,252,255,
  36,207,248,8,72,131,197,4,252,233,244,7,255,248,6,73,137,172,253,36,233,76,
  137,231,73,141,52,222,73,141,20,198,139,77,0,255,232,251,1,35,255,133,192,
  15,132,244,19,73,139,172,253,36,233,139,69,0,72,15,182,200,72,137,207,255,
  249,72,15,182,216,193,232,8,15,182,204,37,252,255,0,0,0,65,129,124,253,222,
  4,239,15,131,244,252,65,129,124,253,198,4,239,15,131,244,252,65,129,124,253,
  206,4,239,15,131,244,252,255,252,242,65,15,16,4,222,252,242,65,15,88,4,206,
  102,65,15,46,4,198,252,242,65,15,17,4,222,15,131,244,254,255,248,6,73,137,
  172,253,36,233,76,137,231,73,141,52,222,73,141,20,198,73,141,12,206,68,139,
  69,0,255,232,251,1,36,255,249,139,69,0,72,15,182,200,72,137,207,255,232,251,
  1,37,255,232,251,1,38,255,232,251,1,39,255,249,72,15,182,216,193,232,8,15,
  182,204,37,252,255,0,0,0,73,141,60,198,73,59,188,253,36,233,15,131,244,255,
  248,3,102,65,129,124,253,222,6,238,15,133,244,40,73,139,20,222,72,35,21,244,
  11,72,139,50,128,190,233,235,15,133,244,38,58,142,233,15,133,244,41,65,15,
  183,94,252,246,72,193,227,48,72,9,252,235,73,137,94,252,240,72,193,224,51,
  72,137,71,252,240,72,137,87,252,248,73,137,148,253,36,233,76,139,174,233,
  72,139,174,233,255,73,137,252,254,73,137,188,253,36,233,72,137,44,36,139,
  69,0,72,15,182,200,72,137,207,255,139,69,0,72,15,182,200,72,131,197,4,193,
  232,8,65,252,255,36,207,248,9,73,137,172,253,36,233,72,137,68,36,56,72,137,
  68,36,64,76,137,231,255,232,251,1,40,255,133,192,15,132,244,19,72,139,68,
  36,56,72,139,76,36,64,252,233,244,3,255,249,72,15,182,216,193,232,8,15,182,
  204,37,252,255,0,0,0,73,141,60,198,73,59,188,253,36,233,15,131,244,255,248,
  3,102,65,129,124,253,222,6,238,15,133,244,40,73,139,20,222,72,35,21,244,11,
  72,139,50,128,190,233,235,15,133,244,39,58,142,233,15,133,244,41,65,15,183,
  94,252,246,72,193,227,48,72,9,252,235,73,137,94,252,240,72,193,224,51,72,
  137,71,252,240,72,137,87,252,248,65,198,70,252,255,1,73,137,148,253,36,233,
  76,139,174,233,72,139,174,233,255,249,72,139,21,244,16,248,2,73,15,183,94,
  252,246,72,129,252,251,239,15,132,244,20,73,41,222,65,128,126,252,255,1,15,
  132,244,2,248,1,73,139,118,252,248,73,137,180,253,36,233,72,139,30,76,139,
  171,233,73,139,110,252,240,72,35,45,244,12,72,139,131,233,72,137,4,36,65,
  199,134,252,252,7,0,0,237,139,69,0,72,15,182,200,72,137,207,255,249,73,139,
  150,252,248,7,0,0,248,2,73,15,183,94,252,246,72,129,252,251,239,15,132,244,
  20,73,41,222,65,128,126,252,255,1,15,132,244,2,248,1,73,139,118,252,248,73,
  137,180,253,36,233,72,139,30,76,139,171,233,73,139,110,252,240,72,35,45,244,
  12,72,139,131,233,72,137,4,36,73,137,150,252,248,7,0,0,139,69,0,72,15,182,
  200,72,137,207,255,249,205,3,255
};

#line 781 "src/interpreter/bytecode-interpreter.dasc"
//|.globals GLBNAME_
enum {
  GLBNAME_ValueHeapMaskStore,
  GLBNAME_ValueHeapMaskLoad,
  GLBNAME_PointerMask,
  GLBNAME_PointerTag,
  GLBNAME_FlagTrueConst,
  GLBNAME_FlagFalseConst,
  GLBNAME_ValueNullConst,
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
  GLBNAME_ModByZero,
  GLBNAME_InterpCompareRV,
  GLBNAME_InterpCompareVR,
  GLBNAME_InterpCompareVS,
  GLBNAME_InterpCompareSV,
  GLBNAME_InterpCompareVV,
  GLBNAME_InterpPropNeedObject,
  GLBNAME_InterpIdxGetI,
  GLBNAME_InterpIdxSetI,
  GLBNAME_InterpIdxOutOfBoundSet,
  GLBNAME_InterpIdxOutOfBoundGet,
  GLBNAME_InterpCall,
  GLBNAME_InterpTCall,
  GLBNAME_InterpNeedObject,
  GLBNAME_InterpArgumentMismatch,
  GLBNAME__MAX
};
#line 782 "src/interpreter/bytecode-interpreter.dasc"
//|.globalnames glbnames
static const char *const glbnames[] = {
  "ValueHeapMaskStore",
  "ValueHeapMaskLoad",
  "PointerMask",
  "PointerTag",
  "FlagTrueConst",
  "FlagFalseConst",
  "ValueNullConst",
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
  "ModByZero",
  "InterpCompareRV",
  "InterpCompareVR",
  "InterpCompareVS",
  "InterpCompareSV",
  "InterpCompareVV",
  "InterpPropNeedObject",
  "InterpIdxGetI",
  "InterpIdxSetI",
  "InterpIdxOutOfBoundSet",
  "InterpIdxOutOfBoundGet",
  "InterpCall",
  "InterpTCall",
  "InterpNeedObject",
  "InterpArgumentMismatch",
  (const char *)0
};
#line 783 "src/interpreter/bytecode-interpreter.dasc"
//|.externnames extnames
static const char *const extnames[] = {
  "PrintOP",
  "InterpreterArithmetic",
  "InterpreterPow",
  "InterpreterModByZero",
  "InterpreterCompare",
  "InterpreterPropNeedObject",
  "InterpreterIdxGet",
  "InterpreterIdxSet",
  "InterpreterIdxOutOfBound",
  "InterpreterCall",
  "InterpreterCallNeedObject",
  "InterpreterArgumentMismatch",
  "InterpreterLoadList0",
  "InterpreterLoadList1",
  "InterpreterLoadList2",
  "InterpreterNewList",
  "InterpreterAddList",
  "InterpreterLoadObj0",
  "InterpreterLoadObj1",
  "InterpreterNewObj",
  "InterpreterAddObj",
  "InterpreterLoadCls",
  "InterpreterInitCls",
  "pow",
  "InterpreterNegateFail",
  "InterpreterPropGetSSONotFound",
  "InterpreterPropGetSSO",
  "InterpreterPropGet",
  "InterpreterPropSetSSONotFound",
  "InterpreterPropSetSSO",
  "InterpreterPropSet",
  "InterpreterGGetNotFoundSSO",
  "InterpreterGGet",
  "InterpreterGSetNotFoundSSO",
  "InterpreterGSet",
  "InterpreterForEnd1",
  "InterpreterForEnd2",
  "InterpreterFEStart",
  "InterpreterFEEnd",
  "InterpreterIDref",
  "ResizeStack",
  (const char *)0
};
#line 784 "src/interpreter/bytecode-interpreter.dasc"
//|.section code,data
#define DASM_SECTION_CODE	0
#define DASM_SECTION_DATA	1
#define DASM_MAXSECTION		2
#line 785 "src/interpreter/bytecode-interpreter.dasc"

/* -------------------------------------------------------------------
 * Preprocessor option for dynasm
 * ------------------------------------------------------------------*/
//|.define CHECK_MOD_BY_ZERO
//|.define CHECK_NUMBER_MEMORY,0
//|.define TRACE_OP, 1
//|.define USE_CMOV_COMP,0

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
     // This branch should be really rare to happen on Linux since Linux map the whole
     // application binary to first 0-2GB memory.
//||   lava_warn("%s","Function FUNC address is not in 0-2GB");
//|.if 0
// I don't know whether this is faster than use rax , need profile. I see
// this one is used in some other places/VM implementation
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
//|.define STKL,                  r14d

//|.define ACCIDX,                2040
//|.define ACCFIDX,               2044
//|.define ACCFHIDX,              2046  // for heap flag
//|.define ACC,                   STK+ACCIDX

// Dispatch table pointer
//|.define DISPATCH,              r15  // callee saved

// Bytecode array
//|.define PC,                    rbp  // callee saved
//|.define PCL,                   ebp

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

//|.define SAVED_PPC,             rsp
//|.define SAVED_PC ,             [rsp]

// Used to save certain registers while we call cross the function
// boundary. Like we may call into ToBoolean function to get value
// of certain register's Boolean value and we may need to save register
// like rax which is part of our argument/operand of isntructions
//|.define SAVED_SLOT1,           rsp+56
//|.define SAVED_SLOT2,           rsp+64

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

void Print64( std::uint64_t a , std::uint64_t b , std::uint64_t c ) {
  lava_error("%" LAVA_FMTU64 ":%" LAVA_FMTU64 ":%" LAVA_FMTU64,a,b,c);
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(Print64)

void PrintV( const Value& v ) {
  lava_error("%s",v.type_name());
}
INTERPRETER_REGISTER_EXTERN_SYMBOL(PrintV)

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
//|  mov INSTR,dword [PC]  // 1 uops
//|  movzx OP,INSTR_OP     // 1 uops
//|  add PC,4              // 1 uops
//|  shr INSTR,8           // 1 uops
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
//|.macro rconstH,XREG,HIGH
//|  mov64 T1,(static_cast<std::uint64_t>(HIGH)<<32)
//|  movd XREG,T1
//|.endmacro

//|.macro rconstL,XREG,LOW
//|  mov64 T1,(static_cast<std::uint64_t>(LOW))
//|  movd XREG,T1
//|.endmacro

//|.macro rconst,XREG,X64V
//|  mov64 T1,X64V
//|  movd  XREG,T1
//|.endmacro

// Used to negate the double precision number's sign bit
//|.macro rconst_sign,XREG; rconstH XREG,0x80000000; .endmacro
//|.macro rconst_one ,XREG; rconstH XREG,0x3ff00000; .endmacro
//|.macro rconst_neg_one,XREG; rconstH XREG,0xbff00000; .endmacro

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
//|  mov T1,qword [T1+ClosureLayout::kUpValueOffset]
//|  mov reg, qword [T1+index*8]
//|.endmacro

//|.macro StUV,index,reg
//|  mov T1,qword [RUNTIME+RuntimeLayout::kCurClsOffset]
//|  mov T1,qword [T1+ClosureLayout::kUpValueOffset]
//|  mov qword [T1+index*8], reg
//|.endmacro

// ----------------------------------------------
// Heap value related stuff

// This byte offset in little endian for type pattern inside of
// heap object header
#define HOH_TYPE_OFFSET 7

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
//|  or  dest,qword [->ValueHeapMaskLoad]
//|.endif
//|.endmacro

//|.macro DerefPtrFromV,v
//|  and v,qword [->ValueHeapMaskLoad]
//|.endmacro

// It is painful to load a string into its Value format
//|.macro LdStrV,val,index
//|  mov T1 , qword [PROTO]
//|  mov T1 , qword [T1+PrototypeLayout::kStringTableOffset]
//|  mov val, qword [T1+index*8]
//|  StHeap val
//|.endmacro

//|.macro LdStr,val,index
//|  mov T1 , qword [PROTO]
//|  mov T1 , qword [T1+PrototypeLayout::kStringTableOffset]
//|  mov val, qword [T1+index*8]
//|.endmacro

// Load SSO value from sso table
//|.macro LdSSO,val,index,temp
//|  mov temp, qword [PROTO]
//|  mov temp, qword [temp+PrototypeLayout::kSSOTableOffset]
//|  shl index,4
//|  mov val , qword [temp+index]
//|.endmacro

// Check whether a Value is a HeapObject
//|.macro CheckHeap,val,fail_label
//|  mov T1,val
//|  shr T1,48
//|  cmp T1L, Value::FLAG_HEAP
//|  jne fail_label
//|.endmacro

// General macro to check a heap object is certain type
//|.macro CheckHeapPtrT,val,pattern,fail_label
//|  cmp byte [val-HOH_TYPE_OFFSET], pattern
//|  jne fail_label
//|.endmacro

//|.macro CheckHeapT,val,pattern,fail_label
//|  and val,qword [->ValueHeapMaskLoad]
//|  mov val,qword [val]
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

// If fail, reg will be set to store the value of that
// actual pointer points to a *unknown* heap object;
// otherwise reg will be set to store the value of pointer
// for that SSO object
//|.macro CheckSSORaw,reg,fail
//|  and reg,qword [->ValueHeapMaskLoad]
//|  mov reg,qword [reg]
//|  cmp byte [reg-HOH_TYPE_OFFSET], SSO_BIT_PATTERN
//|  jne fail
//|  mov reg,qword [reg]
//|.endmacro

#define INTERP_HELPER_LIST(__)               \
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
  __(MOD_BY_ZERO,ModByZero)                  \
  /* comparison */                           \
  __(INTERP_COMPARERV,InterpCompareRV)       \
  __(INTERP_COMPAREVR,InterpCompareVR)       \
  __(INTERP_COMPARESV,InterpCompareSV)       \
  __(INTERP_COMPAREVS,InterpCompareVS)       \
  __(INTERP_COMPAREVV,InterpCompareVV)       \
  /* property get/set */                     \
  __(INTERP_IDX_GETI,InterpIdxGetI)                   \
  __(INTERP_IDX_SETI,InterpIdxSetI)                   \
  __(INTERP_PROP_NEEDOBJECT ,InterpPropNeedObject)    \
  __(INTERP_IDX_OUTOFBOUND_GET,InterpIdxOutOfBoundGet)\
  __(INTERP_IDX_OUTOFBOUND_SET,InterpIdxOutOfBoundSet)\
  /* call */                                          \
  __(INTERP_CALL,InterpCall)                          \
  __(INTERP_TCALL,InterpTCall)                        \
  __(INTERP_NEEDOBJECT,InterpNeedObject)              \
  __(INTERP_ARGUMENTMISMATCH,InterpArgumentMismatch)  \
  /* ---- Debug Helper ---- */                        \
  __(PRINT_OP,PrintOP)                                \
  __(PRINT2  ,Print2 )                                \
  __(PRINT64 ,Print64)                                \
  __(PRINTF  ,PrintF )                                \
  __(PRINTV , PrintV )

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

#define IFRAME_EOF 0xffff  // End of function frame, should return from VM

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
#line 1347 "src/interpreter/bytecode-interpreter.dasc"
  //|->ValueHeapMaskStore:
  //|.dword Value::TAG_HEAP_STORE_MASK_LOWER,Value::TAG_HEAP_STORE_MASK_HIGHER // 8 bytes
  dasm_put(Dst, 2, Value::TAG_HEAP_STORE_MASK_LOWER, Value::TAG_HEAP_STORE_MASK_HIGHER);
#line 1349 "src/interpreter/bytecode-interpreter.dasc"

  //|->ValueHeapMaskLoad:
  //|->PointerMask:
  //|.dword Value::TAG_HEAP_LOAD_MASK_LOWER,Value::TAG_HEAP_LOAD_MASK_HIGHER   // 8 bytes
  dasm_put(Dst, 7, Value::TAG_HEAP_LOAD_MASK_LOWER, Value::TAG_HEAP_LOAD_MASK_HIGHER);
#line 1353 "src/interpreter/bytecode-interpreter.dasc"

  //|->PointerTag:
  //|.dword 0,0xffff0000 // 8 bytes
  dasm_put(Dst, 14);
#line 1356 "src/interpreter/bytecode-interpreter.dasc"

  //|->FlagTrueConst:
  //|.dword Value::FLAG_TRUE  // 4 bytes
  dasm_put(Dst, 27, Value::FLAG_TRUE);
#line 1359 "src/interpreter/bytecode-interpreter.dasc"

  //|->FlagFalseConst:
  //|.dword Value::FLAG_FALSE // 4 bytes
  dasm_put(Dst, 31, Value::FLAG_FALSE);
#line 1362 "src/interpreter/bytecode-interpreter.dasc"

  //|->ValueNullConst:
  //|.dword 0, Value::FLAG_NULL // 8 bytes
  dasm_put(Dst, 35, Value::FLAG_NULL);
#line 1365 "src/interpreter/bytecode-interpreter.dasc"

  //|->RealZero:
  //|.dword 0,0  // 8 btyes
  dasm_put(Dst, 43);
#line 1368 "src/interpreter/bytecode-interpreter.dasc"

  //|.code
  dasm_put(Dst, 54);
#line 1370 "src/interpreter/bytecode-interpreter.dasc"

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
  dasm_put(Dst, 56,  INTERP_START);
#line 1402 "src/interpreter/bytecode-interpreter.dasc"
  // save all callee saved register since we use them to keep tracking of
  // our most important data structure
  //|  interp_prolog
  dasm_put(Dst, 60);
#line 1405 "src/interpreter/bytecode-interpreter.dasc"

  //|  mov RUNTIME ,CARG1                 // runtime
  //|  mov PROTO   ,CARG3                 // proto
  //|  mov STK     ,CARG4                 // stack
  //|  mov PC      ,CARG5                 // pc
  //|  mov DISPATCH,CARG6                 // dispatch
  dasm_put(Dst, 96);
#line 1411 "src/interpreter/bytecode-interpreter.dasc"

  //|  mov qword SAVED_PC,PC              // save the *start* of bc array
  dasm_put(Dst, 113);
#line 1413 "src/interpreter/bytecode-interpreter.dasc"

  // setup the call frame
  //|  mov eax,  IFRAME_EOF
  //|  shl rax,  48
  //|  mov qword [STK],   rax             // Set the IFrame upper 8 bytes to be 0
  //|  mov qword [STK+8], CARG2           // Reset the flag/narg and set the Caller to be PROTO
  //|  add STK,16                         // Bump the STK register
  dasm_put(Dst, 118, IFRAME_EOF);
#line 1420 "src/interpreter/bytecode-interpreter.dasc"

  //|  mov qword [RUNTIME+RuntimeLayout::kCurClsOffset], CARG2
  //|  mov qword [RUNTIME+RuntimeLayout::kCurStackOffset], STK
  dasm_put(Dst, 136, RuntimeLayout::kCurClsOffset, RuntimeLayout::kCurStackOffset);
#line 1423 "src/interpreter/bytecode-interpreter.dasc"

  // run
  //|  Dispatch
  dasm_put(Dst, 149);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 160);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 173);
#line 1426 "src/interpreter/bytecode-interpreter.dasc"

  /* -------------------------------------------
   * Interpreter exit handler                  |
   * ------------------------------------------*/
  //|=> INTERP_FAIL:
  //|->InterpFail:
  //|  xor eax,eax
  //|  interp_epilog
  //|  ret
  dasm_put(Dst, 193,  INTERP_FAIL);
#line 1435 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_RETURN:
  //|->InterpReturn:
  //|  mov rax, RREG
  //|  mov qword [RUNTIME+RuntimeLayout::kRetOffset],rax
  //|  mov rax,1
  dasm_put(Dst, 234,  INTERP_RETURN, RuntimeLayout::kRetOffset);
#line 1441 "src/interpreter/bytecode-interpreter.dasc"

  //|  interp_epilog
  //|  ret
  dasm_put(Dst, 198);
#line 1444 "src/interpreter/bytecode-interpreter.dasc"
}

/* ------------------------------------------
 * helper functions/routines generation     |
 * -----------------------------------------*/

// ----------------------------------------
// helper macros
// ----------------------------------------
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

  /* -------------------------------------------------------------------------
   * InterpArithXXX
   *
   * C++'s ABI is kind of hard to maintain correctly between Assembly
   * code and normal function in C++. The object passing mechanism is
   * not easy if you try to use pass by *value*. So to make things
   * easir, our internal C++ function *all* use reference/pointer, which
   * avoid the passing by value's ABI problem. However, sometimes our
   * value are in register, so we need to spill it into stack to make
   * ABI works.
   *
   * We have SAVED_SLOT1/SAVED_SLOT2 for this cases
   * -----------------------------------------------------------------------*/
  //|=> INTERP_ARITH_REALL:
  //|->InterpArithRealL:
  //|  savepc
  //|  mov CARG1,RUNTIME
  dasm_put(Dst, 254,  INTERP_ARITH_REALL, RuntimeLayout::kCurPCOffset);
#line 1485 "src/interpreter/bytecode-interpreter.dasc"

  //|  LdRealV T2,ARG2F
  //|  lea CARG2,[SAVED_SLOT1]
  //|  mov qword [SAVED_SLOT1], T2
  dasm_put(Dst, 267, PrototypeLayout::kRealTableOffset);
#line 1489 "src/interpreter/bytecode-interpreter.dasc"

  //|  lea CARG3, [STK+ARG3F*8]
  //|  lea CARG4, [STK+ARG1F*8]
  //|  fcall InterpreterArithmetic
  dasm_put(Dst, 288);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterArithmetic))) {
  dasm_put(Dst, 297);
   } else {
     lava_warn("%s","Function InterpreterArithmetic address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterArithmetic)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterArithmetic))>>32));
   }
#line 1493 "src/interpreter/bytecode-interpreter.dasc"
  //|  retbool
  dasm_put(Dst, 302);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 160);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 173);
#line 1494 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_ARITH_REALR:
  //|->InterpArithRealR:
  //|  savepc
  //|  mov CARG1,RUNTIME
  //|  lea CARG2,[STK+ARG2F*8]
  dasm_put(Dst, 319,  INTERP_ARITH_REALR, RuntimeLayout::kCurPCOffset);
#line 1500 "src/interpreter/bytecode-interpreter.dasc"

  //|  LdRealV T2,ARG3F
  //|  lea CARG3,[SAVED_SLOT1]
  //|  mov qword [SAVED_SLOT1], T2
  dasm_put(Dst, 336, PrototypeLayout::kRealTableOffset);
#line 1504 "src/interpreter/bytecode-interpreter.dasc"

  //|  lea CARG4, [STK+ARG1F*8]
  //|  fcall InterpreterArithmetic
  dasm_put(Dst, 292);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterArithmetic))) {
  dasm_put(Dst, 297);
   } else {
     lava_warn("%s","Function InterpreterArithmetic address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterArithmetic)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterArithmetic))>>32));
   }
#line 1507 "src/interpreter/bytecode-interpreter.dasc"
  //|  retbool
  dasm_put(Dst, 302);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 160);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 173);
#line 1508 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_ARITH_VV:
  //|->InterpArithVV:
  //|  savepc
  //|  mov CARG1, RUNTIME
  //|  lea CARG2, [STK+ARG2F*8]
  //|  lea CARG3, [STK+ARG3F*8]
  //|  lea CARG4, [STK+ARG1F*8] // ARG3F == CARG4
  //|  fcall InterpreterArithmetic
  dasm_put(Dst, 357,  INTERP_ARITH_VV, RuntimeLayout::kCurPCOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterArithmetic))) {
  dasm_put(Dst, 297);
   } else {
     lava_warn("%s","Function InterpreterArithmetic address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterArithmetic)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterArithmetic))>>32));
   }
#line 1517 "src/interpreter/bytecode-interpreter.dasc"
  //|  retbool
  dasm_put(Dst, 302);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 160);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 173);
#line 1518 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_POW_SLOWRV:
  //|->InterpPowSlowRV:
  //|  savepc
  //|  mov CARG1, RUNTIME
  dasm_put(Dst, 382,  INTERP_POW_SLOWRV, RuntimeLayout::kCurPCOffset);
#line 1523 "src/interpreter/bytecode-interpreter.dasc"

  //|  LdRealV T2,ARG2F
  //|  lea CARG2, [SAVED_SLOT1]
  //|  mov qword  [SAVED_SLOT1], T2
  dasm_put(Dst, 267, PrototypeLayout::kRealTableOffset);
#line 1527 "src/interpreter/bytecode-interpreter.dasc"

  //|  lea CARG3, [STK+ARG3F*8]
  //|  lea CARG4, [STK+ARG1F*8]
  //|  fcall InterpreterPow
  dasm_put(Dst, 288);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterPow))) {
  dasm_put(Dst, 395);
   } else {
     lava_warn("%s","Function InterpreterPow address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterPow)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterPow))>>32));
   }
#line 1531 "src/interpreter/bytecode-interpreter.dasc"
  //|  retbool
  dasm_put(Dst, 302);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 160);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 173);
#line 1532 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_POW_SLOWVR:
  //|->InterpPowSlowVR:
  //|  savepc
  //|  mov CARG1, RUNTIME
  //|  lea CARG2, [STK+ARG2F*8]
  dasm_put(Dst, 400,  INTERP_POW_SLOWVR, RuntimeLayout::kCurPCOffset);
#line 1538 "src/interpreter/bytecode-interpreter.dasc"

  //|  LdRealV T2,ARG3F
  //|  mov qword [SAVED_SLOT1], T2
  //|  lea CARG3,[SAVED_SLOT1]
  dasm_put(Dst, 417, PrototypeLayout::kRealTableOffset);
#line 1542 "src/interpreter/bytecode-interpreter.dasc"

  //|  lea CARG4, [STK+ARG1F*8]
  //|  fcall InterpreterPow
  dasm_put(Dst, 292);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterPow))) {
  dasm_put(Dst, 395);
   } else {
     lava_warn("%s","Function InterpreterPow address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterPow)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterPow))>>32));
   }
#line 1545 "src/interpreter/bytecode-interpreter.dasc"
  //|  retbool
  dasm_put(Dst, 302);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 160);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 173);
#line 1546 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_POW_SLOWVV:
  //|->InterpPowSlowVV:
  //|  savepc
  //|  instr_D
  //|  mov CARG1, RUNTIME
  //|  lea CARG2, [STK+ARG2F*8]
  //|  lea CARG3, [STK+ARG3F*8]
  //|  lea CARG4, [STK+ARG1F*8]
  //|  fcall InterpreterPow
  dasm_put(Dst, 438,  INTERP_POW_SLOWVV, RuntimeLayout::kCurPCOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterPow))) {
  dasm_put(Dst, 395);
   } else {
     lava_warn("%s","Function InterpreterPow address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterPow)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterPow))>>32));
   }
#line 1556 "src/interpreter/bytecode-interpreter.dasc"
  //|  retbool
  dasm_put(Dst, 302);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 160);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 173);
#line 1557 "src/interpreter/bytecode-interpreter.dasc"

  /* -------------------------------------------
   * Interp Arithmetic Exception               |
   * ------------------------------------------*/
  //|=> MOD_BY_ZERO:
  //|->ModByZero:
  //|  savepc
  //|  mov CARG1,RUNTIME
  //|  fcall InterpreterModByZero
  dasm_put(Dst, 479,  MOD_BY_ZERO, RuntimeLayout::kCurPCOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterModByZero))) {
  dasm_put(Dst, 492);
   } else {
     lava_warn("%s","Function InterpreterModByZero address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterModByZero)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterModByZero))>>32));
   }
#line 1566 "src/interpreter/bytecode-interpreter.dasc"
  //|  jmp ->InterpFail
  dasm_put(Dst, 497);
#line 1567 "src/interpreter/bytecode-interpreter.dasc"

  /* -------------------------------------------
   * Interp Comparison                         |
   * ------------------------------------------*/
  //|=> INTERP_COMPARERV:
  //|->InterpCompareRV:
  //|  savepc
  //|  mov CARG1,RUNTIME
  dasm_put(Dst, 502,  INTERP_COMPARERV, RuntimeLayout::kCurPCOffset);
#line 1575 "src/interpreter/bytecode-interpreter.dasc"

  //|  LdRealV T2,ARG2F
  //|  lea CARG2, [SAVED_SLOT1]
  //|  mov qword  [SAVED_SLOT1], T2
  dasm_put(Dst, 267, PrototypeLayout::kRealTableOffset);
#line 1579 "src/interpreter/bytecode-interpreter.dasc"

  //|  lea CARG3, [STK+ARG2F*8]
  //|  lea CARG4, [STK+ARG1F*8]
  //|  fcall InterpreterCompare
  dasm_put(Dst, 515);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterCompare))) {
  dasm_put(Dst, 524);
   } else {
     lava_warn("%s","Function InterpreterCompare address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterCompare)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterCompare))>>32));
   }
#line 1583 "src/interpreter/bytecode-interpreter.dasc"
  //|  retbool
  dasm_put(Dst, 302);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 160);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 173);
#line 1584 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_COMPAREVR:
  //|->InterpCompareVR:
  //|  savepc
  //|  mov CARG1,RUNTIME
  //|  lea CARG2, [STK+ARG2F*8]
  dasm_put(Dst, 529,  INTERP_COMPAREVR, RuntimeLayout::kCurPCOffset);
#line 1590 "src/interpreter/bytecode-interpreter.dasc"

  //|  LdRealV T2,ARG3F
  //|  lea CARG3, [SAVED_SLOT1]
  //|  mov qword  [SAVED_SLOT1], T2
  dasm_put(Dst, 336, PrototypeLayout::kRealTableOffset);
#line 1594 "src/interpreter/bytecode-interpreter.dasc"

  //|  lea CARG4, [STK+ARG1F*8]
  //|  fcall InterpreterCompare
  dasm_put(Dst, 292);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterCompare))) {
  dasm_put(Dst, 524);
   } else {
     lava_warn("%s","Function InterpreterCompare address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterCompare)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterCompare))>>32));
   }
#line 1597 "src/interpreter/bytecode-interpreter.dasc"
  //|  retbool
  dasm_put(Dst, 302);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 160);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 173);
#line 1598 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_COMPAREVS:
  //|->InterpCompareVS:
  //|  savepc
  //|  mov CARG1, RUNTIME
  //|  lea CARG2, [STK+ARG2F*8]
  dasm_put(Dst, 546,  INTERP_COMPAREVS, RuntimeLayout::kCurPCOffset);
#line 1604 "src/interpreter/bytecode-interpreter.dasc"

  //|  LdStrV T2, ARG3F
  //|  lea CARG3, [SAVED_SLOT1]
  //|  mov qword [SAVED_SLOT1],T2
  dasm_put(Dst, 563, PrototypeLayout::kStringTableOffset);
#line 1608 "src/interpreter/bytecode-interpreter.dasc"

  //|  lea CARG4, [STK+ARG1F*8]
  //|  fcall InterpreterCompare
  dasm_put(Dst, 292);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterCompare))) {
  dasm_put(Dst, 524);
   } else {
     lava_warn("%s","Function InterpreterCompare address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterCompare)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterCompare))>>32));
   }
#line 1611 "src/interpreter/bytecode-interpreter.dasc"
  //|  retbool
  dasm_put(Dst, 302);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 160);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 173);
#line 1612 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_COMPARESV:
  //|->InterpCompareSV:
  //|  savepc
  //|  mov CARG1, RUNTIME
  dasm_put(Dst, 591,  INTERP_COMPARESV, RuntimeLayout::kCurPCOffset);
#line 1617 "src/interpreter/bytecode-interpreter.dasc"

  //|  LdStrV T2, ARG2F
  //|  lea CARG2, [SAVED_SLOT1]
  //|  mov qword [SAVED_SLOT1], T2
  dasm_put(Dst, 604, PrototypeLayout::kStringTableOffset);
#line 1621 "src/interpreter/bytecode-interpreter.dasc"

  //|  lea CARG3, [STK+ARG3F*8]
  //|  lea CARG4, [STK+ARG1F*8]
  //|  fcall InterpreterCompare
  dasm_put(Dst, 288);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterCompare))) {
  dasm_put(Dst, 524);
   } else {
     lava_warn("%s","Function InterpreterCompare address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterCompare)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterCompare))>>32));
   }
#line 1625 "src/interpreter/bytecode-interpreter.dasc"
  //|  retbool
  dasm_put(Dst, 302);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 160);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 173);
#line 1626 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_COMPAREVV:
  //|->InterpCompareVV:
  //|  savepc
  //|  mov CARG1, RUNTIME
  //|  lea CARG2, [STK+ARG2F*8]
  //|  lea CARG3, [STK+ARG3F*8]
  //|  lea CARG4, [STK+ARG1F*8]
  //|  fcall InterpreterCompare
  dasm_put(Dst, 632,  INTERP_COMPAREVV, RuntimeLayout::kCurPCOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterCompare))) {
  dasm_put(Dst, 524);
   } else {
     lava_warn("%s","Function InterpreterCompare address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterCompare)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterCompare))>>32));
   }
#line 1635 "src/interpreter/bytecode-interpreter.dasc"
  //|  retbool
  dasm_put(Dst, 302);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 160);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 173);
#line 1636 "src/interpreter/bytecode-interpreter.dasc"

  /* -------------------------------------------------
   * Property Get/Set                                |
   * ------------------------------------------------*/
  //|=> INTERP_PROP_NEEDOBJECT:
  //|->InterpPropNeedObject:
  //|  savepc
  //|  mov CARG1, RUNTIME
  //|  lea CARG2, [STK+ARG2F*8]
  //|  fcall InterpreterPropNeedObject
  dasm_put(Dst, 657,  INTERP_PROP_NEEDOBJECT, RuntimeLayout::kCurPCOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterPropNeedObject))) {
  dasm_put(Dst, 674);
   } else {
     lava_warn("%s","Function InterpreterPropNeedObject address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterPropNeedObject)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterPropNeedObject))>>32));
   }
#line 1646 "src/interpreter/bytecode-interpreter.dasc"
  //|  jmp ->InterpFail
  dasm_put(Dst, 497);
#line 1647 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_IDX_GETI:
  //|->InterpIdxGetI:
  //|  savepc
  //|  mov CARG1, RUNTIME
  //|  mov CARG2, qword [STK+ARG2F*8]
  //|  cvtsi2sd xmm0, ARG3
  //|  movd CARG3, xmm0
  //|  lea CARG4, qword [STK+ARG1F*8]
  //|  fcall InterpreterIdxGet
  dasm_put(Dst, 679,  INTERP_IDX_GETI, RuntimeLayout::kCurPCOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterIdxGet))) {
  dasm_put(Dst, 710);
   } else {
     lava_warn("%s","Function InterpreterIdxGet address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterIdxGet)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterIdxGet))>>32));
   }
#line 1657 "src/interpreter/bytecode-interpreter.dasc"
  //|  retbool
  dasm_put(Dst, 302);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 160);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 173);
#line 1658 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_IDX_SETI:
  //|->InterpIdxSetI:
  //|  savepc
  //|  mov CARG1, RUNTIME
  //|  mov CARG2, qword [STK+ARG1F*8]
  //|  cvtsi2sd xmm0, ARG2
  //|  movd CARG3, xmm0
  //|  mov CARG4, qword [STK+ARG3F*8]
  //|  fcall InterpreterIdxSet
  dasm_put(Dst, 715,  INTERP_IDX_SETI, RuntimeLayout::kCurPCOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterIdxSet))) {
  dasm_put(Dst, 746);
   } else {
     lava_warn("%s","Function InterpreterIdxSet address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterIdxSet)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterIdxSet))>>32));
   }
#line 1668 "src/interpreter/bytecode-interpreter.dasc"
  //|  retbool
  dasm_put(Dst, 302);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 160);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 173);
#line 1669 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_IDX_OUTOFBOUND_SET:
  //|->InterpIdxOutOfBoundSet:
  //|  savepc
  //|  mov CARG1, RUNTIME
  //|  mov CARG2, qword [STK+ARG2F*8]
  //|  mov CARG3L, ARG3
  //|  fcall InterpreterIdxOutOfBound
  dasm_put(Dst, 751,  INTERP_IDX_OUTOFBOUND_SET, RuntimeLayout::kCurPCOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterIdxOutOfBound))) {
  dasm_put(Dst, 770);
   } else {
     lava_warn("%s","Function InterpreterIdxOutOfBound address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterIdxOutOfBound)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterIdxOutOfBound))>>32));
   }
#line 1677 "src/interpreter/bytecode-interpreter.dasc"
  //|  jmp ->InterpFail
  dasm_put(Dst, 497);
#line 1678 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_IDX_OUTOFBOUND_GET:
  //|->InterpIdxOutOfBoundGet:
  //|  savepc
  //|  mov CARG1, RUNTIME
  //|  mov CARG2, qword [STK+ARG1F*8]
  //|  mov CARG3L, ARG2
  //|  fcall InterpreterIdxOutOfBound
  dasm_put(Dst, 775,  INTERP_IDX_OUTOFBOUND_GET, RuntimeLayout::kCurPCOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterIdxOutOfBound))) {
  dasm_put(Dst, 770);
   } else {
     lava_warn("%s","Function InterpreterIdxOutOfBound address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterIdxOutOfBound)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterIdxOutOfBound))>>32));
   }
#line 1686 "src/interpreter/bytecode-interpreter.dasc"
  //|  jmp ->InterpFail
  dasm_put(Dst, 497);
#line 1687 "src/interpreter/bytecode-interpreter.dasc"

  /* -------------------------------------------------
   * Call
   * ------------------------------------------------*/
  //|=> INTERP_CALL:
  //|->InterpCall:
  //|  savepc
  //|  mov CARG1, RUNTIME
  //|  mov CARG2, qword [STK+ARG2F*8]
  //|  mov CARG3L, ARG2
  //|  mov CARG4L, ARG3
  //|  xor CARG5L, CARG5L
  //|  fcall InterpreterCall
  dasm_put(Dst, 794,  INTERP_CALL, RuntimeLayout::kCurPCOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterCall))) {
  dasm_put(Dst, 818);
   } else {
     lava_warn("%s","Function InterpreterCall address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterCall)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterCall))>>32));
   }
#line 1700 "src/interpreter/bytecode-interpreter.dasc"
  //|  retbool
  dasm_put(Dst, 302);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 160);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 173);
#line 1701 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_TCALL:
  //|->InterpTCall:
  //|  savepc
  //|  mov CARG1, RUNTIME
  //|  mov CARG2, qword [STK+ARG2F*8]
  //|  mov CARG3L,ARG2
  //|  mov CARG4L,ARG3
  //|  mov CARG5L,1
  //|  fcall InterpreterCall
  dasm_put(Dst, 823,  INTERP_TCALL, RuntimeLayout::kCurPCOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterCall))) {
  dasm_put(Dst, 818);
   } else {
     lava_warn("%s","Function InterpreterCall address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterCall)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterCall))>>32));
   }
#line 1711 "src/interpreter/bytecode-interpreter.dasc"
  //|  retbool
  dasm_put(Dst, 302);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
  dasm_put(Dst, 160);
   } else {
     lava_warn("%s","Function PrintOP address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
   }
  dasm_put(Dst, 173);
#line 1712 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_NEEDOBJECT:
  //|->InterpNeedObject:
  //|  savepc
  //|  mov CARG1, RUNTIME
  //|  mov CARG2, qword [STK+ARG1F*8]
  //|  fcall InterpreterCallNeedObject
  dasm_put(Dst, 850,  INTERP_NEEDOBJECT, RuntimeLayout::kCurPCOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterCallNeedObject))) {
  dasm_put(Dst, 867);
   } else {
     lava_warn("%s","Function InterpreterCallNeedObject address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterCallNeedObject)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterCallNeedObject))>>32));
   }
#line 1719 "src/interpreter/bytecode-interpreter.dasc"
  //|  jmp ->InterpFail
  dasm_put(Dst, 497);
#line 1720 "src/interpreter/bytecode-interpreter.dasc"

  //|=> INTERP_ARGUMENTMISMATCH:
  //|->InterpArgumentMismatch:
  //|  savepc
  //|  mov CARG1, RUNTIME
  //|  mov CARG2, qword [STK+ARG1F*8]
  //|  mov CARG3L,ARG3
  //|  fcall InterpreterArgumentMismatch
  dasm_put(Dst, 872,  INTERP_ARGUMENTMISMATCH, RuntimeLayout::kCurPCOffset);
   if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterArgumentMismatch))) {
  dasm_put(Dst, 891);
   } else {
     lava_warn("%s","Function InterpreterArgumentMismatch address is not in 0-2GB");
  dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterArgumentMismatch)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterArgumentMismatch))>>32));
   }
#line 1728 "src/interpreter/bytecode-interpreter.dasc"
  //|  jmp ->InterpFail
  dasm_put(Dst, 497);
#line 1729 "src/interpreter/bytecode-interpreter.dasc"


  // ------------------------------------------------------
  // SSO Hash Lookup Fast Path
  // ------------------------------------------------------

  // This small assembly routine is used to do a key/value lookup inside
  // of a Object/Map. This function is roughly the same as doing an normal
  // open addressing chain resolution when key is SSO inside of Map object.

  // assume objreg is type Map* , pointer to a *Map*
  // assume ssoref is type SSO* , pointer to a *SSO*
  // returned slot/entry is in RREG
  //|.macro objfind_sso,objreg,ssoreg,not_found,found
  //|  mov RREGL, dword [ssoreg+SSOLayout::kHashOffset]     // get the sso hash value
  //|  and RREGL, dword [objreg+MapLayout::kMaskOffset]     // do the masking

  // Assuming Entry inside of Map is 24 bytes , 3 machine word
  //|  lea LREG , [objreg+MapLayout::kArrayOffset]          // Store the Entry's start address
  //|  lea RREG , [RREG+RREG*2]                             // RREG * 3
  //|  lea RREG , [LREG+RREG*8]                             // RREG = [start_of_address+LREG*24]

  // check if entry is *deleted* or *used*
  //|2:
  //|  mov  T1L, dword [RREG+MapEntryLayout::kFlagOffset]
  //| // start the chain resolution loop
  //|  test T1L, (Map::Entry::kUseButNotDelBit)
  //|  jz not_found  // not found

  //|  mov T0, qword [RREG+MapEntryLayout::kKeyOffset]     // Get the key
  //|  CheckSSO T0, >3

  //|  cmp ssoreg, T0
  //|  jne >3
  // we find our key here , RREG points to the entry
  //|  found
  //|3: // next iteration
  //|  test T1L, (Map::Entry::kMoreBit)
  //|  jz not_found // not found
  //|  and T1L , (bits::BitOn<std::uint32_t,0,29>::value)
  //|  lea T0  , [T1+T1*2]
  //|  lea RREG, [LREG+T0*8]
  //|  jmp <2
  //|.endmacro

}

void GenerateOneBytecode( BuildContext* bctx, Bytecode bc ) {
  switch(bc) {
    /** =====================================================
     *  Register Move                                       |
     *  ====================================================*/
    case BC_MOVE:
      //|=> bc:
      //|  instr_E
      //|  mov ARG3F,qword [STK+ARG2F*8]
      //|  mov qword [STK+ARG1F*8],ARG3F
      //|  Dispatch
      dasm_put(Dst, 896,  bc);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 1787 "src/interpreter/bytecode-interpreter.dasc"
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
      dasm_put(Dst, 922,  bc);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 1798 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_LOAD1:
      //|=> bc:
      //|  instr_F
      //|  rconst_one xmm0
      //|  movsd qword[STK+ARG1F*8], xmm0
      //|  Dispatch
      dasm_put(Dst, 948,  bc, (unsigned int)((static_cast<std::uint64_t>(0x3ff00000)<<32)), (unsigned int)(((static_cast<std::uint64_t>(0x3ff00000)<<32))>>32));
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 1806 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_LOADN1:
      //|=> bc:
      //|  instr_F
      //|  rconst_neg_one xmm0
      //|  movsd qword[STK+ARG1F*8], xmm0
      //|  Dispatch
      dasm_put(Dst, 948,  bc, (unsigned int)((static_cast<std::uint64_t>(0xbff00000)<<32)), (unsigned int)(((static_cast<std::uint64_t>(0xbff00000)<<32))>>32));
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 1814 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_LOADR:
      //|=> bc:
      //|  instr_E
      //|  LdReal xmm0,ARG2F
      //|  movsd qword [STK+ARG1F*8],xmm0
      //|  Dispatch
      dasm_put(Dst, 979,  bc, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 1822 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_LOADNULL:
      //|=> bc:
      //|  instr_F
      //|  mov dword [STK+ARG1F*8+4],Value::FLAG_NULL
      //|  Dispatch
      dasm_put(Dst, 1017,  bc, Value::FLAG_NULL);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 1829 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_LOADTRUE:
      //|=> bc:
      //|  instr_F
      //|  mov dword [STK+ARG1F*8+4],Value::FLAG_TRUE
      //|  Dispatch
      dasm_put(Dst, 1017,  bc, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 1836 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_LOADFALSE:
      //|=> bc:
      //|  instr_F
      //|  mov dword [STK+ARG1F*8+4],Value::FLAG_FALSE
      //|  Dispatch
      dasm_put(Dst, 1017,  bc, Value::FLAG_FALSE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 1843 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_LOADSTR:
      //|=> bc:
      //|  instr_E
      //|  LdStrV LREG,ARG2F
      //|  mov qword [STK+ARG1F*8],LREG
      //|  Dispatch
      dasm_put(Dst, 1038,  bc, PrototypeLayout::kStringTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 1851 "src/interpreter/bytecode-interpreter.dasc"
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
      //|  fcall InterpreterLoadList0
      dasm_put(Dst, 1077,  bc, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterLoadList0))) {
      dasm_put(Dst, 1095);
       } else {
         lava_warn("%s","Function InterpreterLoadList0 address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterLoadList0)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterLoadList0))>>32));
       }
#line 1877 "src/interpreter/bytecode-interpreter.dasc"
      //|  Dispatch
      dasm_put(Dst, 149);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 1878 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_LOADLIST1:
      //|=>bc:
      //|  instr_E
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  lea CARG2, [STK+ARG1F*8]
      //|  lea CARG3, [STK+ARG2F*8]
      //|  fcall InterpreterLoadList1
      dasm_put(Dst, 1100, bc, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterLoadList1))) {
      dasm_put(Dst, 1125);
       } else {
         lava_warn("%s","Function InterpreterLoadList1 address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterLoadList1)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterLoadList1))>>32));
       }
#line 1887 "src/interpreter/bytecode-interpreter.dasc"
      //|  Dispatch
      dasm_put(Dst, 149);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 1888 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_LOADLIST2:
      //|=>bc:
      //|  instr_D
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  lea CARG2, [STK+ARG1F*8]
      //|  lea CARG3, [STK+ARG2F*8]
      //|  lea CARG4, [STK+ARG3F*8]
      //|  fcall InterpreterLoadList2
      dasm_put(Dst, 1130, bc, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterLoadList2))) {
      dasm_put(Dst, 1169);
       } else {
         lava_warn("%s","Function InterpreterLoadList2 address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterLoadList2)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterLoadList2))>>32));
       }
#line 1898 "src/interpreter/bytecode-interpreter.dasc"
      //|  Dispatch
      dasm_put(Dst, 149);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 1899 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_NEWLIST:
      //|=>bc:
      //|  instr_B
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  lea CARG2, [STK+ARG1F*8]
      //|  mov CARG3L, ARG2
      //|  fcall InterpreterNewList
      dasm_put(Dst, 1174, bc, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterNewList))) {
      dasm_put(Dst, 1198);
       } else {
         lava_warn("%s","Function InterpreterNewList address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterNewList)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterNewList))>>32));
       }
#line 1908 "src/interpreter/bytecode-interpreter.dasc"
      //|  Dispatch
      dasm_put(Dst, 149);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 1909 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_ADDLIST:
      //|=>bc:
      //|  instr_D
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  lea CARG2, [STK+ARG1F*8]
      //|  mov CARG3L, ARG2
      //|  mov CARG4L, ARG3
      //|  fcall InterpreterAddList
      dasm_put(Dst, 1203, bc, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterAddList))) {
      dasm_put(Dst, 1238);
       } else {
         lava_warn("%s","Function InterpreterAddList address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterAddList)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterAddList))>>32));
       }
#line 1919 "src/interpreter/bytecode-interpreter.dasc"
      //|  Dispatch
      dasm_put(Dst, 149);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 1920 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_LOADOBJ0:
      //|=>bc:
      //|  instr_F
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  lea CARG2, [STK+ARG1F*8]
      //|  fcall InterpreterLoadObj0
      dasm_put(Dst, 1077, bc, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterLoadObj0))) {
      dasm_put(Dst, 1243);
       } else {
         lava_warn("%s","Function InterpreterLoadObj0 address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterLoadObj0)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterLoadObj0))>>32));
       }
#line 1928 "src/interpreter/bytecode-interpreter.dasc"
      //|  Dispatch
      dasm_put(Dst, 149);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 1929 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_LOADOBJ1:
      //|=>bc:
      //|  instr_D
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  lea CARG2, [STK+ARG1F*8]
      //|  lea CARG3, [STK+ARG2F*8]
      //|  lea CARG4, [STK+ARG3F*8]
      //|  fcall InterpreterLoadObj1
      dasm_put(Dst, 1130, bc, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterLoadObj1))) {
      dasm_put(Dst, 1248);
       } else {
         lava_warn("%s","Function InterpreterLoadObj1 address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterLoadObj1)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterLoadObj1))>>32));
       }
#line 1939 "src/interpreter/bytecode-interpreter.dasc"
      //|  retbool
      dasm_put(Dst, 302);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 1940 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_NEWOBJ:
      //|=>bc:
      //|  instr_B
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  lea CARG2, [STK+ARG1F*8]
      //|  mov CARG3L, ARG2
      //|  fcall InterpreterNewObj
      dasm_put(Dst, 1174, bc, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterNewObj))) {
      dasm_put(Dst, 1253);
       } else {
         lava_warn("%s","Function InterpreterNewObj address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterNewObj)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterNewObj))>>32));
       }
#line 1949 "src/interpreter/bytecode-interpreter.dasc"
      //|  Dispatch
      dasm_put(Dst, 149);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 1950 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_ADDOBJ:
      //|=>bc:
      //|  instr_D
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  lea CARG2, [STK+ARG1F*8]
      //|  lea CARG3, [STK+ARG2F*8]
      //|  lea CARG4, [STK+ARG3F*8]
      //|  fcall InterpreterAddObj
      dasm_put(Dst, 1130, bc, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterAddObj))) {
      dasm_put(Dst, 1258);
       } else {
         lava_warn("%s","Function InterpreterAddObj address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterAddObj)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterAddObj))>>32));
       }
#line 1960 "src/interpreter/bytecode-interpreter.dasc"
      //|  retbool
      dasm_put(Dst, 302);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 1961 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_LOADCLS:
      //|=>bc:
      //|  instr_C
      //|  savepc
      //|  mov CARG1 , RUNTIME
      //|  mov CARG2L, ARG2
      //|  lea CARG3 , [STK+ARG1F*8]
      //|  fcall InterpreterLoadCls
      dasm_put(Dst, 1263, bc, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterLoadCls))) {
      dasm_put(Dst, 1287);
       } else {
         lava_warn("%s","Function InterpreterLoadCls address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterLoadCls)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterLoadCls))>>32));
       }
#line 1970 "src/interpreter/bytecode-interpreter.dasc"
      //|  Dispatch
      dasm_put(Dst, 149);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 1971 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_INITCLS:
      //|=>bc:
      //|  instr_G
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  mov CARG2L, ARG1
      //|  fcall InterpreterInitCls
      dasm_put(Dst, 1292, bc, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterInitCls))) {
      dasm_put(Dst, 1309);
       } else {
         lava_warn("%s","Function InterpreterInitCls address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterInitCls)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterInitCls))>>32));
       }
#line 1979 "src/interpreter/bytecode-interpreter.dasc"
      //|  retbool
      dasm_put(Dst, 302);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 1980 "src/interpreter/bytecode-interpreter.dasc"
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
      dasm_put(Dst, 1314, bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2010 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_SUBRV:
      //|=>bc:
      //|  arith_rv BC_SUBRV,InterpArithRealL,subsd
      dasm_put(Dst, 1392, bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2015 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_MULRV:
      //|=>bc:
      //|  arith_rv BC_MULRV,InterpArithRealL,mulsd
      dasm_put(Dst, 1470, bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2020 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_DIVRV:
      //|=>bc:
      //|  arith_rv BC_DIVRV,InterpArithRealL,divsd
      dasm_put(Dst, 1548, bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2025 "src/interpreter/bytecode-interpreter.dasc"
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
      dasm_put(Dst, 1626,  bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2055 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_SUBVR:
      //|=> bc:
      //|  arith_vr BC_SUBVR,InterpArithRealR,subsd
      dasm_put(Dst, 1705,  bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2060 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_MULVR:
      //|=> bc:
      //|  arith_vr BC_MULVR,InterpArithRealR,mulsd
      dasm_put(Dst, 1784,  bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2065 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_DIVVR:
      //|=> bc:
      //|  arith_vr BC_DIVVR,InterpArithRealR,divsd
      dasm_put(Dst, 1863,  bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2070 "src/interpreter/bytecode-interpreter.dasc"
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
    //|  cmp dword [STK+ARG2F*8+4], Value::FLAG_REAL
    //|  jnb ->InterpArithVV

    //| // real && xx
    //|  cmp dword [STK+ARG3F*8+4],Value::FLAG_REAL
    //|  jnb ->InterpArithVV

    //|  movsd xmm0, qword [STK+ARG2F*8]
    //|  instrR xmm0, qword [STK+ARG3F*8]
    //|  StReal ARG1F,xmm0
    //|  Dispatch
    //|.endmacro

    case BC_ADDVV:
      //|  arith_vv BC_ADDVV,addsd
      dasm_put(Dst, 1942,  BC_ADDVV, Value::FLAG_REAL, Value::FLAG_REAL);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2102 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_SUBVV:
      //|  arith_vv BC_SUBVV,subsd
      dasm_put(Dst, 2013,  BC_SUBVV, Value::FLAG_REAL, Value::FLAG_REAL);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2105 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_MULVV:
      //|  arith_vv BC_MULVV,mulsd
      dasm_put(Dst, 2084,  BC_MULVV, Value::FLAG_REAL, Value::FLAG_REAL);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2108 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_DIVVV:
      //|  arith_vv BC_DIVVV,divsd
      dasm_put(Dst, 2155,  BC_DIVVV, Value::FLAG_REAL, Value::FLAG_REAL);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2111 "src/interpreter/bytecode-interpreter.dasc"
      break;

    /* =============================================================
     * MODXX
     *
     *   Similar implementation like Lua not Luajit. Return casted
     *   integer's mod value instead of fmod style value
     *
     * ============================================================*/


    case BC_MODVR:
      //|=>bc:
      //|  instr_D
      dasm_put(Dst, 2226, bc);
#line 2125 "src/interpreter/bytecode-interpreter.dasc"

      //|  cmp dword [STK+ARG2F*8+4], Value::FLAG_REAL
      //|  jnb ->InterpArithVV
      dasm_put(Dst, 2244, Value::FLAG_REAL);
#line 2128 "src/interpreter/bytecode-interpreter.dasc"

      //|  LdReal2Int ARG3,ARG3F,T0
      //|  cvtsd2si eax ,qword [STK+ARG2F*8]  // ARG2F == rax
      dasm_put(Dst, 2256, PrototypeLayout::kRealTableOffset);
#line 2131 "src/interpreter/bytecode-interpreter.dasc"

      //|.if CHECK_MOD_BY_ZERO
      //|  test ARG3,ARG3
      //|  je ->ModByZero
      //|.endif
      dasm_put(Dst, 2276);
#line 2136 "src/interpreter/bytecode-interpreter.dasc"

      //|  cdq
      //|  idiv     ARG3
      //|  StRealFromInt ARG1F,edx
      //|  Dispatch
      dasm_put(Dst, 2283);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2141 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_MODRV:
      //|=>bc:
      //|  instr_D
      dasm_put(Dst, 2226, bc);
#line 2146 "src/interpreter/bytecode-interpreter.dasc"

      //|  cmp dword [STK+ARG3F*8+4], Value::FLAG_REAL
      //|  jnb ->InterpArithVV
      dasm_put(Dst, 2311, Value::FLAG_REAL);
#line 2149 "src/interpreter/bytecode-interpreter.dasc"

      //|  LdReal2Int eax,ARG2F,T0  // ARG2F == rax
      //|  cvtsd2si ARG3 ,qword [STK+ARG3F*8]
      dasm_put(Dst, 2323, PrototypeLayout::kRealTableOffset);
#line 2152 "src/interpreter/bytecode-interpreter.dasc"

      //|.if CHECK_MOD_BY_ZERO
      //|  test ARG3,ARG3
      //|  je ->ModByZero
      //|.endif
      dasm_put(Dst, 2276);
#line 2157 "src/interpreter/bytecode-interpreter.dasc"

      //|  cdq
      //|  idiv ARG3
      //|  StRealFromInt ARG1F,edx
      //|  Dispatch
      dasm_put(Dst, 2283);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2162 "src/interpreter/bytecode-interpreter.dasc"

      break;

    case BC_MODVV:
      //|=>bc :
      //|  instr_D
      //|  cmp dword [STK+ARG2F*8+4] , Value::FLAG_REAL
      //|  jnb ->InterpArithVV
      dasm_put(Dst, 2343, bc , Value::FLAG_REAL);
#line 2170 "src/interpreter/bytecode-interpreter.dasc"

      //|  cmp dword [STK+ARG3F*8+4] , Value::FLAG_REAL
      //|  jnb ->InterpArithVV
      dasm_put(Dst, 2311, Value::FLAG_REAL);
#line 2173 "src/interpreter/bytecode-interpreter.dasc"

      //|  cvtsd2si eax, qword [STK+ARG2F*8]  // ARG2F == rax
      //|  cvtsd2si ARG3,qword [STK+ARG3F*8]
      dasm_put(Dst, 2372);
#line 2176 "src/interpreter/bytecode-interpreter.dasc"

      //|.if CHECK_MOD_BY_ZERO
      //|  test ARG3,ARG3
      //|  je ->ModByZero
      //|.endif
      dasm_put(Dst, 2276);
#line 2181 "src/interpreter/bytecode-interpreter.dasc"

      //|  cdq
      //|  idiv ARG3
      //|  StRealFromInt ARG1F,edx
      //|  Dispatch
      dasm_put(Dst, 2283);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2186 "src/interpreter/bytecode-interpreter.dasc"
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
      dasm_put(Dst, 2387,  bc, PrototypeLayout::kRealTableOffset, Value::FLAG_REAL);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(pow))) {
      dasm_put(Dst, 2438);
       } else {
         lava_warn("%s","Function pow address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(pow)), (unsigned int)((reinterpret_cast<std::uintptr_t>(pow))>>32));
       }
      dasm_put(Dst, 930);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2215 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_POWVR:
      //|=> bc:
      //|  instr_D
      //|  LdReal xmm1,ARG3F
      //|  arith_pow LREGL,xmm0,ARG2F,InterpPowSlowVR
      dasm_put(Dst, 2443,  bc, PrototypeLayout::kRealTableOffset, Value::FLAG_REAL);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(pow))) {
      dasm_put(Dst, 2438);
       } else {
         lava_warn("%s","Function pow address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(pow)), (unsigned int)((reinterpret_cast<std::uintptr_t>(pow))>>32));
       }
      dasm_put(Dst, 930);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2222 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_POWVV:
      //|=> bc:
      //|  jmp ->InterpPowSlowVV
      dasm_put(Dst, 2494,  bc);
#line 2227 "src/interpreter/bytecode-interpreter.dasc"
      break;


    /* ====================================================================
     * Comparison
     *
     * Inline numeric comparison
     * ===================================================================*/

    /* --------------------------------------------------------------------
     * Comparison XV                                                      |
     * -------------------------------------------------------------------*/
    //|.macro comp_xv,BC,slow_path,false_jmp
    //|  instr_D

    //|  cmp dword [STK+ARG3F*8+4], Value::FLAG_REAL
    //|  jnb ->slow_path

    //|  LdReal xmm0, ARG2F
    //|  ucomisd xmm0, qword [STK+ARG3F*8]
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
      dasm_put(Dst, 2500, bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2570, Value::FLAG_FALSE);
#line 2260 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_LERV:
      //|=> bc:
      //|  comp_xv BC_LERV,InterpCompareRV,ja
      dasm_put(Dst, 2602,  bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2570, Value::FLAG_FALSE);
#line 2264 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_GTRV:
      //|=>bc:
      //|  comp_xv BC_GTRV,InterpCompareRV,jbe
      dasm_put(Dst, 2672, bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2570, Value::FLAG_FALSE);
#line 2268 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_GERV:
      //|=> bc:
      //|  comp_xv BC_GERV,InterpCompareRV,jb
      dasm_put(Dst, 2742,  bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2570, Value::FLAG_FALSE);
#line 2272 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_EQRV:
      //|=> bc:
      //|  comp_xv BC_EQRV,InterpCompareRV,jne
      dasm_put(Dst, 2812,  bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2570, Value::FLAG_FALSE);
#line 2276 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_NERV:
      //|=> bc:
      //|  comp_xv BC_NERV,InterpCompareRV,je
      dasm_put(Dst, 2882,  bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2570, Value::FLAG_FALSE);
#line 2280 "src/interpreter/bytecode-interpreter.dasc"
      break;

    /* --------------------------------------------------------------------
     * Comparison VX                                                      |
     * -------------------------------------------------------------------*/
    //|.macro comp_vx,BC,slow_path,false_jmp
    //|  instr_D

    //|  cmp dword [STK+ARG2F*8+4], Value::FLAG_REAL
    //|  jnb ->slow_path

    //|  LdReal xmm1,ARG3F
    //|  movsd xmm0, qword [STK+ARG2F*8]
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
      dasm_put(Dst, 2952, bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2570, Value::FLAG_FALSE);
#line 2306 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_LEVR:
      //|=>bc:
      //|  comp_vx BC_LEVR,InterpCompareVR,ja
      dasm_put(Dst, 3027, bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2570, Value::FLAG_FALSE);
#line 2310 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_GTVR:
      //|=>bc:
      //|  comp_vx BC_GTVR,InterpCompareVR,jbe
      dasm_put(Dst, 3102, bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2570, Value::FLAG_FALSE);
#line 2314 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_GEVR:
      //|=>bc:
      //|  comp_vx BC_GEVR,InterpCompareVR,jb
      dasm_put(Dst, 3177, bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2570, Value::FLAG_FALSE);
#line 2318 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_EQVR:
      //|=>bc:
      //|  comp_vx BC_EQVR,InterpCompareVR,jne
      dasm_put(Dst, 3252, bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2570, Value::FLAG_FALSE);
#line 2322 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_NEVR:
      //|=>bc:
      //|  comp_vx BC_NEVR,InterpCompareVR,je
      dasm_put(Dst, 3327, bc, Value::FLAG_REAL, PrototypeLayout::kRealTableOffset, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2570, Value::FLAG_FALSE);
#line 2326 "src/interpreter/bytecode-interpreter.dasc"
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
      dasm_put(Dst, 3402, bc, Value::FLAG_REAL, Value::FLAG_REAL, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2570, Value::FLAG_FALSE);
#line 2358 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_LEVV:
      //|=>bc:
      //|  comp_vv,BC_LEVV,ja
      dasm_put(Dst, 3477, bc, Value::FLAG_REAL, Value::FLAG_REAL, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2570, Value::FLAG_FALSE);
#line 2362 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_GTVV:
      //|=>bc:
      //|  comp_vv,BC_GTVV,jbe
      dasm_put(Dst, 3552, bc, Value::FLAG_REAL, Value::FLAG_REAL, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2570, Value::FLAG_FALSE);
#line 2366 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_GEVV:
      //|=>bc:
      //|  comp_vv,BC_GEVV,jb
      dasm_put(Dst, 3627, bc, Value::FLAG_REAL, Value::FLAG_REAL, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 2570, Value::FLAG_FALSE);
#line 2370 "src/interpreter/bytecode-interpreter.dasc"
      break;

    //|.macro comp_eqne_vv,BC,T,F
    //|  instr_D

    // We fast check numeric number's value. Pay attension that bit
    // comparison is not okay due to the fact we have +0 and -0
    //|  cmp dword [STK+ARG2F*8+4], Value::FLAG_REAL
    //|  jnb >3

    //|  cmp dword [STK+ARG3F*8+4], Value::FLAG_REAL
    //|  jnb >3

    //|  movsd xmm0, qword [STK+ARG2F*8]
    //|  ucomisd xmm0, qword [STK+ARG3F*8]
    //|  jne >1
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
    //|  jne >4

    // LREG and RREG doesn't match, we need to rule out heap type to
    // actually tell whether LREGL and RREGL are the same or not
    //|  cmp LREGL, Value::FLAG_HEAP
    //|  je >5
    //|  cmp RREGL, Value::FLAG_HEAP
    //|  je >5

    // Okay, both LREGL an RREGL are not heap tag, so we can assert
    // they are equal due to they are primitive type
    //|  mov dword [STK+ARG1F*8+4], T
    //|  jmp <2

    // Primitive type are definitly not equal since they have different
    // type, so we just return *FALSE*
    //|4:
    //|  mov dword [STK+ARG1F*8+4], F
    //|  jmp <2

    // When we reach 5, we know at least one of the operand is a *HEAP*
    // object. We can try to inline a SSO check here or just go back to
    // InterpCompareVV to do the job
    //|5:
    //|  CheckSSORaw T0,>7
    //|  CheckSSORaw T1,>7
    //|  cmp T0,T1
    //|  jne >6
    //|  mov dword [STK+ARG1F*8+4], T
    //|6:
    //|  mov dword [STK+ARG1F*8+4], F
    //|  jmp <2

    // Calls into InterpreterCompare
    //|7:
    //|  // T0/T1 stores HeapObject*
    //|  savepc
    //|  mov CARG1, RUNTIME
    //|  lea CARG2, [STK+ARG2F*8]
    //|  lea CARG3, [STK+ARG3F*8]
    //|  lea CARG4, [STK+ARG1F*8] // where to set the true/false
    //|  fcall InterpreterCompare
    //|  test eax,eax
    //|  je ->InterpFail
    //|  Dispatch
    //|.endmacro


    case BC_EQVV:
      //|=>bc:
      //|  comp_eqne_vv BC_EQVV,Value::FLAG_TRUE,Value::FLAG_FALSE
      dasm_put(Dst, 3702, bc, Value::FLAG_REAL, Value::FLAG_REAL, Value::FLAG_TRUE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 3777, Value::FLAG_FALSE, Value::FLAG_HEAP, Value::FLAG_HEAP, Value::FLAG_TRUE, Value::FLAG_FALSE);
      dasm_put(Dst, 3883, -HOH_TYPE_OFFSET, SSO_BIT_PATTERN, -HOH_TYPE_OFFSET, SSO_BIT_PATTERN, Value::FLAG_TRUE, Value::FLAG_FALSE);
      dasm_put(Dst, 360, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterCompare))) {
      dasm_put(Dst, 524);
       } else {
         lava_warn("%s","Function InterpreterCompare address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterCompare)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterCompare))>>32));
       }
      dasm_put(Dst, 302);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2454 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_NEVV:
      //|=>bc:
      //|  comp_eqne_vv BC_NEVV,Value::FLAG_FALSE,Value::FLAG_TRUE
      dasm_put(Dst, 3702, bc, Value::FLAG_REAL, Value::FLAG_REAL, Value::FLAG_FALSE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 3777, Value::FLAG_TRUE, Value::FLAG_HEAP, Value::FLAG_HEAP, Value::FLAG_FALSE, Value::FLAG_TRUE);
      dasm_put(Dst, 3883, -HOH_TYPE_OFFSET, SSO_BIT_PATTERN, -HOH_TYPE_OFFSET, SSO_BIT_PATTERN, Value::FLAG_FALSE, Value::FLAG_TRUE);
      dasm_put(Dst, 360, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterCompare))) {
      dasm_put(Dst, 524);
       } else {
         lava_warn("%s","Function InterpreterCompare address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterCompare)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterCompare))>>32));
       }
      dasm_put(Dst, 302);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2458 "src/interpreter/bytecode-interpreter.dasc"
      break;

    // For string equality comparison , we inline SSO comparison since
    // they are just checking the address are equal or not
    //|.macro eq_sv,BC,SlowPath,instr,false_jmp
    //|  instr_D
    //|  LdStr LREG,ARG2F
    //|  mov RREG,qword [STK+ARG3F*8]
    //|  CheckSSO LREG,>1
    //|  CheckSSOV RREG,>1

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
    //|  CheckSSOV LREG,>1
    //|  CheckSSO  RREG,>1

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
      dasm_put(Dst, 3950,  bc, PrototypeLayout::kStringTableOffset, -HOH_TYPE_OFFSET, SSO_BIT_PATTERN, Value::FLAG_HEAP, -HOH_TYPE_OFFSET, SSO_BIT_PATTERN, Value::FLAG_TRUE);
      dasm_put(Dst, 4053, Value::FLAG_FALSE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 4072);
#line 2521 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_EQVS:
      //|=> bc:
      //|  eq_vs BC_EQVS,InterpCompareVS,cmove,jne
      dasm_put(Dst, 4098,  bc, PrototypeLayout::kStringTableOffset, Value::FLAG_HEAP, -HOH_TYPE_OFFSET, SSO_BIT_PATTERN, -HOH_TYPE_OFFSET, SSO_BIT_PATTERN, Value::FLAG_TRUE);
      dasm_put(Dst, 4053, Value::FLAG_FALSE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 4202);
#line 2525 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_NESV:
      //|=>bc:
      //|  eq_sv BC_NESV,InterpCompareSV,cmovne,je
      dasm_put(Dst, 4228, bc, PrototypeLayout::kStringTableOffset, -HOH_TYPE_OFFSET, SSO_BIT_PATTERN, Value::FLAG_HEAP, -HOH_TYPE_OFFSET, SSO_BIT_PATTERN, Value::FLAG_TRUE);
      dasm_put(Dst, 4053, Value::FLAG_FALSE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 4072);
#line 2529 "src/interpreter/bytecode-interpreter.dasc"
      break;
    case BC_NEVS:
      //|=>bc:
      //|  eq_vs BC_NEVS,InterpCompareVS,cmovne,je
      dasm_put(Dst, 4331, bc, PrototypeLayout::kStringTableOffset, Value::FLAG_HEAP, -HOH_TYPE_OFFSET, SSO_BIT_PATTERN, -HOH_TYPE_OFFSET, SSO_BIT_PATTERN, Value::FLAG_TRUE);
      dasm_put(Dst, 4053, Value::FLAG_FALSE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 4202);
#line 2533 "src/interpreter/bytecode-interpreter.dasc"
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
      dasm_put(Dst, 4435,  bc, Value::FLAG_REAL);
#line 2548 "src/interpreter/bytecode-interpreter.dasc"

      //|  movsd, xmm0, qword [STK+ARG2F*8]
      //|  rconst_sign xmm1
      //|  xorpd xmm0, xmm1
      //|  movsd qword [STK+ARG1F*8], xmm0
      //|  Dispatch
      dasm_put(Dst, 4454, (unsigned int)((static_cast<std::uint64_t>(0x80000000)<<32)), (unsigned int)(((static_cast<std::uint64_t>(0x80000000)<<32))>>32));
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2554 "src/interpreter/bytecode-interpreter.dasc"

      //|8:
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  mov CARG2, qword [STK+ARG2F*8]
      //|  fcall InterpreterNegateFail
      dasm_put(Dst, 4492, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterNegateFail))) {
      dasm_put(Dst, 4508);
       } else {
         lava_warn("%s","Function InterpreterNegateFail address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterNegateFail)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterNegateFail))>>32));
       }
#line 2560 "src/interpreter/bytecode-interpreter.dasc"
      //|  jmp ->InterpFail
      dasm_put(Dst, 497);
#line 2561 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_NOT:
      //|=> bc:
      //|  instr_E
      //|  mov ARG3, Value::FLAG_FALSE
      dasm_put(Dst, 4513,  bc, Value::FLAG_FALSE);
#line 2567 "src/interpreter/bytecode-interpreter.dasc"
      // check if the value is a heap object
      //|  cmp word [STK+ARG2F*8+6], Value::FLAG_HEAP
      //|  je >1
      //|  cmp dword [STK+ARG2F*8+4], Value::FLAG_FALSECOND
      //|  cmova ARG3, dword [->FlagTrueConst]
      //|1:
      //|  mov dword [STK+ARG1F*8+4], ARG3
      //|  Dispatch
      dasm_put(Dst, 4523, Value::FLAG_HEAP, Value::FLAG_FALSECOND);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2575 "src/interpreter/bytecode-interpreter.dasc"
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
      dasm_put(Dst, 4565, bc, Value::FLAG_HEAP, Value::FLAG_FALSECOND);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2598 "src/interpreter/bytecode-interpreter.dasc"
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
      dasm_put(Dst, 4619, bc, Value::FLAG_HEAP, Value::FLAG_FALSECOND);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2610 "src/interpreter/bytecode-interpreter.dasc"
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
      dasm_put(Dst, 4671, bc, Value::FLAG_HEAP, Value::FLAG_FALSECOND);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2622 "src/interpreter/bytecode-interpreter.dasc"
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
      dasm_put(Dst, 4565, bc, Value::FLAG_HEAP, Value::FLAG_FALSECOND);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2635 "src/interpreter/bytecode-interpreter.dasc"
      break;

    //|.macro absolute_jmp,BC
    //|=>BC:
    //|  instr_G
    //|  branch_to ARG1F,ARG3F
    //|  Dispatch
    //|.endmacro

    case BC_JMP:
      //|  absolute_jmp,BC_JMP
      dasm_put(Dst, 4723, BC_JMP);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2646 "src/interpreter/bytecode-interpreter.dasc"
      break;

    // ----------------------------------------------------------
    // Property/Index
    // ---------------------------------------------------------*/


    case BC_PROPGETSSO:
      //|.macro getsso_found
      //|  mov T0, qword [RREG+MapEntryLayout::kValueOffset]
      //|  mov qword [STK+ARG1F*8], T0
      //|  Dispatch
      //|.endmacro

      //|=>bc:
      //|  instr_D
      dasm_put(Dst, 2226, bc);
#line 2662 "src/interpreter/bytecode-interpreter.dasc"
      // Check ARG2F points to a *Object*
      //|  cmp word [STK+ARG2F*8+6], Value::FLAG_HEAP
      //|  jne ->InterpPropNeedObject
      dasm_put(Dst, 4747, Value::FLAG_HEAP);
#line 2665 "src/interpreter/bytecode-interpreter.dasc"

      //|  lea  CARG2, [STK+ARG2F*8]
      //|  mov ARG2F, qword [STK+ARG2F*8]
      //|  CheckObj ARG2F, >9
      dasm_put(Dst, 4760, -HOH_TYPE_OFFSET, OBJECT_BIT_PATTERN);
#line 2669 "src/interpreter/bytecode-interpreter.dasc"

      // Load *Map* object into ARG2F
      //|  mov ARG2F, qword [ARG2F+ObjectLayout::kMapOffset]
      //|  mov ARG2F, qword [ARG2F]
      dasm_put(Dst, 4785, ObjectLayout::kMapOffset);
#line 2673 "src/interpreter/bytecode-interpreter.dasc"

      // Load SSO/key into ARG3F
      //|  LdSSO ARG3F,ARG3F,T0
      dasm_put(Dst, 4793, PrototypeLayout::kSSOTableOffset);
#line 2676 "src/interpreter/bytecode-interpreter.dasc"

      // Do the search
      //|  objfind_sso ARG2F,ARG3F,>8,getsso_found
      dasm_put(Dst, 4810, SSOLayout::kHashOffset, MapLayout::kMaskOffset, MapLayout::kArrayOffset, MapEntryLayout::kFlagOffset, (Map::Entry::kUseButNotDelBit), MapEntryLayout::kKeyOffset, -HOH_TYPE_OFFSET, SSO_BIT_PATTERN, MapEntryLayout::kValueOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 4888, (Map::Entry::kMoreBit), (bits::BitOn<std::uint32_t,0,29>::value));
#line 2679 "src/interpreter/bytecode-interpreter.dasc"

      //|8: // not fonud label
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  mov CARG2, ARG3F
      //|  fcall InterpreterPropGetSSONotFound
      dasm_put(Dst, 4936, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterPropGetSSONotFound))) {
      dasm_put(Dst, 4951);
       } else {
         lava_warn("%s","Function InterpreterPropGetSSONotFound address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterPropGetSSONotFound)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterPropGetSSONotFound))>>32));
       }
#line 2685 "src/interpreter/bytecode-interpreter.dasc"
      //|  jmp ->InterpFail
      dasm_put(Dst, 497);
#line 2686 "src/interpreter/bytecode-interpreter.dasc"

      //|9: // failed at *object*
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  mov CARG3L, ARG3
      //|  lea CARG4, [STK+ARG1F*8]
      //|  fcall InterpreterPropGetSSO
      dasm_put(Dst, 4956, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterPropGetSSO))) {
      dasm_put(Dst, 4974);
       } else {
         lava_warn("%s","Function InterpreterPropGetSSO address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterPropGetSSO)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterPropGetSSO))>>32));
       }
#line 2693 "src/interpreter/bytecode-interpreter.dasc"
      //|  retbool
      dasm_put(Dst, 302);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2694 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_PROPGET:
      // This version of bytecode indicates that the string *MUST NOT* be a SSO
      // just directly fallback to the slow version written in C++
      //|=>bc:
      //|  instr_D
      //|  savepc
      //|  mov   CARG1, RUNTIME
      //|  lea   CARG2, [STK+ARG2F*8]
      //|  LdStr CARG3, ARG3F
      //|  lea   CARG4, [STK+ARG1F*8]
      //|  fcall InterpreterPropGet
      dasm_put(Dst, 4979, bc, RuntimeLayout::kCurPCOffset, PrototypeLayout::kStringTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterPropGet))) {
      dasm_put(Dst, 5026);
       } else {
         lava_warn("%s","Function InterpreterPropGet address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterPropGet)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterPropGet))>>32));
       }
#line 2707 "src/interpreter/bytecode-interpreter.dasc"
      //|  retbool
      dasm_put(Dst, 302);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2708 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_PROPSETSSO:
      //|=>bc:
      //|.macro setsso_found
      //|  mov T0, qword [STK+ARG3F*8]
      //|  mov qword [RREG+MapEntryLayout::kValueOffset], T0
      //|  Dispatch
      //|.endmacro
      dasm_put(Dst, 5031, bc);
#line 2717 "src/interpreter/bytecode-interpreter.dasc"

      //|  instr_D
      //|  cmp word [STK+ARG1F*8+6], Value::FLAG_HEAP
      //|  jne ->InterpPropNeedObject
      dasm_put(Dst, 5033, Value::FLAG_HEAP);
#line 2721 "src/interpreter/bytecode-interpreter.dasc"

      //|  lea CARG2, [STK+ARG1F*8]
      //|  mov ARG1F, qword [STK+ARG1F*8]
      //|  CheckObj ARG1F, >9
      dasm_put(Dst, 5062, -HOH_TYPE_OFFSET, OBJECT_BIT_PATTERN);
#line 2725 "src/interpreter/bytecode-interpreter.dasc"

      // Load the *Map* object into ARG1F
      //|  mov ARG1F, qword [ARG1F+ObjectLayout::kMapOffset]
      //|  mov ARG1F, qword [ARG1F]
      //|  LdSSO ARG2F,ARG2F,T0
      dasm_put(Dst, 5087, ObjectLayout::kMapOffset, PrototypeLayout::kSSOTableOffset);
#line 2730 "src/interpreter/bytecode-interpreter.dasc"

      //|  objfind_sso ARG1F,ARG2F,>8,setsso_found
      dasm_put(Dst, 5111, SSOLayout::kHashOffset, MapLayout::kMaskOffset, MapLayout::kArrayOffset, MapEntryLayout::kFlagOffset, (Map::Entry::kUseButNotDelBit), MapEntryLayout::kKeyOffset, -HOH_TYPE_OFFSET, SSO_BIT_PATTERN, MapEntryLayout::kValueOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 4888, (Map::Entry::kMoreBit), (bits::BitOn<std::uint32_t,0,29>::value));
#line 2732 "src/interpreter/bytecode-interpreter.dasc"

      //|8:
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  mov CARG2, ARG2F
      //|  fcall InterpreterPropSetSSONotFound
      dasm_put(Dst, 5189, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterPropSetSSONotFound))) {
      dasm_put(Dst, 5204);
       } else {
         lava_warn("%s","Function InterpreterPropSetSSONotFound address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterPropSetSSONotFound)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterPropSetSSONotFound))>>32));
       }
#line 2738 "src/interpreter/bytecode-interpreter.dasc"
      //|  jmp ->InterpFail
      dasm_put(Dst, 497);
#line 2739 "src/interpreter/bytecode-interpreter.dasc"

      //|9:
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  mov CARG3L, ARG2
      //|  lea CARG4, [STK+ARG3F*8]
      //|  fcall InterpreterPropSetSSO
      dasm_put(Dst, 5209, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterPropSetSSO))) {
      dasm_put(Dst, 5227);
       } else {
         lava_warn("%s","Function InterpreterPropSetSSO address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterPropSetSSO)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterPropSetSSO))>>32));
       }
#line 2746 "src/interpreter/bytecode-interpreter.dasc"
      //|  retbool
      dasm_put(Dst, 302);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2747 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_PROPSET:
      //|=>bc:
      //|  instr_D
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  lea CARG2, [STK+ARG1F*8]
      //|  LdStr CARG3, ARG2F
      //|  lea CARG4, [STK+ARG3F*8]
      //|  fcall InterpreterPropSet
      dasm_put(Dst, 5232, bc, RuntimeLayout::kCurPCOffset, PrototypeLayout::kStringTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterPropSet))) {
      dasm_put(Dst, 5279);
       } else {
         lava_warn("%s","Function InterpreterPropSet address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterPropSet)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterPropSet))>>32));
       }
#line 2758 "src/interpreter/bytecode-interpreter.dasc"
      //|  retbool
      dasm_put(Dst, 302);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2759 "src/interpreter/bytecode-interpreter.dasc"
      break;

    // Assume the ARG3F *already* contains index value ,4 bytes
    //|.macro index_list,oob,not_list
    //|  cmp word [STK+ARG2F*8+6], Value::FLAG_HEAP
    //|  jne ->InterpPropNeedObject

    //|  mov LREG, qword [STK+ARG2F*8]
    //|  CheckList LREG,not_list // if it is not a list then jump

    //|  cmp ARG3, dword [LREG+ListLayout::kSizeOffset]
    //|  jae ->oob

    //|  mov RREG, qword [LREG+ListLayout::kSliceOffset]
    //|  mov ARG2F,qword [RREG]

    //|  mov ARG2F, qword [ARG2F+ARG3F*8+SliceLayout::kArrayOffset]
    //|  mov qword [STK+ARG1F*8], ARG2F

    //|  Dispatch
    //|.endmacro

    case BC_IDXGET:
      //|=>bc:
      //|  instr_D
      //|  cmp dword [STK+ARG3F*8+4], Value::FLAG_REAL
      //|  jnb >1
      //|  cvtsd2si T0, qword [STK+ARG3F*8]
      //|  test T0,T0
      //|  js >1 // negative index, cannot handle
      dasm_put(Dst, 5284, bc, Value::FLAG_REAL);
#line 2789 "src/interpreter/bytecode-interpreter.dasc"

      // do the indexing for list or array
      //|  mov ARG3F, T0
      //|  index_list,InterpIdxOutOfBoundGet,->InterpIdxGetI
      dasm_put(Dst, 5328, Value::FLAG_HEAP, -HOH_TYPE_OFFSET, LIST_BIT_PATTERN, ListLayout::kSizeOffset, ListLayout::kSliceOffset, SliceLayout::kArrayOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2793 "src/interpreter/bytecode-interpreter.dasc"

      // general type index
      //|1:
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  lea CARG2, [STK+ARG2F*8]
      //|  lea CARG3, [STK+ARG3F*8]
      //|  lea CARG4, [STK+ARG1F*8]
      //|  fcall InterpreterIdxGet
      dasm_put(Dst, 5399, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterIdxGet))) {
      dasm_put(Dst, 710);
       } else {
         lava_warn("%s","Function InterpreterIdxGet address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterIdxGet)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterIdxGet))>>32));
       }
#line 2802 "src/interpreter/bytecode-interpreter.dasc"
      //|  retbool
      dasm_put(Dst, 302);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2803 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_IDXGETI:
      //|=>bc:
      //|  instr_D
      //|  index_list,InterpIdxOutOfBoundGet,->InterpIdxGetI
      dasm_put(Dst, 5423, bc, Value::FLAG_HEAP, -HOH_TYPE_OFFSET, LIST_BIT_PATTERN, ListLayout::kSizeOffset, ListLayout::kSliceOffset, SliceLayout::kArrayOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2809 "src/interpreter/bytecode-interpreter.dasc"
      break;

    //|.macro set_list,oob,not_list
    //|  cmp word [STK+ARG1F*8+6], Value::FLAG_HEAP
    //|  jne ->InterpPropNeedObject

    //|  mov LREG, qword [STK+ARG1F*8]
    //|  CheckList LREG,not_list

    //|  cmp ARG2, dword [LREG+ListLayout::kSizeOffset]
    //|  jae ->oob

    //|  mov RREG, qword [LREG+ListLayout::kSliceOffset]
    //|  mov ARG1F,qword [RREG]  // ARG1F --> Slice*

    //|  mov LREG, qword [STK+ARG3F*8]
    //|  mov qword [ARG1F+ARG2F*8+SliceLayout::kArrayOffset], LREG
    //|  Dispatch
    //|.endmacro


    case BC_IDXSET:
      //|=>bc:
      //|  instr_D
      dasm_put(Dst, 2226, bc);
#line 2833 "src/interpreter/bytecode-interpreter.dasc"

      // check the idx is a number
      //|  cmp dword [STK+ARG2F*8+4], Value::FLAG_REAL
      //|  jnb >1
      dasm_put(Dst, 5507, Value::FLAG_REAL);
#line 2837 "src/interpreter/bytecode-interpreter.dasc"
      // conversion
      //|  cvtsd2si T0, qword [STK+ARG2F*8]
      //|  test T0, T0
      //|  jz >1
      dasm_put(Dst, 5519);
#line 2841 "src/interpreter/bytecode-interpreter.dasc"

      //|  mov ARG2F, T0
      //|  set_list,InterpIdxOutOfBoundSet,->InterpIdxSetI
      dasm_put(Dst, 5535, Value::FLAG_HEAP, -HOH_TYPE_OFFSET, LIST_BIT_PATTERN, ListLayout::kSizeOffset, ListLayout::kSliceOffset, SliceLayout::kArrayOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2844 "src/interpreter/bytecode-interpreter.dasc"

      //|1:
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  lea CARG2, [STK+ARG1F*8]
      //|  lea CARG3, [STK+ARG2F*8]
      //|  lea CARG4, [STK+ARG3F*8]
      //|  fcall InterpreterIdxSet
      dasm_put(Dst, 5606, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterIdxSet))) {
      dasm_put(Dst, 746);
       } else {
         lava_warn("%s","Function InterpreterIdxSet address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterIdxSet)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterIdxSet))>>32));
       }
#line 2852 "src/interpreter/bytecode-interpreter.dasc"
      //|  retbool
      dasm_put(Dst, 302);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2853 "src/interpreter/bytecode-interpreter.dasc"
      break;


    case BC_IDXSETI:
      //|=>bc:
      //|  instr_D // ARG1 == object; ARG2 == imm; ARG3 == value
      //|  set_list,InterpIdxOutOfBoundSet,->InterpIdxSetI
      dasm_put(Dst, 5630, bc, Value::FLAG_HEAP, -HOH_TYPE_OFFSET, LIST_BIT_PATTERN, ListLayout::kSizeOffset, ListLayout::kSliceOffset, SliceLayout::kArrayOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2860 "src/interpreter/bytecode-interpreter.dasc"
      break;

    /* ========================================================
     * Globals
     * =======================================================*/
    case BC_GGETSSO:
      // handler for handling key entry found case
      //|.macro ggetsso_found
      //|  mov LREG, qword [RREG+MapEntryLayout::kValueOffset]
      //|  mov qword [STK+ARG1F*8],LREG
      //|  Dispatch
      //|.endmacro

      //|=>bc:
      //|  instr_B
      dasm_put(Dst, 5714, bc);
#line 2875 "src/interpreter/bytecode-interpreter.dasc"

      //|  mov ARG3F, qword [RUNTIME+RuntimeLayout::kGlobalOffset]
      //|  mov ARG3F, qword [ARG3F]
      dasm_put(Dst, 5723, RuntimeLayout::kGlobalOffset);
#line 2878 "src/interpreter/bytecode-interpreter.dasc"

      //|  mov ARG3F, qword [ARG3F+ObjectLayout::kMapOffset]
      //|  mov ARG3F, qword [ARG3F]
      dasm_put(Dst, 5733, ObjectLayout::kMapOffset);
#line 2881 "src/interpreter/bytecode-interpreter.dasc"

      //|  LdSSO ARG2F,ARG2F,T0
      dasm_put(Dst, 5094, PrototypeLayout::kSSOTableOffset);
#line 2883 "src/interpreter/bytecode-interpreter.dasc"

      //|  objfind_sso ARG3F,ARG2F,>8,ggetsso_found
      dasm_put(Dst, 5741, SSOLayout::kHashOffset, MapLayout::kMaskOffset, MapLayout::kArrayOffset, MapEntryLayout::kFlagOffset, (Map::Entry::kUseButNotDelBit), MapEntryLayout::kKeyOffset, -HOH_TYPE_OFFSET, SSO_BIT_PATTERN, MapEntryLayout::kValueOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 4888, (Map::Entry::kMoreBit), (bits::BitOn<std::uint32_t,0,29>::value));
#line 2885 "src/interpreter/bytecode-interpreter.dasc"

      // Globals not found
      //|8:
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  mov CARG2, ARG2F
      //|  fcall InterpreterGGetNotFoundSSO
      dasm_put(Dst, 5189, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterGGetNotFoundSSO))) {
      dasm_put(Dst, 5819);
       } else {
         lava_warn("%s","Function InterpreterGGetNotFoundSSO address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterGGetNotFoundSSO)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterGGetNotFoundSSO))>>32));
       }
#line 2892 "src/interpreter/bytecode-interpreter.dasc"
      //|  jmp ->InterpFail
      dasm_put(Dst, 497);
#line 2893 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_GGET:
      //|=>bc:
      //|  instr_B
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  lea CARG2, [STK+ARG1F*8]
      //|  LdStr CARG3, ARG2F
      //|  fcall InterpreterGGet
      dasm_put(Dst, 5824, bc, RuntimeLayout::kCurPCOffset, PrototypeLayout::kStringTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterGGet))) {
      dasm_put(Dst, 5858);
       } else {
         lava_warn("%s","Function InterpreterGGet address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterGGet)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterGGet))>>32));
       }
#line 2903 "src/interpreter/bytecode-interpreter.dasc"
      //|  retbool
      dasm_put(Dst, 302);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2904 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_GSETSSO:
      //|.macro gsetsso_found
      //|  mov LREG, qword [STK+ARG2F*8]
      //|  mov qword [RREG+MapEntryLayout::kValueOffset], LREG
      //|  Dispatch
      //|.endmacro

      //|=>bc:
      //|  instr_B
      dasm_put(Dst, 5714, bc);
#line 2915 "src/interpreter/bytecode-interpreter.dasc"

      //|  mov ARG3F, qword [RUNTIME+RuntimeLayout::kGlobalOffset]
      //|  mov ARG3F, qword [ARG3F]
      dasm_put(Dst, 5723, RuntimeLayout::kGlobalOffset);
#line 2918 "src/interpreter/bytecode-interpreter.dasc"

      //|  mov ARG3F, qword [ARG3F+ObjectLayout::kMapOffset]
      //|  mov ARG3F, qword [ARG3F]
      dasm_put(Dst, 5733, ObjectLayout::kMapOffset);
#line 2921 "src/interpreter/bytecode-interpreter.dasc"

      //|  LdSSO ARG1F,ARG1F,T0
      //|  objfind_sso ARG3F,ARG1F,>8,gsetsso_found
      dasm_put(Dst, 5863, PrototypeLayout::kSSOTableOffset, SSOLayout::kHashOffset, MapLayout::kMaskOffset, MapLayout::kArrayOffset, MapEntryLayout::kFlagOffset, (Map::Entry::kUseButNotDelBit), MapEntryLayout::kKeyOffset, -HOH_TYPE_OFFSET, SSO_BIT_PATTERN, MapEntryLayout::kValueOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
#line 2924 "src/interpreter/bytecode-interpreter.dasc"
      //|8:
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  mov CARG2, ARG1F
      //|  fcall InterpreterGSetNotFoundSSO
      dasm_put(Dst, 5957, (Map::Entry::kMoreBit), (bits::BitOn<std::uint32_t,0,29>::value), RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterGSetNotFoundSSO))) {
      dasm_put(Dst, 6019);
       } else {
         lava_warn("%s","Function InterpreterGSetNotFoundSSO address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterGSetNotFoundSSO)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterGSetNotFoundSSO))>>32));
       }
#line 2929 "src/interpreter/bytecode-interpreter.dasc"
      //|  jmp ->InterpFail
      dasm_put(Dst, 497);
#line 2930 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_GSET:
      //|=>bc:
      //|  instr_C
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  LdStr CARG2, ARG1F
      //|  lea CARG3, [STK+ARG2F*8]
      //|  fcall InterpreterGSet
      dasm_put(Dst, 6024, bc, RuntimeLayout::kCurPCOffset, PrototypeLayout::kStringTableOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterGSet))) {
      dasm_put(Dst, 6058);
       } else {
         lava_warn("%s","Function InterpreterGSet address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterGSet)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterGSet))>>32));
       }
#line 2940 "src/interpreter/bytecode-interpreter.dasc"
      //|  retbool
      dasm_put(Dst, 302);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2941 "src/interpreter/bytecode-interpreter.dasc"
      break;

    // ==========================================================
    // Upvalue
    // ==========================================================
    case BC_UVGET:
      //|=>bc:
      //|  instr_B
      //|  LdUV LREG,ARG2F
      //|  mov  qword [STK+ARG1F*8], LREG
      //|  Dispatch
      dasm_put(Dst, 6063, bc, RuntimeLayout::kCurClsOffset, ClosureLayout::kUpValueOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2952 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_UVSET:
      //|=>bc:
      //|  instr_C
      //|  mov RREG, qword [STK+ARG2F*8]
      //|  StUV ARG1F,RREG
      //|  Dispatch
      dasm_put(Dst, 6100, bc, RuntimeLayout::kCurClsOffset, ClosureLayout::kUpValueOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 2960 "src/interpreter/bytecode-interpreter.dasc"
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
      dasm_put(Dst, 5714, bc);
#line 2972 "src/interpreter/bytecode-interpreter.dasc"
      // must be boolean flag here
      //|  cmp dword [STK+ACCFIDX], Value::FLAG_FALSE
      //|  je >1
      //|2:
      //|  Dispatch
      dasm_put(Dst, 6137, Value::FLAG_FALSE);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
#line 2977 "src/interpreter/bytecode-interpreter.dasc"
      //|1:
      //|  branch_to ARG2F,ARG3F
      //|  jmp <2
      dasm_put(Dst, 6164);
#line 2980 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_FEND1:
      //|=>bc:
      //|  instr_E // actually TYPE_H instruction
      //|  cmp dword [STK+ARG1F*8+4], Value::FLAG_REAL
      //|  jnb >6
      dasm_put(Dst, 6198, bc, Value::FLAG_REAL);
#line 2987 "src/interpreter/bytecode-interpreter.dasc"

      //|  cmp dword [STK+ARG2F*8+4], Value::FLAG_REAL
      //|  jnb >6
      dasm_put(Dst, 6217, Value::FLAG_REAL);
#line 2990 "src/interpreter/bytecode-interpreter.dasc"

      //|  movsd xmm0, qword [STK+ARG1F*8]
      //|  ucomisd xmm0, qword [STK+ARG2F*8]
      //|  jae >8 // loop exit
      dasm_put(Dst, 6229);
#line 2994 "src/interpreter/bytecode-interpreter.dasc"

      //|  mov ARG1, dword [PC]
      //|  branch_to ARG1F,ARG3F
      //|7:
      //|  Dispatch
      dasm_put(Dst, 6247);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
#line 2999 "src/interpreter/bytecode-interpreter.dasc"
      //|8:
      //|  // skip the 4th argument
      //|  add PC,4
      //|  jmp <7
      dasm_put(Dst, 6271);
#line 3003 "src/interpreter/bytecode-interpreter.dasc"

      //|6: // fallback for situation that is not integer
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  lea CARG2, [STK+ARG1F*8]
      //|  lea CARG3, [STK+ARG2F*8]
      //|  mov CARG4L, dword [PC]
      //|  fcall InterpreterForEnd1
      dasm_put(Dst, 6301, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterForEnd1))) {
      dasm_put(Dst, 6324);
       } else {
         lava_warn("%s","Function InterpreterForEnd1 address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterForEnd1)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterForEnd1))>>32));
       }
#line 3011 "src/interpreter/bytecode-interpreter.dasc"
      // handle return value
      //|  test eax,eax
      //|  je ->InterpFail
      //|  mov PC, qword [RUNTIME+RuntimeLayout::kCurPCOffset]
      //|  Dispatch
      dasm_put(Dst, 6329, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 3016 "src/interpreter/bytecode-interpreter.dasc"
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
      dasm_put(Dst, 6352, bc, Value::FLAG_REAL, Value::FLAG_REAL, Value::FLAG_REAL);
#line 3027 "src/interpreter/bytecode-interpreter.dasc"

      //|  movsd xmm0, qword [STK+ARG1F*8]
      //|  addsd xmm0, qword [STK+ARG3F*8]
      //|  ucomisd xmm0, qword [STK+ARG2F*8]
      //|  movsd qword [STK+ARG1F*8], xmm0 // need to write back
      //|  jae >8 // loop exit
      dasm_put(Dst, 6403);
#line 3033 "src/interpreter/bytecode-interpreter.dasc"

      // fallthrough
      //|  mov ARG1, dword [PC]
      //|  branch_to ARG1F,ARG3F
      //|7:
      //|  Dispatch
      dasm_put(Dst, 6247);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
#line 3039 "src/interpreter/bytecode-interpreter.dasc"
      //|8:
      //|  add PC,4
      //|  jmp <7
      dasm_put(Dst, 6271);
#line 3042 "src/interpreter/bytecode-interpreter.dasc"

      //|6:
      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  lea CARG2, [STK+ARG1F*8]
      //|  lea CARG3, [STK+ARG2F*8]
      //|  lea CARG4, [STK+ARG3F*8]
      //|  mov CARG5L, dword [PC]
      //|  fcall InterpreterForEnd2
      dasm_put(Dst, 6435, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterForEnd2))) {
      dasm_put(Dst, 6463);
       } else {
         lava_warn("%s","Function InterpreterForEnd2 address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterForEnd2)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterForEnd2))>>32));
       }
#line 3051 "src/interpreter/bytecode-interpreter.dasc"
      //|  test eax,eax
      //|  je ->InterpFail
      //|  mov PC, qword [RUNTIME+RuntimeLayout::kCurPCOffset]
      //|  Dispatch
      dasm_put(Dst, 6329, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 3055 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_FEVRSTART:
      //|=>bc:
      //|  instr_X
      //|  Dispatch
      dasm_put(Dst, 6468, bc);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 3061 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_FEVREND:
      //|=>bc:
      //|  instr_G
      //|  branch_to ARG1F,ARG3F
      //|  Dispatch
      dasm_put(Dst, 4723, bc);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 3068 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_BRK:
      //|  absolute_jmp BC_BRK
      dasm_put(Dst, 4723, BC_BRK);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 3072 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_CONT:
      //|  absolute_jmp BC_CONT
      dasm_put(Dst, 4723, BC_CONT);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 3076 "src/interpreter/bytecode-interpreter.dasc"
      break;

    // Foreach instructions
    case BC_FESTART:
      //|=>bc:
      //|  instr_B
      dasm_put(Dst, 5714, bc);
#line 3082 "src/interpreter/bytecode-interpreter.dasc"

      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  lea CARG2, [STK+ARG1F*8]
      //|  mov CARG3L, ARG2
      //|  fcall InterpreterFEStart
      dasm_put(Dst, 1182, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterFEStart))) {
      dasm_put(Dst, 6480);
       } else {
         lava_warn("%s","Function InterpreterFEStart address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterFEStart)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterFEStart))>>32));
       }
#line 3088 "src/interpreter/bytecode-interpreter.dasc"
      //|  test eax,eax
      //|  je ->InterpFail
      //|  mov PC, qword [RUNTIME+RuntimeLayout::kCurPCOffset]
      //|  Dispatch
      dasm_put(Dst, 6329, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 3092 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_FEEND:
      //|=>bc:
      //|  instr_B
      dasm_put(Dst, 5714, bc);
#line 3097 "src/interpreter/bytecode-interpreter.dasc"

      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  lea CARG2, [STK+ARG1F*8]
      //|  mov CARG3L,ARG2
      //|  fcall InterpreterFEEnd
      dasm_put(Dst, 1182, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterFEEnd))) {
      dasm_put(Dst, 6485);
       } else {
         lava_warn("%s","Function InterpreterFEEnd address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterFEEnd)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterFEEnd))>>32));
       }
#line 3103 "src/interpreter/bytecode-interpreter.dasc"
      //|  mov PC, qword [RUNTIME+RuntimeLayout::kCurPCOffset]
      //|  Dispatch
      dasm_put(Dst, 6335, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 3105 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_IDREF:
      //|=>bc:
      //|  instr_D
      dasm_put(Dst, 2226, bc);
#line 3110 "src/interpreter/bytecode-interpreter.dasc"

      //|  savepc
      //|  mov CARG1, RUNTIME
      //|  lea CARG2, [STK+ARG1F*8]
      //|  lea CARG3, [STK+ARG2F*8]
      //|  lea CARG4, [STK+ARG3F*8]
      //|  fcall InterpreterIDref
      dasm_put(Dst, 1147, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(InterpreterIDref))) {
      dasm_put(Dst, 6490);
       } else {
         lava_warn("%s","Function InterpreterIDref address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(InterpreterIDref)), (unsigned int)((reinterpret_cast<std::uintptr_t>(InterpreterIDref))>>32));
       }
#line 3117 "src/interpreter/bytecode-interpreter.dasc"
      //|  Dispatch
      dasm_put(Dst, 149);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 173);
#line 3118 "src/interpreter/bytecode-interpreter.dasc"
      break;

    /* ------------------------------------------------------------
     * Call/TCall/Return
     * -----------------------------------------------------------*/

      //|.macro do_call,set_flag,slow_path
      //|  instr_D

      // 1. Do a stack check to see wheather we *need* to grow the
      //    stack since a function call *may* run out of the stack
      //    space
      //|  lea T0, [STK+ARG2F*8]
      //|  cmp T0, qword [RUNTIME+RuntimeLayout::kStackEndOffset]
      //|  jae >9  // Resize stack

      // 2. Check object type
      //|3:
      //|  cmp word [STK+ARG1F*8+6], Value::FLAG_HEAP
      //|  jne ->InterpNeedObject

      // Okay , we have heap object now and we need to tell its type
      // and then do the actual dispatching. 2 types of value can be
      // used for a call , one is prototype in script ; the other is
      // extension type. Extension type will be dispatched by c++
      // function.
      //|  mov RREG, qword [STK+ARG1F*8]
      //|  DerefPtrFromV RREG
      //|  mov LREG, qword [RREG]                    // Get HeapObject*

      //|  CheckHeapPtrT LREG,CLOSURE_BIT_PATTERN  ,->slow_path

      // Check argument number
      //|  cmp ARG3_8 , byte [LREG+ClosureLayout::kArgumentSizeOffset]
      //|  jne ->InterpArgumentMismatch

      // RREG (Closure**)
      // LREG (Closure* )
      // ARG2 (Base)
      // ARG3 (Narg)

      /*
       * Store the old PC into the *current* frame for recovery of
       * stack frame when return
       */
      //|  movzx ARG1 , word [STK-10]
      //|  shl   ARG1F, 48
      //|  or    ARG1F, PC
      //|  mov   qword [STK-16], ARG1F

      /*
       * Store the BASE value into the *new* frame
       */
      //|  shl ARG2F,51              // 51 == 48 + 3 (3 means ARG2*8)
      //|  mov qword [T0-16], ARG2F  // This will reset the whole quad to be 0 ended
                                   // Because we use *or* to set the PC to the place

      //|  mov qword [T0-8] , RREG   // Set the *closure* pointer into the *new* frame

      // set the needed flag
      //|  set_flag

      // set the closure pointer back to *runtime* object
      //|  mov qword [RUNTIME+RuntimeLayout::kCurClsOffset],RREG

      // get the *new* proto object
      //|  mov PROTO , qword [LREG+ClosureLayout::kPrototypeOffset]

      // get the *new* code buffer starting pointer
      //|  mov PC , qword [LREG+ClosureLayout::kCodeBufferOffset]

      // change the current context PROTO and PC register to the correct field
      // of the new closure
      //|  mov STK   , T0               // set the new *stack*
      //|  mov qword [RUNTIME+RuntimeLayout::kCurStackOffset], T0

      //|  mov qword SAVED_PC, PC       // set the savedpc
      //|  Dispatch

      // stack overflow
      // ARG2F/ARG3F are caller saved, ARG1F are callee saved
      //|9:
      //|  savepc
      //|  mov qword [SAVED_SLOT1] , ARG2F
      //|  mov qword [SAVED_SLOT2] , ARG2F
      //|  mov CARG1, RUNTIME
      //|  fcall ResizeStack
      //|  test eax,eax
      //|  je ->InterpFail
      //|  mov ARG2F, qword [SAVED_SLOT1]
      //|  mov ARG3F, qword [SAVED_SLOT2]
      //|  jmp <3 // resume execution
      //|.endmacro

      //|.macro call_flag
      //|.endmacro

      //|.macro tcall_flag
      // A tcall flag needs to be set up and no need to store PC
      // offset in current frame since we won't return to this frame
      //|  mov byte [STK-1], 1 // mark it as a tcall frame
      //|.endmacro


    case BC_CALL:
      //|=>bc:
      //|  do_call call_flag,InterpCall
      dasm_put(Dst, 6495, bc, RuntimeLayout::kStackEndOffset, Value::FLAG_HEAP, -HOH_TYPE_OFFSET, CLOSURE_BIT_PATTERN, ClosureLayout::kArgumentSizeOffset, RuntimeLayout::kCurClsOffset, ClosureLayout::kPrototypeOffset, ClosureLayout::kCodeBufferOffset);
      dasm_put(Dst, 6615, RuntimeLayout::kCurStackOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 6640, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(ResizeStack))) {
      dasm_put(Dst, 6681);
       } else {
         lava_warn("%s","Function ResizeStack address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(ResizeStack)), (unsigned int)((reinterpret_cast<std::uintptr_t>(ResizeStack))>>32));
       }
      dasm_put(Dst, 6686);
#line 3225 "src/interpreter/bytecode-interpreter.dasc"
      break;

    case BC_TCALL:
      //|=>bc:
      //|  do_call tcall_flag,InterpTCall
      dasm_put(Dst, 6707, bc, RuntimeLayout::kStackEndOffset, Value::FLAG_HEAP, -HOH_TYPE_OFFSET, CLOSURE_BIT_PATTERN, ClosureLayout::kArgumentSizeOffset, RuntimeLayout::kCurClsOffset, ClosureLayout::kPrototypeOffset, ClosureLayout::kCodeBufferOffset);
      dasm_put(Dst, 6615, RuntimeLayout::kCurStackOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
      dasm_put(Dst, 160);
       } else {
         lava_warn("%s","Function PrintOP address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
       }
      dasm_put(Dst, 6640, RuntimeLayout::kCurPCOffset);
       if(CheckAddress(reinterpret_cast<std::uintptr_t>(ResizeStack))) {
      dasm_put(Dst, 6681);
       } else {
         lava_warn("%s","Function ResizeStack address is not in 0-2GB");
      dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(ResizeStack)), (unsigned int)((reinterpret_cast<std::uintptr_t>(ResizeStack))>>32));
       }
      dasm_put(Dst, 6686);
#line 3230 "src/interpreter/bytecode-interpreter.dasc"
      break;

    //|.macro do_ret

    //|2:
    //|  movzx ARG1F, word [STK-10]

    //|  cmp ARG1F,IFRAME_EOF
    //|  je ->InterpReturn             // Interpreter return from here

    //|  sub   STK  , ARG1F            // Now STK points to the *previous* frame

    // now STK points to the *previous* frame and we need to check
    // whether the previous frame is a frame we need to skip since
    // it maybe a tail call frame
    //|  cmp byte [STK-1], 1
    //|  je <2

    //|1:
    //|  mov   LREG , qword [STK-8]    // LREG == Closure**
    //|  mov   qword [RUNTIME+RuntimeLayout::kCurClsOffset], LREG
    //|  mov   ARG1F, qword [LREG]
    //|  mov   PROTO, qword [ARG1F+ClosureLayout::kPrototypeOffset]
    //|  mov   PC , qword [STK-16]
    //|  and   PC , qword [->PointerMask]

    //|  mov   ARG2F, qword [ARG1F+ClosureLayout::kCodeBufferOffset]
    //|  mov   qword SAVED_PC, ARG2F

    //|.endmacro

    case BC_RETNULL:
    //|=>bc:
    //|  instr_X
    //|  mov RREG, qword [->ValueNullConst]
    //|  do_ret
    //|  mov dword [STK+ACCFIDX], Value::FLAG_NULL
    //|  Dispatch
    dasm_put(Dst, 6833, bc, IFRAME_EOF, RuntimeLayout::kCurClsOffset, ClosureLayout::kPrototypeOffset, ClosureLayout::kCodeBufferOffset, Value::FLAG_NULL);
     if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
    dasm_put(Dst, 160);
     } else {
       lava_warn("%s","Function PrintOP address is not in 0-2GB");
    dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
     }
    dasm_put(Dst, 173);
#line 3268 "src/interpreter/bytecode-interpreter.dasc"
    break;

    case BC_RET:
    //|=>bc:
    //|  instr_X
    //|  mov RREG, qword [ACC]
    //|  do_ret
    //|  mov qword [ACC], RREG
    //|  Dispatch
    dasm_put(Dst, 6927, bc, IFRAME_EOF, RuntimeLayout::kCurClsOffset, ClosureLayout::kPrototypeOffset, ClosureLayout::kCodeBufferOffset);
     if(CheckAddress(reinterpret_cast<std::uintptr_t>(PrintOP))) {
    dasm_put(Dst, 160);
     } else {
       lava_warn("%s","Function PrintOP address is not in 0-2GB");
    dasm_put(Dst, 165, (unsigned int)(reinterpret_cast<std::uintptr_t>(PrintOP)), (unsigned int)((reinterpret_cast<std::uintptr_t>(PrintOP))>>32));
     }
    dasm_put(Dst, 173);
#line 3277 "src/interpreter/bytecode-interpreter.dasc"
    break;

    default:
      //|=> bc:
      //|  Break
      dasm_put(Dst, 7023,  bc);
#line 3282 "src/interpreter/bytecode-interpreter.dasc"
      break;
  }
}

// Help Dasm to resolve external address via Index idx
int ResolveExternalAddress( void** ctx , unsigned char* addr ,
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
  interp->interp_entry_ = reinterpret_cast<void*>(static_cast<char*>(buffer) + off);

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

  // Get the runtime object pointer
  Runtime* rt = context->gc()->GetInterpreterRuntime(script.ref(), globals.ref(), error);

  // Main function
  Handle<Prototype> main_proto(script->main());

  // Main function's closure
  Handle<Closure> cls(Closure::New(context->gc(),main_proto));

  // Entry of our assembly interpreter
  Main m = reinterpret_cast<Main>(interp_->interp_entry_);

  // Interpret the bytecode
  bool ret = m(rt,
               cls.ref(),
               (main_proto.ref()),
               reinterpret_cast<void*>(rt->stack_begin),
               const_cast<void*>(reinterpret_cast<const void*>(main_proto->code_buffer())),
               dispatch_interp_);
  // Check return
  if(ret) *rval = rt->ret;

  context->gc()->ReturnInterpreterRuntime(rt);
  return ret;
}

} // namespace interpreter
} // namespace lavascript
