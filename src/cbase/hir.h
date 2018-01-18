#ifndef CBASE_HIR_H_
#define CBASE_HIR_H_
#include "src/config.h"
#include "src/util.h"
#include "src/stl-helper.h"
#include "src/zone/zone.h"
#include "src/zone/vector.h"
#include "src/zone/list.h"
#include "src/zone/string.h"
#include "src/cbase/bytecode-analyze.h"
#include "src/interpreter/intrinsic-call.h"

#include <type_traits>
#include <map>
#include <vector>
#include <deque>
#include <stack>

namespace lavascript {
namespace cbase {
namespace hir {
using namespace ::lavascript;

class Graph;
class GraphBuilder;

/**
 * This is a separate information maintained for each IR node. It contains
 * information needed for (1)GC (2)OSRExit. This information is always kept
 * and carry on to code generation.
 *
 * Some IR may have exactly same IRInfo object since one bytecode can be mapped
 * to multiple IR node in some cases.
 */

class IRInfo {
 public:
  IRInfo( std::uint32_t method , const interpreter::BytecodeLocation& bc ):
    bc_     (bc),
    method_ (method)
  {}
 public:
  std::uint32_t method() const { return method_; }
  const interpreter::BytecodeLocation& bc() const { return bc_; }
 private:
  interpreter::BytecodeLocation bc_;  // Encoded bytecode
  std::uint32_t method_;              // Index for method information
};

/**
 * Used to record each IR's corresponding prototype information
 */
struct PrototypeInfo : zone::ZoneObject {
  std::uint32_t base;
  Handle<Closure> closure;
  PrototypeInfo( std::uint32_t b , const Handle<Closure>& cls ):
    base(b),
    closure(cls)
  {}
};

#define CBASE_IR_EXPRESSION(__)                 \
  /* const    */                                \
  __(Float64,FLOAT64,"float64",true)            \
  __(LString,LONG_STRING,"lstring",true)        \
  __(SString,SMALL_STRING,"small_string",true)  \
  __(Boolean,BOOLEAN,"boolean",true)            \
  __(Nil,NIL,"null",true)                       \
  /* compound */                                \
  __(IRList,LIST,   "list",false)               \
  __(IRObjectKV,OBJECT_KV,"object_kv",false)    \
  __(IRObject,OBJECT, "object",false)           \
  /* closure */                                 \
  __(LoadCls,LOAD_CLS,"load_cls",true)          \
  /* argument node */                           \
  __(Arg,ARG,"arg",true)                        \
  /* ariethmetic/comparison node */             \
  __(Binary,BINARY,"binary",false)              \
  __(Unary,UNARY ,"unary",false)                \
  /* ternary node */                            \
  __(Ternary,TERNARY,"ternary",false)           \
  /* upvalue */                                 \
  __(UVal,UVAL,"uval",true)                     \
  __(USet,USET  ,"uset",true)                   \
  /* property/idx */                            \
  __(PGet,PGET  ,"pget",false)                  \
  __(PSet,PSET  ,"pset",false)                  \
  __(IGet,IGET  ,"iget",false)                  \
  __(ISet,ISET  ,"iset",false)                  \
  /* gget */                                    \
  __(GGet,GGET  , "gget",false)                 \
  __(GSet,GSET  , "gset",false)                 \
  /* iterator */                                \
  __(ItrNew ,ITR_NEW ,"itr_new",false)          \
  __(ItrNext,ITR_NEXT,"itr_next",false)         \
  __(ItrTest,ITR_TEST,"itr_test",false)         \
  __(ItrDeref,ITR_DEREF,"itr_deref",false)      \
  /* call     */                                \
  __(Call,CALL   ,"call",false)                 \
  /* intrinsic call */                          \
  __(ICall,ICALL ,"icall",false)                \
  /* phi */                                     \
  __(Phi,PHI,"phi",false)                       \
  /* statement */                               \
  __(InitCls,INIT_CLS,"init_cls",false)         \
  __(Projection,PROJECTION,"projection",false)  \
  /* osr */                                     \
  __(OSRLoad,OSR_LOAD,"osr_load",true)          \
  /* effect */                                  \
  __(Effect,EFFECT,"effect",false)              \
  /* checkpoints */                             \
  __(Checkpoint,CHECKPOINT,"checkpoint",false)  \
  __(StackSlot,STACK_SLOT, "stackslot" ,false)  \
  __(UValSlot ,UVAL_SLOT , "uvalslot"  ,false)  \
  /* test , used for guarding */                \
  __(TestIndexOOB,TEST_INDEXOOB,"test_indexoob", false)   \
  __(TestType    ,TEST_TYPE     ,"test_type"  ,   false)

#define CBASE_IR_CONTROL_FLOW(__)               \
  __(Start,START,"start",false)                 \
  __(End,END  , "end" ,false)                   \
  __(LoopHeader,LOOP_HEADER,"loop_header",false)\
  __(Loop,LOOP ,"loop",false)                   \
  __(LoopExit,LOOP_EXIT,"loop_exit",false)      \
  __(If,IF,"if",false)                          \
  __(IfTrue,IF_TRUE,"if_true",false)            \
  __(IfFalse,IF_FALSE,"if_false",false)         \
  __(Jump,JUMP,"jump",false)                    \
  __(Guard,GUARD,"guard",false)                 \
  __(Fail ,FAIL,"fail" ,true)                   \
  __(Success,SUCCESS,"success",false)           \
  __(Return,RETURN,"return",false)              \
  __(Region,REGION,"region",false)              \
  __(Trap,TRAP, "trap",false)                   \
  /* osr */                                     \
  __(OSRStart,OSR_START,"osr_start",false)      \
  __(OSREnd  ,OSR_END  ,"osr_end"  ,false)

#define CBASE_IR_LIST(__)                       \
  CBASE_IR_EXPRESSION(__)                       \
  CBASE_IR_CONTROL_FLOW(__)

enum IRType {
#define __(A,B,...) IRTYPE_##B,

  /** expression related IRType **/
  CBASE_IR_EXPRESSION_START,
  CBASE_IR_EXPRESSION(__)
  CBASE_IR_EXPRESSION_END,

  /** control flow related IRType **/
  CBASE_IR_CONTROL_FLOW_START,
  CBASE_IR_CONTROL_FLOW(__)
  CBASE_IR_CONTROL_FLOW_END

#undef __ // __
};

// TODO:: modify this if new section of IR is added , eg OSR
#define SIZE_OF_IR (CBASE_IR_STMT_END-6)

const char* IRTypeGetName( IRType );

inline bool IRTypeIsExpr( IRType type ) {
  return (type >= CBASE_IR_EXPRESSION_START && type <= CBASE_IR_EXPRESSION_END);
}

inline bool IRTypeIsControlFlow( IRType type ) {
  return (type >= CBASE_IR_CONTROL_FLOW_START && type <= CBASE_IR_CONTROL_FLOW_END);
}

// Forward class declaration
#define __(A,...) class A;
CBASE_IR_LIST(__)
#undef __ // __

// Base class for each node type
class Node;
class Expr;
class ControlFlow;
class Stmt;
class BailoutEntry;

// ----------------------------------------------------------------------------
// Effect
// Some operation has side effect which is visiable to rest of the
// program. This types of dependency is not explicit represented by
// use-def and def-use , so it must be taken care of specifically.
//
// For each expression inside of our IR they are not bounded to a certain
// basic block due to the nature of sea of nodes , but for expression
// that has a certain side effect we will bind it to a certain basic
// block where we see the bytecode lies in.
//
// This relationship basically define the side effect of certain expression.
// Due to the natural order of control flow node, so these operations that
// has side effect will have automatic order.
typedef zone::List<Expr*> EffectList;

typedef EffectList::ForwardIterator EffectNodeIterator;

// This structure is held by *all* the expression. If the region field is not
// NULL then it means this expression has side effect and it is bounded at
// certain control flow region
struct EffectEdge {
  ControlFlow* region;
  EffectNodeIterator iterator;
  bool IsUsed() const { return region != NULL; }

  EffectEdge( ControlFlow* r , const EffectNodeIterator& itr ): region(r), iterator(itr) {}
  EffectEdge(): region(NULL), iterator() {}
};

/**
 * Def-Use chain and Use-Def chain. We rename these field to
 * make it more easy to understand since I personally don't
 * think Def-Use and Use-Def chain to be a easy way to express
 * what it does mean
 *
 * 1) operand list , represent what expression is used/depend on
 *    by this expression
 *
 * 2) ref list     , represent list of expression that use *this*
 *    expression
 *
 * NOTES:
 *
 * All the IR node must try to use OperandList to store their operand,
 * and try to express any kinds of depdenency of the node as part of
 * ir node and then use OperandList to store them. Do not add a new
 * member inside of the IR node to store any kinds of ir node dependency,
 * except for very specific data.
 *
 * The reason is OperandList provide way to modify all the reference
 * introduced by certain node. During the optimization phase, any node
 * is a candidate to be modified !
 *
 */
typedef zone::List<Expr*> OperandList;
typedef OperandList::ForwardIterator OperandIterator;

struct Ref {
  OperandIterator id;  // iterator used for fast deletion of this Ref it is
                       // modified
  Node* node;
  Ref( const OperandIterator& iter , Node* n ): id(iter),node(n) {}
  Ref(): id(), node(NULL) {}
};

typedef zone::List<Ref>          RefList;

// Mother of all IR node , most of the important information should be stored via ID
// as out of line storage
class Node : public zone::ZoneObject {
 public:
  // type of the node
  IRType type() const { return type_; }

  // name/string of the type
  const char* type_name() const { return IRTypeGetName(type()); }

  // a unique id for this node , it can be used to indexed into secondary storage
  std::uint32_t id() const { return id_; }

  // get the belonged graph object
  Graph* graph() const { return graph_; }

  // get the belonged zone object from graph
  inline zone::Zone* zone() const;

 public: // type check and cast

#define __(A,B,...) bool Is##A() const { return type() == IRTYPE_##B; }
  CBASE_IR_LIST(__)
#undef __ // __

#define __(A,B,...) inline A* As##A(); inline const A* As##A() const;
  CBASE_IR_LIST(__)
#undef __ // __

  bool IsString() const { return IsSString() || IsLString(); }
  inline const zone::String& AsZoneString() const;

  bool IsControlFlow() const { return IRTypeIsControlFlow(type()); }
  inline ControlFlow* AsControlFlow();
  inline const ControlFlow* AsControlFlow() const;

  bool IsExpr() const { return IRTypeIsExpr(type()); }
  inline Expr* AsExpr();
  inline const Expr* AsExpr() const;

 protected:
  Node( IRType type , std::uint32_t id , Graph* graph ):
    type_    (type),
    id_      (id)  ,
    graph_   (graph)
 {}

 private:
  IRType        type_;
  std::uint32_t id_;
  Graph*        graph_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(Node)
};

// Mother of IR node that can be bailout or needs to hold a check point.
// Basically IR supports speculative execution or needs to bailout to GC
// routine needs a Checkpoint expression attached to it to generate frame
// state while bailout.
//
// One thing to note, we can omit this frame state if we only need a allocation
// since we could disable GC temporarily
class BailoutEntry {
 public:
  void set_checkpoint( Checkpoint* node ) { checkpoint_ = node; }
  Checkpoint* checkpoint() const { return checkpoint_; }
  bool HasCheckpoint() const { return checkpoint_ != NULL; }
  void ClearCheckpoint()  { checkpoint_ = NULL; }

  BailoutEntry() : checkpoint_(NULL) {}
 private:
  Checkpoint* checkpoint_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(BailoutEntry)
};

// ================================================================
//
// Expr
//
//   This node is the mother all other expression node and its solo
//   goal is to expose def-use and use-def chain into different types
//
// ================================================================

class Expr : public Node {
 public: // GVN hash value and hash function

  // If GVNHash returns 0 means this expression doesn't support GVN
  virtual std::uint64_t GVNHash()   const { return 0; }
  virtual bool Equal( const Expr* ) const { return false; }

 public:
  bool HasEffect() const { return effect_.IsUsed(); }
  void set_effect( const EffectEdge& ee ) { effect_ = ee; }
  const EffectEdge& effect() const { return effect_; }

 public: // patching function helps to mutate any def-use and use-def

  // Replace *this* node with the input expression node. This replace
  // will only change all the node that *reference* this node but not
  // touch all the operands' reference list
  virtual void Replace( Expr* );
 public:

  // Operand list
  //
  // This list returns a list of operands used by this Expr/IR node. Most
  // of time operand_list will return at most 3 operands except for call
  // function
  OperandList* operand_list() { return &operand_list_; }
  const OperandList* operand_list() const { return &operand_list_; }

  // This function will add the input node into this node's operand list and
  // it will take care of the input node's ref list as well
  inline void AddOperand( Expr* node );

  // Reference list
  //
  // This list returns a list of Ref object which allow user to
  //   1) get a list of expression that uses this expression, ie who uses me
  //   2) get the corresponding iterator where *me* is inserted into the list
  //      so we can fast modify/remove us from its list
  const RefList* ref_list() const { return &ref_list_; }
  RefList* ref_list() { return &ref_list_; }

  // Add the referece into the reference list
  void AddRef( Node* who_uses_me , const OperandIterator& iter ) {
    ref_list()->PushBack(zone(),Ref(iter,who_uses_me));
  }

 public:
  // Check if this Expression is a Leaf node or not
  bool IsLeaf() const {
#define __(A,B,C,D) case IRTYPE_##B: return D;
    switch(type()) {
      CBASE_IR_EXPRESSION(__)
      default: lava_die(); return false;
    }
#undef __ // __
  }

  bool IsNoneLeaf() const { return !IsLeaf(); }

  IRInfo* ir_info() const { return ir_info_; }

 public:
  Expr( IRType type , std::uint32_t id , Graph* graph , IRInfo* info ):
    Node             (type,id,graph),
    operand_list_    (),
    ref_list_        (),
    ir_info_         (info),
    effect_          ()
  {}

 private:
  OperandList operand_list_;
  RefList     ref_list_;
  IRInfo*     ir_info_;
  EffectEdge  effect_;
};

/** ============================================================
 * GVN hash function helper implementation
 *
 * Helper function to implement the GVN hash table function
 * ============================================================
 */

template< typename T >
std::uint64_t GVNHash0( T* ptr ) {
  std::uint64_t type = reinterpret_cast<std::uint64_t>(ptr);
  return type;
}

template< typename T , typename V >
std::uint64_t GVNHash1( T* ptr , const V& value ) {
  std::uint64_t uval = static_cast<std::uint64_t>(value);
  std::uint64_t type = reinterpret_cast<std::uint64_t>(ptr);
  return (uval << 7) ^ (type);
}

template< typename T , typename V1 , typename V2 >
std::uint64_t GVNHash2( T* ptr , const V1& v1 , const V2& v2 ) {
  std::uint64_t uv2 = static_cast<std::uint64_t>(v2);
  return GVNHash1(ptr,v1) ^ (uv2);
}

template< typename T , typename V1, typename V2 , typename V3 >
std::uint64_t GVNHash3( T* ptr , const V1& v1 , const V2& v2 ,
                                                const V3& v3 ) {
  std::uint64_t uv3 = static_cast<std::uint64_t>(v3);
  return GVNHash2(ptr,v1,v2) ^ (uv3);
}

class GVNHashN {
 public:
  template< typename T >
  GVNHashN( T* seed ): value_(reinterpret_cast<std::uint64_t>(seed)<<7) {}

  template< typename T >
  void Add( const T& value ) { value_ ^= static_cast<std::uint64_t>(value); }

  std::uint64_t value() const { return value_; }
 private:
  std::uint64_t value_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(GVNHashN)
};

class Arg : public Expr {
 public:
  inline static Arg* New( Graph* , std::uint32_t );
  std::uint32_t index() const { return index_; }

  Arg( Graph* graph , std::uint32_t id , std::uint32_t index ):
    Expr  (IRTYPE_ARG,id,graph,NULL),
    index_(index)
  {}

 private:
  std::uint32_t index_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Arg)
};

class Float64 : public Expr {
 public:
  inline static Float64* New( Graph* , double , IRInfo* );
  double value() const { return value_; }

  Float64( Graph* graph , std::uint32_t id , double value , IRInfo* info ):
    Expr  (IRTYPE_FLOAT64,id,graph,info),
    value_(value)
  {}

 public:
  virtual std::uint64_t GVNHash() const {
    return GVNHash1(type_name(),value_);
  }

  virtual bool Equal( const Expr* that ) const {
    return that->IsFloat64() && (that->AsFloat64()->value() == value_);
  }

 private:
  double value_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Float64)
};

class Boolean : public Expr {
 public:
  inline static Boolean* New( Graph* , bool , IRInfo* );
  bool value() const { return value_; }

  Boolean( Graph* graph , std::uint32_t id , bool value , IRInfo* info ):
    Expr  (IRTYPE_BOOLEAN,id,graph,info),
    value_(value)
  {}

  virtual std::uint64_t GVNHash() const {
    return GVNHash1(type_name(),value_ ? 1 : 0);
  }

  virtual bool Equal( const Expr* that ) const {
    return that->IsBoolean() && (that->AsBoolean()->value() == value_);
  }

 private:
  bool value_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Boolean)
};

class LString : public Expr {
 public:
  inline static LString* New( Graph* , const LongString& , IRInfo* );
  const zone::String* value() const { return value_; }

  LString( Graph* graph , std::uint32_t id , const zone::String* value ,
                                             IRInfo* info ):
    Expr  (IRTYPE_LONG_STRING,id,graph,info),
    value_(value)
  {}

  virtual std::uint64_t GVNHash() const {
    return GVNHash1(type_name(),reinterpret_cast<std::uint64_t>(value_));
  }

  virtual bool Equal( const Expr* that ) const {
    return that->IsLString() && (*(that->AsLString()->value()) == *value_);
  }

 private:
  const zone::String* value_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(LString)
};

class SString : public Expr {
 public:
  inline static SString* New( Graph* , const SSO& , IRInfo* );
  const zone::String* value() const { return value_; }

  SString( Graph* graph , std::uint32_t id , const zone::String* value ,
                                             IRInfo* info ):
    Expr (IRTYPE_SMALL_STRING,id,graph,info),
    value_(value)
  {}

  virtual std::uint64_t GVNHash() const {
    return GVNHash1(type_name(),reinterpret_cast<std::uint64_t>(value_));
  }

  virtual bool Equal( const Expr* that ) const {
    return that->IsSString() && (*(that->AsSString()->value()) == *value_);
  }

 private:
  const zone::String* value_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(SString)
};

class Nil : public Expr {
 public:
  inline static Nil* New( Graph* , IRInfo* );

  Nil( Graph* graph , std::uint32_t id , IRInfo* info ):
    Expr(IRTYPE_NIL,id,graph,info)
  {}

  virtual std::uint64_t GVNHash() const {
    return GVNHash0(type_name());
  }

  virtual bool Equal( const Expr* that ) const {
    return that->IsNil();
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Nil)
};

class IRList : public Expr {
 public:
  inline static IRList* New( Graph* , std::size_t size , IRInfo* );

  void Add( Expr* node ) { AddOperand(node); }

  std::size_t Size() const { return operand_list()->size(); }

  IRList( Graph* graph , std::uint32_t id , std::size_t size , IRInfo* info ):
    Expr  (IRTYPE_LIST,id,graph,info)
  {
    (void)size; // implicit indicated by the size of operand_list()
  }

  virtual std::uint64_t GVNHash() const;
  virtual bool Equal( const Expr* ) const;

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(IRList)
};

class IRObjectKV : public Expr {
 public:
  inline static IRObjectKV* New( Graph* , Expr* , Expr* ,IRInfo* );

  Expr* key  () const { return operand_list()->First(); }
  Expr* value() const { return operand_list()->Last(); }

  IRObjectKV( Graph* graph , std::uint32_t id , Expr* key , Expr* val ,
                                                            IRInfo* info ):
    Expr(IRTYPE_OBJECT_KV,id,graph,info)
  {
    AddOperand(key);
    AddOperand(val);
  }

  virtual std::uint64_t GVNHash() const {
    auto khash = key()->GVNHash();
    if(!khash) return 0;
    auto vhash = value()->GVNHash();
    if(!vhash) return 0;
    return GVNHash2(type_name(),khash,vhash);
  }

  virtual bool Equal( const Expr* that ) const {
    if(that->IsIRObjectKV()) {
      auto kv = that->AsIRObjectKV();
      return kv->key()->Equal(key()) && kv->value()->Equal(value());
    }
    return false;
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(IRObjectKV)
};

class IRObject : public Expr {
 public:
  inline static IRObject* New( Graph* , std::size_t size , IRInfo* );

  void Add( Expr* key , Expr* val ) {
    AddOperand(IRObjectKV::New(graph(),key,val,ir_info()));
  }

  std::size_t Size() const { return operand_list()->size(); }

  IRObject( Graph* graph , std::uint32_t id , std::size_t size , IRInfo* info ):
    Expr  (IRTYPE_OBJECT,id,graph,info)
  {
    (void)size;
  }

  virtual std::uint64_t GVNHash() const;
  virtual bool Equal( const Expr* ) const;

  IRObject* Clone() const;

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(IRObject)
};

// ----------------------------------------------------------------
// Closure
// ----------------------------------------------------------------
class LoadCls : public Expr {
 public:
  static inline LoadCls* New( Graph* , std::uint32_t ref , IRInfo* info );
  std::uint32_t ref() const { return ref_; }

  LoadCls( Graph* graph , std::uint32_t id , std::uint32_t ref , IRInfo* info ):
    Expr (IRTYPE_LOAD_CLS,id,graph,info),
    ref_ (ref)
  {}

  virtual std::uint64_t GVNHash() const {
    return GVNHash1(type_name(),ref_);
  }

  virtual bool Equal( const Expr* that ) const {
    return that->IsLoadCls() && (that->AsLoadCls()->ref() == ref_);
  }

 private:
  std::uint32_t ref_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(LoadCls);
};

class Unary : public Expr , public BailoutEntry {
 public:
  enum Operator { MINUS, NOT };

  inline static Unary* New( Graph* , Expr* , Operator , IRInfo* );

  inline static Operator BytecodeToOperator( interpreter::Bytecode bc );
  inline static const char* GetOperatorName( Operator op );

 public:
  Expr* operand() const { return operand_list()->First(); }
  Operator op  () const { return op_;      }
  const char* op_name() const { return GetOperatorName(op()); }

  Unary( Graph* graph , std::uint32_t id , Expr* opr , Operator op ,
                                                       IRInfo* info ):
    Expr  (IRTYPE_UNARY,id,graph,info),
    op_   (op)
  {
    AddOperand(opr);
  }

  virtual std::uint64_t GVNHash() const {
    auto opr = operand()->GVNHash();
    if(!opr) return 0;
    return GVNHash1(op_name(),opr);
  }

  virtual bool Equal( const Expr* that ) const {
    return that->IsUnary() && (operand()->Equal(that->AsUnary()->operand()));
  }

 private:
  Operator   op_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Unary)
};

class Binary : public Expr , public BailoutEntry {
 public:
  enum Operator {
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    POW,
    LT ,
    LE ,
    GT ,
    GE ,
    EQ ,
    NE ,
    AND,
    OR
  };
  inline static Operator BytecodeToOperator( interpreter::Bytecode );
  inline static const char* GetOperatorName( Operator );

  // Create a binary node
  inline static Binary* New( Graph* , Expr* , Expr* , Operator , IRInfo* );

 public:
  Expr*   lhs() const { return operand_list()->First(); }
  Expr*   rhs() const { return operand_list()->Last (); }
  Operator op() const { return op_;  }
  const char* op_name() const { return GetOperatorName(op()); }

  Binary( Graph* graph , std::uint32_t id , Expr* lhs , Expr* rhs , Operator op ,
                                                                    IRInfo* info ):
    Expr  (IRTYPE_BINARY,id,graph,info),
    op_   (op)
  {
    AddOperand(lhs);
    AddOperand(rhs);
  }

  virtual std::uint64_t GVNHash() const {
    auto l = lhs()->GVNHash();
    if(!l) return 0;
    auto r = rhs()->GVNHash();
    if(!r) return 0;
    return GVNHash2(op_name(),l,r);
  }

  virtual bool Equal( const Expr* that ) const {
    if(that->IsBinary()) {
      auto bin = that->AsBinary();
      return lhs()->Equal(bin->lhs()) && rhs()->Equal(bin->rhs());
    }
    return false;
  }

 private:
  Operator op_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Binary)
};

class Ternary: public Expr , public BailoutEntry {
 public:
  inline static Ternary* New( Graph* , Expr* , Expr* , Expr* , IRInfo* );

  Ternary( Graph* graph , std::uint32_t id , Expr* cond , Expr* lhs ,
                                                          Expr* rhs ,
                                                          IRInfo* info ):
    Expr  (IRTYPE_TERNARY,id,graph,info)
  {
    AddOperand(cond);
    AddOperand(lhs);
    AddOperand(rhs);
  }

  Expr* condition() const { return operand_list()->First(); }
  Expr* lhs () const { return operand_list()->Index(1); }
  Expr* rhs () const { return operand_list()->Last(); }

  virtual std::uint64_t GVNHash() const {
    auto c = condition()->GVNHash();
    if(!c) return 0;
    auto l = lhs()->GVNHash();
    if(!l) return 0;
    auto r = rhs()->GVNHash();
    if(!r) return 0;
    return GVNHash3(type_name(),c,l,r);
  }

  virtual bool Equal( const Expr* that ) const {
    if(that->IsTernary()) {
      auto u = that->AsTernary();
      return condition()->Equal(u->condition()) &&
             lhs()->Equal(u->lhs())             &&
             rhs()->Equal(u->rhs());
    }
    return false;
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Ternary)
};

// -------------------------------------------------------------------------
// upvalue get/set
// -------------------------------------------------------------------------
class UVal : public Expr {
 public:
  inline static UVal* New( Graph* , std::uint8_t );

  std::uint8_t index() const { return index_; }

  UVal( Graph* graph , std::uint32_t id , std::uint8_t index ):
    Expr  (IRTYPE_UVAL,id,graph,NULL),
    index_(index)
  {}

 private:
  std::uint8_t index_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(UVal)
};

class USet : public Expr {
 public:
  inline static USet* New( Graph* , std::uint32_t , Expr* opr , IRInfo* ,
                                                                ControlFlow* );

  std::uint32_t method() const { return method_; }
  Expr* value() const { return operand_list()->First();  }

  USet( Graph* graph , std::uint8_t id , std::uint32_t method ,
                                         Expr* value,
                                         IRInfo* info ):
    Expr   (IRTYPE_USET,id,graph,info),
    method_(method)
  {
    AddOperand(value);
  }

 private:
  std::uint32_t method_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(USet)
};

// -------------------------------------------------------------------------
// property set/get (side effect)
// -------------------------------------------------------------------------
class PGet : public Expr , public BailoutEntry {
 public:
  inline static PGet* New( Graph* , Expr* , Expr* , IRInfo* , ControlFlow* );
  Expr* object() const { return operand_list()->First(); }
  Expr* key   () const { return operand_list()->Last (); }

  PGet( Graph* graph , std::uint32_t id , Expr* object , Expr* index ,
                                                         IRInfo* info ):
    Expr  (IRTYPE_PGET,id,graph,info)
  {
    AddOperand(object);
    AddOperand(index );
  }

  virtual std::uint64_t GVNHash() const {
    auto o = object()->GVNHash();
    if(!o) return 0;
    auto k = object()->GVNHash();
    if(!k) return 0;
    return GVNHash2(type_name(),o,k);
  }

  virtual bool Equal( const Expr* that ) const {
    if(that->IsPGet()) {
      auto pget = that->AsPGet();
      return object()->Equal(pget->object()) && key()->Equal(pget->key());
    }
    return false;
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(PGet)
};

class PSet : public Expr , public BailoutEntry {
 public:
  inline static PSet* New( Graph* , Expr* , Expr* , Expr* , IRInfo* ,
                                                            ControlFlow* );
  Expr* object() const { return operand_list()->First(); }
  Expr* key   () const { return operand_list()->Index(1);}
  Expr* value () const { return operand_list()->Last (); }

  PSet( Graph* graph , std::uint32_t id , Expr* object , Expr* index ,
                                                         Expr* value ,
                                                         IRInfo* info ):
    Expr  (IRTYPE_PSET,id,graph,info)
  {
    AddOperand(object);
    AddOperand(index );
    AddOperand(value );
  }

  virtual std::uint64_t GVNHash() const {
    auto o = object()->GVNHash();
    if(!o) return 0;
    auto k = key()->GVNHash();
    if(!k) return 0;
    auto v = value()->GVNHash();
    if(!v) return 0;
    return GVNHash3(type_name(),o,k,v);
  }

  virtual bool Equal( const Expr* that ) const {
    if(that->IsPSet()) {
      auto pset = that->AsPSet();
      return object()->Equal(pset->object()) && key()->Equal(pset->key()) &&
                                                value()->Equal(pset->value()) ;
    }
    return false;
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(PSet)
};

class IGet : public Expr , public BailoutEntry {
 public:
  inline static IGet* New( Graph* , Expr* , Expr* , IRInfo* , ControlFlow* );
  Expr* object() const { return operand_list()->First(); }
  Expr* index () const { return operand_list()->Last (); }

  IGet( Graph* graph , std::uint32_t id , Expr* object , Expr* index ,
                                                         IRInfo* info ):
    Expr  (IRTYPE_IGET,id,graph,info)
  {
    AddOperand(object);
    AddOperand(index );
  }

  virtual std::uint64_t GVNHash() const {
    auto o = object()->GVNHash();
    if(!o) return 0;
    auto i = index()->GVNHash();
    if(!i) return 0;
    return GVNHash2(type_name(),o,i);
  }

  virtual bool Equal( const Expr* that ) const {
    if(that->IsIGet()) {
      auto iget = that->AsIGet();
      return object()->Equal(iget->object()) && index()->Equal(iget->index());
    }
    return false;
  }


 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(IGet)
};

class ISet : public Expr , public BailoutEntry {
 public:
  inline static ISet* New( Graph* , Expr* , Expr* , Expr* , IRInfo* ,
                                                            ControlFlow* );

  Expr* object() const { return operand_list()->First(); }
  Expr* index () const { return operand_list()->Index(1);}
  Expr* value () const { return operand_list()->Last (); }

  ISet( Graph* graph , std::uint32_t id , Expr* object , Expr* index ,
                                                         Expr* value ,
                                                         IRInfo* info ):
    Expr(IRTYPE_ISET,id,graph,info)
  {
    AddOperand(object);
    AddOperand(index );
    AddOperand(value );
  }

  virtual std::uint64_t GVNHash() const {
    auto o = object()->GVNHash();
    if(!o) return 0;
    auto i = index()->GVNHash();
    if(!i) return 0;
    auto v = value()->GVNHash();
    if(!v) return 0;
    return GVNHash3(type_name(),o,i,v);
  }

  virtual bool Equal( const Expr* that ) const {
    if(that->IsISet()) {
      auto iset = that->AsISet();
      return object()->Equal(iset->object()) && index()->Equal(iset->index()) &&
                                                value()->Equal(iset->value());
    }
    return false;
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ISet)
};

// -------------------------------------------------------------------------
// global set/get (side effect)
// -------------------------------------------------------------------------
class GGet : public Expr , public BailoutEntry {
 public:
  inline static GGet* New( Graph* , Expr* , IRInfo* , ControlFlow* );
  Expr* key() const { return operand_list()->First(); }

  GGet( Graph* graph , std::uint32_t id , Expr* name , IRInfo* info ):
    Expr  (IRTYPE_GGET,id,graph,info)
  {
    AddOperand(name);
  }

  virtual std::uint64_t GVNHash() const {
    auto k = key()->GVNHash();
    if(!k) return 0;
    return GVNHash1(type_name(),k);
  }

  virtual bool Equal( const Expr* that ) const {
    return that->IsGGet() && (key()->Equal(that->AsGGet()->key()));
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(GGet)
};

class GSet : public Expr , public BailoutEntry {
 public:
  inline static GSet* New( Graph* , Expr* key , Expr* value , IRInfo* ,
                                                              ControlFlow* );
  Expr* key () const { return operand_list()->First(); }
  Expr* value()const { return operand_list()->Last() ; }

  GSet( Graph* graph , std::uint32_t id , Expr* key , Expr* value ,
                                                      IRInfo* info ):
    Expr  (IRTYPE_GSET,id,graph,info)
  {
    AddOperand(key);
    AddOperand(value);
  }

  virtual std::uint64_t GVNHash() const {
    auto k = key()->GVNHash();
    if(!k) return 0;
    auto v = value()->GVNHash();
    if(!v) return 0;
    return GVNHash2(type_name(),k,v);
  }

  virtual bool Equal( const Expr* that ) const {
    if(that->IsGSet()) {
      auto gset = that->AsGSet();
      return key()->Equal(gset->key()) && value()->Equal(gset->value());
    }
    return false;
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(GSet)
};

// -------------------------------------------------------------------------
// Iterator node (side effect)
// -------------------------------------------------------------------------
class ItrNew : public Expr , public BailoutEntry {
 public:
  inline static ItrNew* New( Graph* , Expr* , IRInfo* , ControlFlow* );
  Expr* operand() const { return operand_list()->First(); }

  ItrNew( Graph* graph , std::uint32_t id , Expr* operand , IRInfo* info ):
    Expr  (IRTYPE_ITR_NEW,id,graph,info)
  {
    AddOperand(operand);
  }

  virtual std::uint64_t GVNHash() const {
    auto opr = operand()->GVNHash();
    if(!opr) return 0;
    return GVNHash1(type_name(),opr);
  }

  virtual bool Equal( const Expr* that ) const {
    return that->IsItrNew() && (operand()->Equal(that->AsItrNew()->operand()));
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ItrNew)
};

class ItrNext : public Expr , public BailoutEntry {
 public:
  inline static ItrNext* New( Graph* , Expr* , IRInfo* , ControlFlow* );
  Expr* operand() const { return operand_list()->First(); }

  ItrNext( Graph* graph , std::uint32_t id , Expr* operand , IRInfo* info ):
    Expr  (IRTYPE_ITR_NEXT,id,graph,info)
  {
    AddOperand(operand);
  }

  virtual std::uint64_t GVNHash() const {
    auto opr = operand()->GVNHash();
    if(!opr) return 0;
    return GVNHash1(type_name(),opr);
  }

  virtual bool Equal( const Expr* that ) const {
    return that->IsItrNew() && (operand()->Equal(that->AsItrNew()->operand()));
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ItrNext)
};

class ItrTest : public Expr , public BailoutEntry {
 public:
  inline static ItrTest* New( Graph* , Expr* , IRInfo* , ControlFlow* );
  Expr* operand() const { return operand_list()->First(); }

  ItrTest( Graph* graph , std::uint32_t id , Expr* operand , IRInfo* info ):
    Expr  (IRTYPE_ITR_TEST,id,graph,info)
  {
    AddOperand(operand);
  }

  virtual std::uint64_t GVNHash() const {
    auto opr = operand()->GVNHash();
    if(!opr) return 0;
    return GVNHash1(type_name(),opr);
  }

  virtual bool Equal( const Expr* that ) const {
    return that->IsItrNew() && (operand()->Equal(that->AsItrNew()->operand()));
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ItrTest)
};

class ItrDeref : public Expr , public BailoutEntry {
 public:
  enum {
    PROJECTION_KEY = 0,
    PROJECTION_VAL
  };

  inline static ItrDeref* New( Graph* , Expr* , IRInfo* , ControlFlow* );

  Expr* operand() const { return operand_list()->First(); }

  ItrDeref( Graph* graph , std::uint32_t id , Expr* operand , IRInfo* info ):
    Expr   (IRTYPE_ITR_DEREF,id,graph,info)
  {
    AddOperand(operand);
  }

  virtual std::uint64_t GVNHash() const {
    auto opr = operand()->GVNHash();
    if(!opr) return 0;
    return GVNHash1(type_name(),opr);
  }

  virtual bool Equal( const Expr* that ) const {
    return that->IsItrNew() && (operand()->Equal(that->AsItrNew()->operand()));
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ItrDeref)
};

// -------------------------------------------------------------------------
// Phi node
//
// A Phi node is a control flow related merged node. It can accept *at most*
// 2 input nodes.
// -------------------------------------------------------------------------
class Phi : public Expr {
 public:
  inline static Phi* New( Graph* , ControlFlow* , IRInfo* );
  inline static Phi* New( Graph* , Expr* , Expr* , ControlFlow* , IRInfo* );

  // Bounded control flow region node.
  // Each phi node is bounded to a control flow regional node
  // and by this we can easily decide which region contributs
  // to a certain input node of Phi node
  ControlFlow* region() const { return region_; }

  Phi( Graph* graph , std::uint32_t id , ControlFlow* region , IRInfo* info ):
    Expr           (IRTYPE_PHI,id,graph,info),
    region_        (region)
  {}

  virtual std::uint64_t GVNHash() const;
  virtual bool Equal( const Expr* ) const;

 private:
  ControlFlow* region_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Phi)
};

class Call : public Expr , public BailoutEntry {
 public:
  inline static Call* New( Graph* graph , Expr* , std::uint8_t , std::uint8_t ,
                                                                 IRInfo* );

  Call( Graph* graph , std::uint32_t id , Expr* obj , std::uint8_t base ,
                                                      std::uint8_t narg ,
                                                      IRInfo* info ):
    Expr  (IRTYPE_CALL,id,graph,info),
    base_ (base),
    narg_ (narg)
  {
    AddOperand(obj);
  }

 private:
  std::uint8_t base_;
  std::uint8_t narg_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Call)
};

// intrinsic function call
class ICall : public Expr {
 public:
  inline static ICall* New( Graph* , interpreter::IntrinsicCall , bool tail ,
                                                                  IRInfo* );
  // add argument back to the ICall's argument list
  void AddArgument( Expr* expr ) {
    lava_debug(NORMAL,lava_verify(
          operand_list()->size() < interpreter::GetIntrinsicCallArgumentSize(ic_)););

    AddOperand(expr);
  }

  // intrinsic call method index
  interpreter::IntrinsicCall ic() const { return ic_; }

  // whether this call is a tail call
  bool tail_call() const { return tail_call_; }

  ICall( Graph* graph , std::uint32_t id , interpreter::IntrinsicCall ic ,
                                           bool tail ,
                                           IRInfo* info ):
    Expr(IRTYPE_ICALL,id,graph,info),
    ic_ (ic),
    tail_call_(tail)
  {}

 private:
  interpreter::IntrinsicCall ic_;
  bool tail_call_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(ICall)
};

// -------------------------------------------------------------------------
// Statement
//
//  This statement node is used to represent any statement that has side
//  effect
//
// -------------------------------------------------------------------------
class Projection : public Expr {
 public:
  inline static Projection* New( Graph* , Expr* , std::uint32_t index , IRInfo* );

  Expr* operand() const { return operand_list()->First(); }

  // a specific value to indicate which part of the input operand
  // needs to be projected
  std::uint32_t index() const { return index_; }

  Projection( Graph* graph , std::uint32_t id , Expr* operand , std::uint32_t index ,
                                                                IRInfo* info ):
    Expr  (IRTYPE_PROJECTION,id,graph,info),
    index_(index)
  {
    AddOperand(operand);
  }

  virtual std::uint64_t GVNHash() const {
    return GVNHash1(type_name(),index());
  }

  virtual bool Equal( const Expr* that ) const {
    return that->IsProjection() && (that->AsProjection()->index() == index());
  }

 private:
  std::uint32_t index_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Projection)
};

class InitCls : public Expr , public BailoutEntry {
 public:
  inline static InitCls* New( Graph* , Expr* , IRInfo* );

  InitCls( Graph* graph , std::uint32_t id , Expr* key , IRInfo* info ):
    Expr (IRTYPE_INIT_CLS,id,graph,info)
  {
    AddOperand(key);
  }

  Expr* key() const { return operand_list()->First(); }

  virtual std::uint64_t GVNHash() const {
    auto k = key()->GVNHash();
    if(!k) return 0;
    return GVNHash1(type_name(),k);
  }

  virtual bool Equal( const Expr* that ) const {
    return that->IsInitCls() && (key()->Equal(that->AsInitCls()->key()));
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(InitCls)
};

// OSR related
class OSRLoad : public Expr {
 public:
  inline static OSRLoad* New( Graph* , std::uint32_t );

  // Offset in sizeof(Value)/8 bytes to load this value from osr input buffer
  std::uint32_t index() const { return index_; }

  OSRLoad( Graph* graph , std::uint32_t id , std::uint32_t index ):
    Expr  ( IRTYPE_OSR_LOAD , id , graph , NULL ),
    index_(index)
  {}

  virtual std::uint64_t GVNHash() const {
    return GVNHash1(type_name(),index());
  }

  virtual bool Equal( const Expr* that ) const {
    return that->IsOSRLoad() && (that->AsOSRLoad()->index() == index());
  }

 private:
  std::uint32_t index_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(OSRLoad)
};

// Effect node
//
// This node is mainly used to represent side effect can be observed
// introduced by certain type of operations. This node is mainly used
// to mark side effect generated by function call in none OSR graph. In
// OSR graph, everything has side effect and we use another way to mark
// side effect
class Effect : public Expr {
 public:
  inline static Effect* New( Graph* , Expr* , Expr* , IRInfo* );

  Expr* receiver() const { return operand_list()->First(); }
  Expr* applier () const { return operand_list()->Last (); }

  Effect( Graph* graph , std::uint32_t id , Expr* receiver ,
                                            Expr* applier  ,
                                            IRInfo* info ):
    Expr(IRTYPE_EFFECT,id,graph,info)
  {
    AddOperand(receiver);
    AddOperand(applier);
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Effect)
};

// Checkpoint node
//
// A checkpoint node will capture the VM/Interpreter state. When an irnode
// needs a bailout for speculative execution , it needs Checkpoint node.
//
// The checkpoint node will impact CFG generation/scheduling , anynode that
// is referenced by checkpoint must be scheduled before the node that actually
// reference this checkpoint node.
//
// For VM/Interpreter state , there're only 3 types of states
//
// 1) a register stack state , represented by StackSlot node
// 2) a upvalue state , represented by UValSlot node
// 3) a global value state , currently we don't have any optimization against
//    global values, so they are not needed to be captured in the checkpoint
//    they are more like volatile in C/C++ , always read from its memory and
//    write through back to where it is
//
class Checkpoint : public Expr {
 public:
  inline static Checkpoint* New( Graph* );

  // add a StackSlot into the checkpoint
  inline void AddStackSlot( Expr* , std::uint32_t );

  // add a upvalue slot
  inline void AddUValSlot ( Expr* , std::uint32_t );

  Checkpoint( Graph* graph , std::uint32_t id ):
    Expr(IRTYPE_CHECKPOINT,id,graph,NULL)
  {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Checkpoint)
};

// StackSlot node
//
// Represent a value must be flushed back a certain stack slot when will
// bailout from the interpreter
//
// It is only used inside of the checkpoint nodes
class StackSlot : public Expr {
 public:
  inline static StackSlot* New( Graph* , Expr* , std::uint32_t );
  std::uint32_t index() const { return index_; }
  Expr* expr() const { return operand_list()->First(); }

  StackSlot( Graph* graph , std::uint32_t id , Expr* expr , std::uint32_t index ):
    Expr(IRTYPE_STACK_SLOT,id,graph,NULL),
    index_(index)
  {
    AddOperand(expr);
  }

 private:
  std::uint32_t index_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(StackSlot)
};

// UValSlot node
//
// Represent a value must be flushed back to the upvalue array of the
// calling closure
class UValSlot : public Expr {
 public:
  inline static UValSlot* New( Graph* , Expr* , std::uint32_t );
  std::uint32_t index() const { return index_; }
  Expr* expr() const { return operand_list()->First(); }

  UValSlot( Graph* graph , std::uint32_t id , Expr* expr , std::uint32_t index ):
    Expr(IRTYPE_UVAL_SLOT,id,graph,NULL),
    index_(index)
  {
    AddOperand(expr);
  }
 private:
  std::uint32_t index_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(UValSlot)
};

class TestIndexOOB : public Expr {
 public:
  inline static TestIndexOOB* New( Graph* , Expr* , Expr* , IRInfo* );

  Expr* object() const { return operand_list()->First(); }
  Expr* index () const { return operand_list()->Last (); }

  TestIndexOOB( Graph* graph , std::uint32_t id , Expr* obj , Expr* idx ,
                                                              IRInfo* info ):
    Expr(IRTYPE_TEST_INDEXOOB,id,graph,info)
  {
    AddOperand(obj);
    AddOperand(idx);
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(TestIndexOOB)
};

class TestType : public Expr {
 public:
  enum TypeCategory {
    TT_FLOAT64,
    TT_BOOLEAN,
    TT_NIL,
    TT_SSTRING,
    TT_LSTRING,
    TT_STRING,
    TT_LIST,
    TT_OBJECT,
    TT_EXTENSION,

    TT_COMPONENT,       // test whether this object can have component
    TT_INDEXABLE,       // test whether this object can be indexable
    TT_PROPERTIABLE     // test whether this object can be used with dot/property
  };

  inline static const char* GetTypeCategoryName( TypeCategory );

  inline static TestType* New( Graph* , TypeCategory , Expr* , IRInfo* );

  TypeCategory type_category() const { return type_category_; }

  const char* type_category_name() const { return GetTypeCategoryName(type_category_); }

  Expr* object() const { return operand_list()->First(); }

  TestType( Graph* graph , std::uint32_t id , TypeCategory tc ,
                                              Expr* obj,
                                              IRInfo* info ):
    Expr(IRTYPE_TEST_TYPE,id,graph,info),
    type_category_(tc)
  {
    AddOperand(obj);
  }

 private:
  TypeCategory type_category_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(TestType)
};

// -------------------------------------------------------------------------
// Control Flow
//
//  The control flow node needs to support one additional important
//  feature , mutation/modification/deletion of existed control flow
//  graph.
//
// -------------------------------------------------------------------------
class ControlFlow : public Node {
 public:
  const zone::Vector<ControlFlow*>* backward_edge() const {
    return &backward_edge_;
  }

  zone::Vector<ControlFlow*>* backedge_edge() {
    return &backward_edge_;
  }

  void AddBackwardEdge( ControlFlow* edge ) {
    backward_edge_.Add(zone(),edge);
  }

  // Effective expression doesn't belong to certain expression chain
  //
  // Like free function invocation, they are not part certain expression chain
  // but they have visiable effects.
  //
  // All these types of expressions are stored inside of the effect_expr list
  // to be used later on for code generation
  const EffectList* effect_expr() const {
    return &effect_expr_;
  }

  EffectList* effect_expr() {
    return &effect_expr_;
  }

  void AddEffectExpr( Expr* node ) {
    auto itr = effect_expr_.PushBack(zone(),node);
    node->set_effect(EffectEdge(this,itr));
  }

  // OperandList
  //
  // All control flow's related data input should be stored via this list
  // since this list supports expression substitution/replacement. It is
  // used in all optimization pass

  OperandList* operand_list() {
    return &operand_list_;
  }

  const OperandList* operand_list() const {
    return &operand_list_;
  }

  void AddOperand( Expr* node ) {
    auto itr = operand_list()->PushBack(zone(),node);
    node->AddRef(this,itr);
  }

  ControlFlow( IRType type , std::uint32_t id , Graph* graph , ControlFlow* parent = NULL ):
    Node(type,id,graph),
    backward_edge_   (),
    effect_expr_     (),
    operand_list_    ()
  {
    if(parent) backward_edge_.Add(zone(),parent);
  }

 private:
  zone::Vector<ControlFlow*> backward_edge_;
  EffectList                 effect_expr_;
  OperandList                operand_list_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(ControlFlow)
};


class Region: public ControlFlow {
 public:
  inline static Region* New( Graph* );
  inline static Region* New( Graph* , ControlFlow* parent );

  Region( Graph* graph , std::uint32_t id ):
    ControlFlow(IRTYPE_REGION,id,graph)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Region)
};

// --------------------------------------------------------------------------
//
// Loop related blocks
//
// --------------------------------------------------------------------------
class LoopHeader : public ControlFlow {
 public:
  inline static LoopHeader* New( Graph* , ControlFlow* );

  Expr* condition() const { return operand_list()->First(); }

  void set_condition( Expr* condition ) {
    lava_debug(NORMAL,lava_verify(operand_list()->empty()););
    AddOperand(condition);
  }

  LoopHeader( Graph* graph , std::uint32_t id , ControlFlow* region ):
    ControlFlow(IRTYPE_LOOP_HEADER,id,graph,region)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(LoopHeader);
};

class Loop : public ControlFlow {
 public:
  inline static Loop* New( Graph* );

  Loop( Graph* graph , std::uint32_t id ):
    ControlFlow(IRTYPE_LOOP,id,graph)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Loop)
};

class LoopExit : public ControlFlow {
 public:
  inline static LoopExit* New( Graph* , Expr* );
  Expr* condition() const { return operand_list()->First(); }

  LoopExit( Graph* graph , std::uint32_t id , Expr* cond ):
    ControlFlow(IRTYPE_LOOP_EXIT,id,graph)
  {
    AddOperand(cond);
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(LoopExit)
};

// -----------------------------------------------------------------------
//
// Branch
//
// -----------------------------------------------------------------------
class If : public ControlFlow {
 public:
  inline static If* New( Graph* , Expr* , ControlFlow* );
  Expr* condition() const { return operand_list()->First(); }

  If( Graph* graph , std::uint32_t id , Expr* cond , ControlFlow* region ):
    ControlFlow(IRTYPE_IF,id,graph,region)
  {
    AddOperand(cond);
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(If)
};

class IfTrue : public ControlFlow {
 public:
  inline static IfTrue* New( Graph* , ControlFlow* );
  inline static IfTrue* New( Graph* );

  IfTrue( Graph* graph , std::uint32_t id , ControlFlow* region ):
    ControlFlow(IRTYPE_IF_TRUE,id,graph,region)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(IfTrue)
};

class IfFalse: public ControlFlow {
 public:
  inline static IfFalse * New( Graph* , ControlFlow* );
  inline static IfFalse * New( Graph* );

  IfFalse( Graph* graph , std::uint32_t id , ControlFlow* region ):
    ControlFlow(IRTYPE_IF_FALSE,id,graph,region)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(IfFalse)
};

class Jump : public ControlFlow {
 public:
  inline static Jump* New( Graph* , const std::uint32_t* , ControlFlow* );
  // which target this jump jumps to
  ControlFlow* target() const { return target_; }
  inline bool TrySetTarget( const std::uint32_t* , ControlFlow* );

  Jump( Graph* graph , std::uint32_t id , ControlFlow* region ,
                                          const std::uint32_t* bytecode_bc ):
    ControlFlow(IRTYPE_JUMP,id,graph,region),
    target_(NULL),
    bytecode_pc_(bytecode_bc)
  {}

 private:
  ControlFlow* target_; // where this Jump node jumps to
  const std::uint32_t* bytecode_pc_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Jump)
};

class Guard : public ControlFlow {
 public:
  inline static Guard* New( Graph* , Expr* , ControlFlow* );

  Expr* test() const { return operand_list()->First(); }

  Guard( Graph* graph , std::uint32_t id , Expr* t , ControlFlow* r ):
    ControlFlow(IRTYPE_GUARD,id,graph,r)
  {
    AddOperand(t);
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Guard)
};

class Fail : public ControlFlow {
 public:
  inline static Fail* New( Graph* );

  Fail( Graph* graph , std::uint32_t id ):
    ControlFlow(IRTYPE_FAIL,id,graph)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Fail)
};

class Success : public ControlFlow {
 public:
  inline static Success* New( Graph* );

  Expr* return_value() const { return operand_list()->First(); }

  void set_return_value( Expr* return_value ) {
    lava_debug(NORMAL,lava_verify(operand_list()->empty()););
    AddOperand(return_value);
  }

  Success( Graph* graph , std::uint32_t id ):
    ControlFlow  (IRTYPE_END,id,graph)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Success)
};


class Return : public ControlFlow {
 public:
  inline static Return* New( Graph* , Expr* , ControlFlow* );
  Expr* value() const { return operand_list()->First(); }

  Return( Graph* graph , std::uint32_t id , Expr* value , ControlFlow* region ):
    ControlFlow(IRTYPE_RETURN,id,graph,region)
  {
    AddOperand(value);
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Return)
};

// Special node of the graph
class Start : public ControlFlow {
 public:
  inline static Start* New( Graph* );

  Start( Graph* graph , std::uint32_t id ):
    ControlFlow(IRTYPE_START,id,graph)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Start)
};

class End : public ControlFlow {
 public:
  inline static End* New( Graph* , Success* , Fail* );

  Success* success() const { return backward_edge()->First()->AsSuccess(); }
  Fail*    fail()    const { return backward_edge()->Last ()->AsFail   (); }

  End( Graph* graph , std::uint32_t id , Success* s , Fail* f ):
    ControlFlow(IRTYPE_END,id,graph)
  {
    AddBackwardEdge(s);
    AddBackwardEdge(f);
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(End)
};

class Trap : public ControlFlow {
  enum {
    FRAME_STATE_UNINIT = -1
  };

 public:
  inline static Trap* New( Graph* , ControlFlow* region );

  void set_frame_state_index( std::uint32_t idx ) {
    frame_state_index_ = static_cast<std::int32_t>(idx);
  }

  std::uint32_t frame_state_index() const {
    lava_debug(NORMAL,lava_verify(HasFrameState()););
    return static_cast<std::uint32_t>(frame_state_index_);
  }
  bool HasFrameState() const { return frame_state_index_ >= 0; }
  void ClearFrameState()     { frame_state_index_ = FRAME_STATE_UNINIT; }

  Trap( Graph* graph , std::uint32_t id , ControlFlow* region ):
    ControlFlow(IRTYPE_TRAP,id,graph,region),
    frame_state_index_(FRAME_STATE_UNINIT)
  {}
 private:

  std::int32_t frame_state_index_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Trap)
};

class OSRStart : public ControlFlow {
 public:
  inline static OSRStart* New( Graph* );

  OSRStart( Graph* graph  , std::uint32_t id ):
    ControlFlow(IRTYPE_OSR_START,id,graph)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(OSRStart)
};

class OSREnd : public ControlFlow {
 public:
  inline static OSREnd* New( Graph* , Success* succ , Fail* f );

  Success* success() const { return backward_edge()->First()->AsSuccess(); }
  Fail*    fail   () const { return backward_edge()->Last()->AsFail(); }

  OSREnd( Graph* graph , std::uint32_t id , Success* succ , Fail* f ):
    ControlFlow(IRTYPE_OSR_END,id,graph)
  {
    AddBackwardEdge(succ);
    AddBackwardEdge(f);
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(OSREnd)
};

// ========================================================
//
// Graph
//
// ========================================================
class Graph {
 public:
  Graph():
    zone_          (),
    start_         (NULL),
    end_           (NULL),
    prototype_info_(),
    id_            ()
  {}

  // initialize the *graph* object with start and end
  void Initialize( Start* start    , End* end );
  void Initialize( OSRStart* start , OSREnd* end  );

 public: // getter and setter
  ControlFlow* start() const { return start_; }
  ControlFlow* end  () const { return end_;   }

  zone::Zone* zone()   { return &zone_; }
  const zone::Zone* zone() const { return &zone_; }

  std::uint32_t MaxID() const { return id_; }
  std::uint32_t AssignID() { return id_++; }

  // check whether the graph is OSR construction graph
  bool IsOSR() const {
    lava_debug(NORMAL,lava_verify(start_););
    return start_->IsOSRStart();
  }
 public:
  std::uint32_t AddPrototypeInfo( const Handle<Closure>& cls ,
      std::uint32_t base ) {
    prototype_info_.Add(zone(),PrototypeInfo(base,cls));
    return static_cast<std::uint32_t>(prototype_info_.size()-1);
  }

  const PrototypeInfo& GetProrotypeInfo( std::uint32_t index ) const {
    return prototype_info_[index];
  }
 public: // string dedup
  zone::String* NewString( const char* data , std::size_t size );

 public: // static helper function

  struct DotFormatOption {
    bool checkpoint;
    DotFormatOption() : checkpoint(false) {}
  };

  // Print the graph into dot graph representation which can be visualized by
  // using graphviz or other similar tools
  static std::string PrintToDotFormat( const Graph& ,
                                       const DotFormatOption& opt = DotFormatOption() );

 private:
  zone::Zone                  zone_;
  ControlFlow*                start_;
  ControlFlow*                end_;

  zone::Vector<PrototypeInfo> prototype_info_;
  std::uint32_t               id_;

  friend class GraphBuilder;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Graph)
};


// --------------------------------------------------------------------------
// A simple stack tracks which node has been added into the stack. This avoids
// adding element that is already in the list back to list again
class SetList {
 public:
  explicit SetList( const Graph& );
  bool Push( Node* node );
  void Pop();
  Node* Top() const { return array_.back(); }
  bool empty() const { return array_.empty(); }
  void Clear() { array_.clear(); BitSetReset(&existed_); }
 private:
  DynamicBitSet existed_;
  std::vector<Node*> array_ ;
};

// --------------------------------------------------------------------------
// A simple stack tracks which node has been added for at least once. This
// avoids adding element that has been added once to list again
class OnceList {
 public:
  explicit OnceList( const Graph& );
  bool Push( Node* node );
  void Pop();
  Node* Top() const { return array_.back(); }
  bool empty() const { return array_.empty(); }
  void Clear() { array_.clear(); BitSetReset(&existed_); }
 private:
  DynamicBitSet existed_;
  std::vector<Node*> array_;
};

// For concept check when used with dispatch routine
struct ControlFlowIterator  {};
struct ExprIterator   {};

template< typename T >
constexpr bool IsControlFlowIterator() {
  return std::is_base_of<ControlFlowIterator,T>::value;
}

template< typename T >
constexpr bool IsExprIterator() {
  return std::is_base_of<ExprIterator,T>::value;
}

// --------------------------------------------------------------------------
// A graph dfs iterator that iterate all control flow graph node in DFS order
// the expression node simply ignored and left the user to use whatever method
// they like to iterate/visit them
class ControlFlowDFSIterator : public ControlFlowIterator {
 public:
  ControlFlowDFSIterator( const Graph& graph ):
    stack_  (graph),
    graph_  (&graph),
    next_   (NULL)
  {
    stack_.Push(graph.end());
    Move();
  }

  // whether there's another control flow graph node needs to visit
  bool HasNext() const { return next_ != NULL; }

  // move the control flow graph node to next one
  bool Move();

  // get the current control flow graph node in DFS order
  ControlFlow* value() const { lava_debug(NORMAL,lava_verify(HasNext());); return next_; }

 private:
  OnceList stack_;
  const Graph* graph_;
  ControlFlow* next_;
};

// ---------------------------------------------------------------------------
// A graph bfs iterator
class ControlFlowBFSIterator : public ControlFlowIterator {
 public:
  ControlFlowBFSIterator( const Graph& graph ):
    stack_  (graph),
    graph_  (&graph),
    next_   (NULL)
  {
    stack_.Push(graph.end());
    Move();
  }

  bool HasNext() const { return next_ != NULL; }

  bool Move();

  ControlFlow* value() const { lava_debug(NORMAL,lava_verify(HasNext());); return next_; }

 private:
  OnceList stack_;
  const Graph* graph_;
  ControlFlow* next_;
};

// -------------------------------------------------------------------------------
// A graph's edge iterator. It will not guarantee any order except visit the edge
// exactly once
class ControlFlowEdgeIterator {
 public:
  struct Edge {
    ControlFlow* from;
    ControlFlow* to;
    Edge( ControlFlow* f , ControlFlow* t ): from(f) , to(t) {}
    Edge(): from(NULL), to(NULL) {}

    void Clear() { from = NULL; to = NULL; }
    bool empty() const { return from == NULL; }
  };
 public:
  ControlFlowEdgeIterator( const Graph& graph ):
    stack_  (graph),
    results_(),
    graph_  (&graph),
    next_   ()
  {
    stack_.Push(graph.end()) ;
    Move();
  }

  bool HasNext() const { return !next_.empty(); }

  bool Move();

  const Edge& value() const { lava_debug(NORMAL,lava_verify(HasNext());); return next_; }

 private:
  OnceList stack_;
  std::deque<Edge> results_;
  const Graph* graph_;
  Edge next_;
};

// ---------------------------------------------------------------------------------
// An expression iterator. It will visit a expression in DFS order
class ExprDFSIterator : public ExprIterator {
 public:
  ExprDFSIterator( const Graph& graph , Expr* node ):
    root_(node),
    next_(NULL),
    stack_(graph)
  { stack_.Push(node); Move(); }

  ExprDFSIterator( const Graph& graph ):
    root_(NULL),
    next_(NULL),
    stack_(graph)
  {}

 public:

  void Reset( Expr* node ) {
    root_ = node;
    next_ = NULL;
    stack_.Clear();
    Move();
  }

  bool HasNext() const { return next_ != NULL; }

  bool Move();

  Expr* value() const { lava_debug(NORMAL,lava_verify(HasNext());); return next_; }

 private:
  Expr* root_;
  Expr* next_;
  OnceList stack_;
};


// =========================================================================
//
// Inline Functions
//
// =========================================================================

#define __(A,B,...)                           \
  inline A* Node::As##A() {                   \
    lava_debug(NORMAL,lava_verify(Is##A());); \
    return static_cast<A*>(this);             \
  }                                           \
  inline const A* Node::As##A() const {       \
    lava_debug(NORMAL,lava_verify(Is##A());); \
    return static_cast<const A*>(this);       \
  }

CBASE_IR_LIST(__)

#undef __ // __

inline const zone::String& Node::AsZoneString() const {
  lava_debug(NORMAL,lava_verify(IsString()););
  return IsLString() ? *AsLString()->value() :
                       *AsSString()->value() ;
}

inline void Expr::AddOperand( Expr* node ) {
  auto itr = operand_list()->PushBack(zone(),node);
  node->AddRef(this,itr);
}

inline ControlFlow* Node::AsControlFlow() {
  lava_debug(NORMAL,lava_verify(IsControlFlow()););
  return static_cast<ControlFlow*>(this);
}

inline const ControlFlow* Node::AsControlFlow() const {
  lava_debug(NORMAL,lava_verify(IsControlFlow()););
  return static_cast<const ControlFlow*>(this);
}

inline Expr* Node::AsExpr() {
  lava_debug(NORMAL,lava_verify(IsExpr()););
  return static_cast<Expr*>(this);
}

inline const Expr* Node::AsExpr() const {
  lava_debug(NORMAL,lava_verify(IsExpr()););
  return static_cast<const Expr*>(this);
}

inline zone::Zone* Node::zone() const {
  return graph_->zone();
}

inline Arg* Arg::New( Graph* graph , std::uint32_t index ) {
  return graph->zone()->New<Arg>(graph,graph->AssignID(),index);
}

inline Float64* Float64::New( Graph* graph , double value , IRInfo* info ) {
  return graph->zone()->New<Float64>(graph,graph->AssignID(),value,info);
}

inline Boolean* Boolean::New( Graph* graph , bool value , IRInfo* info ) {
  return graph->zone()->New<Boolean>(graph,graph->AssignID(),value,info);
}

inline LString* LString::New( Graph* graph , const LongString& str , IRInfo* info ) {
  return graph->zone()->New<LString>(graph,graph->AssignID(),
      zone::String::New(graph->zone(),static_cast<const char*>(str.data()),str.size),info);
}

inline SString* SString::New( Graph* graph , const SSO& str , IRInfo* info ) {
  return graph->zone()->New<SString>(graph,graph->AssignID(),
      zone::String::New(graph->zone(),static_cast<const char*>(str.data()),str.size()),info);
}

inline Nil* Nil::New( Graph* graph , IRInfo* info ) {
  return graph->zone()->New<Nil>(graph,graph->AssignID(),info);
}

inline IRList* IRList::New( Graph* graph , std::size_t size , IRInfo* info ) {
  return graph->zone()->New<IRList>(graph,graph->AssignID(),size,info);
}

inline IRObjectKV* IRObjectKV::New( Graph* graph , Expr* key , Expr* val , IRInfo* info ) {
  return graph->zone()->New<IRObjectKV>(graph,graph->AssignID(),key,val,info);
}

inline IRObject* IRObject::New( Graph* graph , std::size_t size , IRInfo* info ) {
  return graph->zone()->New<IRObject>(graph,graph->AssignID(),size,info);
}

inline Binary* Binary::New( Graph* graph , Expr* lhs , Expr* rhs, Operator op ,
                                                                  IRInfo* info ) {
  return graph->zone()->New<Binary>(graph,graph->AssignID(),lhs,rhs,op,info);
}

inline Binary::Operator Binary::BytecodeToOperator( interpreter::Bytecode op ) {
  switch(op) {
    case interpreter::BC_ADDRV: case interpreter::BC_ADDVR: case interpreter::BC_ADDVV: return ADD;
    case interpreter::BC_SUBRV: case interpreter::BC_SUBVR: case interpreter::BC_SUBVV: return SUB;
    case interpreter::BC_MULRV: case interpreter::BC_MULVR: case interpreter::BC_MULVV: return MUL;
    case interpreter::BC_DIVRV: case interpreter::BC_DIVVR: case interpreter::BC_DIVVV: return DIV;
    case interpreter::BC_MODRV: case interpreter::BC_MODVR: case interpreter::BC_MODVV: return MOD;
    case interpreter::BC_POWRV: case interpreter::BC_POWVR: case interpreter::BC_POWVV: return POW;
    case interpreter::BC_LTRV : case interpreter::BC_LTVR : case interpreter::BC_LTVV : return LT ;
    case interpreter::BC_LERV : case interpreter::BC_LEVR : case interpreter::BC_LEVV : return LE ;
    case interpreter::BC_GTRV : case interpreter::BC_GTVR : case interpreter::BC_GTVV : return GT ;
    case interpreter::BC_GERV : case interpreter::BC_GEVR : case interpreter::BC_GEVV : return GE ;

    case interpreter::BC_EQRV : case interpreter::BC_EQVR : case interpreter::BC_EQSV :
    case interpreter::BC_EQVS: case interpreter::BC_EQVV:
      return EQ;

    case interpreter::BC_NERV : case interpreter::BC_NEVR : case interpreter::BC_NESV :
    case interpreter::BC_NEVS: case interpreter::BC_NEVV:
      return NE;

    case interpreter::BC_AND: return AND;
    case interpreter::BC_OR : return OR;
    default:
      lava_unreachF("unknown bytecode %s",interpreter::GetBytecodeName(op));
      break;
  }
  return ADD;
}

inline const char* Binary::GetOperatorName( Operator op ) {
  switch(op) {
    case ADD :    return "add";
    case SUB :    return "sub";
    case MUL :    return "mul";
    case DIV :    return "div";
    case MOD :    return "mod";
    case POW :    return "pow";
    case LT  :    return "lt" ;
    case LE  :    return "le" ;
    case GT  :    return "gt" ;
    case GE  :    return "ge" ;
    case EQ  :    return "eq" ;
    case NE  :    return "ne" ;
    case AND :    return "and";
    case OR  :    return "or";
    default:
      lava_die(); return NULL;
  }
}

inline Unary::Operator Unary::BytecodeToOperator( interpreter::Bytecode bc ) {
  if(bc == interpreter::BC_NEGATE)
    return MINUS;
  else
    return NOT;
}

inline const char* Unary::GetOperatorName( Operator op ) {
  if(op == MINUS)
    return "minus";
  else
    return "not";
}

inline Unary* Unary::New( Graph* graph , Expr* opr , Operator op , IRInfo* info ) {
  return graph->zone()->New<Unary>(graph,graph->AssignID(),opr,op,info);
}

inline Ternary* Ternary::New( Graph* graph , Expr* cond , Expr* lhs , Expr* rhs ,
                                                                      IRInfo* info ) {
  return graph->zone()->New<Ternary>(graph,graph->AssignID(),cond,lhs,rhs,info);
}

inline UVal* UVal::New( Graph* graph , std::uint8_t index ) {
  return graph->zone()->New<UVal>(graph,graph->AssignID(),index);
}

inline USet* USet::New( Graph* graph , std::uint32_t method , Expr* opr , IRInfo* info ,
                                                                          ControlFlow* region ) {
  auto ret = graph->zone()->New<USet>(graph,graph->AssignID(),method,opr,info);
  region->AddEffectExpr(ret);
  return ret;
}

inline PGet* PGet::New( Graph* graph , Expr* obj , Expr* key , IRInfo* info ,
                                                               ControlFlow* region ) {
  auto ret = graph->zone()->New<PGet>(graph,graph->AssignID(),obj,key,info);
  region->AddEffectExpr(ret);
  return ret;
}

inline PSet* PSet::New( Graph* graph , Expr* obj , Expr* key , Expr* value ,
                                                               IRInfo* info,
                                                               ControlFlow* region ) {
  auto ret = graph->zone()->New<PSet>(graph,graph->AssignID(),obj,key,value,info);
  region->AddEffectExpr(ret);
  return ret;
}

inline IGet* IGet::New( Graph* graph , Expr* obj, Expr* key , IRInfo* info ,
                                                              ControlFlow* region ) {
  auto ret = graph->zone()->New<IGet>(graph,graph->AssignID(),obj,key,info);
  region->AddEffectExpr(ret);
  return ret;
}

inline ISet* ISet::New( Graph* graph , Expr* obj , Expr* key , Expr* val ,
                                                               IRInfo* info ,
                                                               ControlFlow* region ) {
  auto ret = graph->zone()->New<ISet>(graph,graph->AssignID(),obj,key,val,info);
  region->AddEffectExpr(ret);
  return ret;
}

inline GGet* GGet::New( Graph* graph , Expr* key , IRInfo* info , ControlFlow* region ) {
  auto ret = graph->zone()->New<GGet>(graph,graph->AssignID(),key,info);
  region->AddEffectExpr(ret);
  return ret;
}

inline GSet* GSet::New( Graph* graph , Expr* key, Expr* value , IRInfo* info ,
                                                                ControlFlow* region ) {
  auto ret = graph->zone()->New<GSet>(graph,graph->AssignID(),key,value,info);
  region->AddEffectExpr(ret);
  return ret;
}

inline ItrNew* ItrNew::New( Graph* graph , Expr* operand , IRInfo* info ,
                                                           ControlFlow* region ) {
  auto ret = graph->zone()->New<ItrNew>(graph,graph->AssignID(),operand,info);
  region->AddEffectExpr(ret);
  return ret;
}

inline ItrNext* ItrNext::New( Graph* graph , Expr* operand , IRInfo* info ,
                                                             ControlFlow* region ) {
  auto ret = graph->zone()->New<ItrNext>(graph,graph->AssignID(),operand,info);
  region->AddEffectExpr(ret);
  return ret;
}

inline ItrTest* ItrTest::New( Graph* graph , Expr* operand , IRInfo* info ,
                                                             ControlFlow* region ) {
  auto ret = graph->zone()->New<ItrTest>(graph,graph->AssignID(),operand,info);
  region->AddEffectExpr(ret);
  return ret;
}

inline ItrDeref* ItrDeref::New( Graph* graph , Expr* operand , IRInfo* info ,
                                                               ControlFlow* region ) {
  auto ret = graph->zone()->New<ItrDeref>(graph,graph->AssignID(),operand,info);
  region->AddEffectExpr(ret);
  return ret;
}

inline Phi* Phi::New( Graph* graph , Expr* lhs , Expr* rhs , ControlFlow* region ,
                                                             IRInfo* info ) {
  auto ret = graph->zone()->New<Phi>(graph,graph->AssignID(),region,info);
  ret->AddOperand(lhs);
  ret->AddOperand(rhs);
  return ret;
}

inline Phi* Phi::New( Graph* graph , ControlFlow* region , IRInfo* info ) {
  return graph->zone()->New<Phi>(graph,graph->AssignID(),region,info);
}

inline ICall* ICall::New( Graph* graph , interpreter::IntrinsicCall ic ,
                                         bool tc,
                                         IRInfo* info ) {
  return graph->zone()->New<ICall>(graph,graph->AssignID(),ic,tc,info);
}

inline LoadCls* LoadCls::New( Graph* graph , std::uint32_t ref , IRInfo* info ) {
  return graph->zone()->New<LoadCls>(graph,graph->AssignID(),ref,info);
}

inline Projection* Projection::New( Graph* graph , Expr* operand , std::uint32_t index ,
                                                                   IRInfo* info ) {
  return graph->zone()->New<Projection>(graph,graph->AssignID(),operand,index,info);
}

inline OSRLoad* OSRLoad::New( Graph* graph , std::uint32_t index ) {
  return graph->zone()->New<OSRLoad>(graph,graph->AssignID(),index);
}

inline Effect* Effect::New( Graph* graph , Expr* receiver , Expr* applier ,
                                                            IRInfo* info ) {
  return graph->zone()->New<Effect>(graph,graph->AssignID(),receiver,applier,info);
}

inline Checkpoint* Checkpoint::New( Graph* graph ) {
  return graph->zone()->New<Checkpoint>(graph,graph->AssignID());
}

inline void Checkpoint::AddStackSlot( Expr* val , std::uint32_t index ) {
  AddOperand(StackSlot::New(graph(),val,index));
}

inline void Checkpoint::AddUValSlot ( Expr* val , std::uint32_t index ) {
  AddOperand(UValSlot::New(graph(),val,index));
}

inline TestIndexOOB* TestIndexOOB::New( Graph* graph , Expr* object , Expr* key ,
                                                                      IRInfo* info ) {
  return graph->zone()->New<TestIndexOOB>(graph,graph->AssignID(),object,key,info);
}

inline TestType* TestType::New( Graph* graph , TypeCategory tc , Expr* object ,
                                                                 IRInfo* info ) {
  return graph->zone()->New<TestType>(graph,graph->AssignID(),tc,object,info);
}

inline const char* TestType::GetTypeCategoryName( TypeCategory tc ) {
  switch(tc) {
    case TT_FLOAT64: return "tt_float64";
    case TT_BOOLEAN: return "tt_boolean";
    case TT_NIL    : return "tt_nil";
    case TT_STRING : return "tt_string";
    case TT_SSTRING: return "tt_sstring";
    case TT_LSTRING: return "tt_lstring";
    case TT_LIST   : return "tt_list";
    case TT_OBJECT : return "tt_object";
    case TT_EXTENSION: return "tt_extension";
    case TT_COMPONENT: return "tt_component";
    case TT_INDEXABLE: return "tt_indexable";
    case TT_PROPERTIABLE: return "tt_propertiable";
    default: lava_die(); return NULL;
  }
}

inline StackSlot* StackSlot::New( Graph* graph , Expr* expr , std::uint32_t index ) {
  return graph->zone()->New<StackSlot>(graph,graph->AssignID(),expr,index);
}

inline UValSlot* UValSlot::New( Graph* graph , Expr* expr , std::uint32_t index ) {
  return graph->zone()->New<UValSlot>(graph,graph->AssignID(),expr,index);
}

inline Region* Region::New( Graph* graph ) {
  return graph->zone()->New<Region>(graph,graph->AssignID());
}

inline Region* Region::New( Graph* graph , ControlFlow* parent ) {
  auto ret = New(graph);
  ret->AddBackwardEdge(parent);
  return ret;
}

inline LoopHeader* LoopHeader::New( Graph* graph , ControlFlow* parent ) {
  return graph->zone()->New<LoopHeader>(graph,graph->AssignID(),parent);
}

inline Loop* Loop::New( Graph* graph ) { return graph->zone()->New<Loop>(graph,graph->AssignID()); }

inline LoopExit* LoopExit::New( Graph* graph , Expr* condition ) {
  return graph->zone()->New<LoopExit>(graph,graph->AssignID(),condition);
}

inline If* If::New( Graph* graph , Expr* condition , ControlFlow* parent ) {
  return graph->zone()->New<If>(graph,graph->AssignID(),condition,parent);
}

inline IfTrue* IfTrue::New( Graph* graph , ControlFlow* parent ) {
  return graph->zone()->New<IfTrue>(graph,graph->AssignID(),parent);
}

inline IfTrue* IfTrue::New( Graph* graph ) {
  return IfTrue::New(graph,NULL);
}

inline IfFalse* IfFalse::New( Graph* graph , ControlFlow* parent ) {
  return graph->zone()->New<IfFalse>(graph,graph->AssignID(),parent);
}

inline IfFalse* IfFalse::New( Graph* graph ) {
  return IfFalse::New(graph,NULL);
}

inline Jump* Jump::New( Graph* graph , const std::uint32_t* pc , ControlFlow* parent ) {
  return graph->zone()->New<Jump>(graph,graph->AssignID(),parent,pc);
}

inline Guard* Guard::New( Graph* graph , Expr* test , ControlFlow* parent ) {
  return graph->zone()->New<Guard>(graph,graph->AssignID(),test,parent);
}

inline Fail* Fail::New( Graph* graph ) {
  return graph->zone()->New<Fail>(graph,graph->AssignID());
}

inline Success* Success::New( Graph* graph ) {
  return graph->zone()->New<Success>(graph,graph->AssignID());
}

inline bool Jump::TrySetTarget( const std::uint32_t* bytecode_pc , ControlFlow* target ) {
  if(bytecode_pc_ == bytecode_pc) {
    target_ = target;
    return true;
  }
  // The target should not be set since this jump doesn't and shouldn't jump
  // to the input region
  return false;
}

inline Return* Return::New( Graph* graph , Expr* value , ControlFlow* parent ) {
  return graph->zone()->New<Return>(graph,graph->AssignID(),value,parent);
}

inline Start* Start::New( Graph* graph ) {
  return graph->zone()->New<Start>(graph,graph->AssignID());
}

inline End* End::New( Graph* graph , Success* s , Fail* f ) {
  return graph->zone()->New<End>(graph,graph->AssignID(),s,f);
}

inline Trap* Trap::New( Graph* graph , ControlFlow* region ) {
  return graph->zone()->New<Trap>(graph,graph->AssignID(),region);
}

inline OSRStart* OSRStart::New( Graph* graph ) {
  return graph->zone()->New<OSRStart>(graph,graph->AssignID());
}

inline OSREnd* OSREnd::New( Graph* graph , Success* s , Fail* f ) {
  return graph->zone()->New<OSREnd>(graph,graph->AssignID(),s,f);
}

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_H_