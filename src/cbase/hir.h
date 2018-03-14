#ifndef CBASE_HIR_H_
#define CBASE_HIR_H_
#include "type.h"
#include "src/config.h"
#include "src/util.h"
#include "src/stl-helper.h"
#include "src/zone/zone.h"
#include "src/zone/vector.h"
#include "src/zone/list.h"
#include "src/zone/string.h"
#include "src/cbase/bytecode-analyze.h"
#include "src/interpreter/intrinsic-call.h"

#include <memory>
#include <type_traits>
#include <map>
#include <vector>
#include <deque>
#include <stack>

namespace lavascript {
namespace cbase {
namespace hir {
using namespace ::lavascript;


// High level HIR node. Used to describe unttyped polymorphic
// operations
#define CBASE_IR_EXPRESSION_HIGH(__)            \
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
  /* arithmetic/comparison node */              \
  __(Unary,UNARY ,"unary",false)                \
  __(Binary,BINARY,"binary",false)              \
  __(Ternary,TERNARY,"ternary",false)           \
  /* upvalue */                                 \
  __(UGet,UGET,"uval",true)                     \
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
  /* alias */                                   \
  __(Alias,ALIAS,"alias",false)                 \
  /* checkpoints */                             \
  __(Checkpoint,CHECKPOINT,"checkpoint",false)  \
  __(StackSlot ,STACK_SLOT, "stackslot",false)  \
  __(UGetSlot  ,UGET_SLOT , "uvalslot" ,false)

// Low level HIR node and they are fully typped or partially typped

/**
 * These arithmetic and compare node are used to do typed arithmetic
 * or compare operations. It is used after we do speculative type guess.
 *
 * The node accepts unboxed value for specific type indicated by the node
 * name and output unboxed value. So in general, we should mark the output
 * with a box node. The input should be marked with unbox node or the node
 * itself is a typped node which produce unbox value.
 *
 */
#define CBASE_IR_EXPRESSION_LOW_ARITHMETIC_AND_COMPARE(__)            \
  __(Float64Negate,FLOAT64_NEGATE,"float64_negate",false)             \
  __(Float64Arithmetic,FLOAT64_ARITHMETIC,"float64_arithmetic",false) \
  __(Float64Bitwise,FLOAT64_BITWISE,"float64_bitwise",false)          \
  __(Float64Compare,FLOAT64_COMPARE,"float64_compare",false)          \
  __(StringCompare,STRING_COMPARE,"string_compare",false)             \
  __(SStringEq,SSTRING_EQ,"sstring_eq",false)                         \
  __(SStringNe,SSTRING_NE,"sstring_ne",false)                         \

#define CBASE_IR_EXPRESSION_LOW_PROPERTY(__)                          \
  __(ObjectGet    ,OBJECT_GET    ,"object_get"   ,false)              \
  __(ObjectSet    ,OBJECT_SET    ,"object_set"   ,false)              \
  __(ListGet      ,LIST_GET      ,"list_get"     ,false)              \
  __(ListSet      ,LIST_SET      ,"list_set"     ,false)              \
  __(ExtensionGet ,EXTENSION_GET ,"extension_get",false)              \
  __(ExtensionSet ,EXTENSION_SET ,"extension_set",false)

// All the low HIR nodes
#define CBASE_IR_EXPRESSION_LOW(__)                                   \
  CBASE_IR_EXPRESSION_LOW_ARITHMETIC_AND_COMPARE(__)                  \
  CBASE_IR_EXPRESSION_LOW_PROPERTY(__)

// Guard conditional node , used to do type guess or speculative inline
#define CBASE_IR_EXPRESSION_TEST(__)                                  \
  __(TestType    ,TEST_TYPE      ,"test_type"      , false)           \
  __(TestListOOB ,TEST_LISTOOB   ,"test_listobb"   , false)

/**
 * Guard
 *
 * I don't remember how many times I have refactoried this piece of code.
 *
 * The initial guard design is make guard as a control flow node and then
 * later on it is easy for us to do control flow optimizaiton like DCE on
 * it but this ends up with many guards when the graph is generated because
 * the type information is propoagted along with control flow as well.
 * However we don't really have a clear picture of control flow graph when
 * we do graph generation.
 *
 * Finally I decide to make guard as an expression . As an expression, it
 * means the node can be used in any expression optimization and it is flowed
 * inside of the bytecode register simulation which implicitly propogate along
 * the control flow. So we could end up saving many guard node generation.
 *
 * Another thing is GVN can help to elminiate redundancy. Only one thing is
 * hard to achieve is this :
 *
 * if( type(a) == "string" ) guard(a,"string")
 *
 * This sort of redundancy needs an extra pass to solve because in general,
 * expression node doesn't participate in control flow node optimization.
 *
 *
 * A Unbox operation/node should always follow a guard node since it doesn't
 * expect an type mismatch happened. This means a crash during execution.
 */

#define CBASE_IR_GUARD(__)                       \
  __(TypeGuard,TYPE_GUARD,"type_guard",false)


/**
 * Box operation will wrap a value into the internal box representation
 * basically use Value object as wrapper
 * Unbox is on the contary, basically load the actual value inside of
 * Value object into its plain value.
 *
 * It means for primitive type, the actual primitive value will be loaded
 * out ; for heap type, the GCRef (pointer of pointer) will be loaded out.
 *
 * It is added after the lowering phase of the HIR
 */
#define CBASE_IR_BOXOP(__)                      \
  __(Box,BOX,"box",false)                       \
  __(Unbox,UNBOX,"unbox",false)

// All the expression IR nodes
#define CBASE_IR_EXPRESSION(__)                 \
  CBASE_IR_EXPRESSION_HIGH(__)                  \
  CBASE_IR_EXPRESSION_LOW (__)                  \
  CBASE_IR_EXPRESSION_TEST(__)                  \
  CBASE_IR_BOXOP(__)                            \
  CBASE_IR_GUARD(__)

// All the control flow IR nodes
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

#define SIZE_OF_IR (CBASE_IR_STMT_END-6)

// IR classes forward declaration
#define __(A,...) class A;
CBASE_IR_LIST(__)
#undef __ // __

// IRType value static mapping
template< typename T > struct MapIRClassToIRType {};

#define __(A,B,...) template<> struct MapIRClassToIRType<A> { static const IRType value = IRTYPE_##B; };
CBASE_IR_LIST(__)
#undef __ // __

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

class Graph;
class GraphBuilder;
class ValueRange;
class Node;
class Expr;
class ControlFlow;


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
  Handle<Prototype> prototype;
  PrototypeInfo( std::uint32_t b , const Handle<Prototype>& proto ):
    base(b),
    prototype(proto)
  {}
};


// ----------------------------------------------------------------------------
// Statement list
//
// Bunch of statements that are not used by any expression but have observable
// effects. Example like : foo() , a free function call
typedef zone::List<Expr*> StatementList;

typedef StatementList::ForwardIterator StatementIterator;

// This structure is held by *all* the expression. If the region field is not
// NULL then it means this expression has side effect and it is bounded at
// certain control flow region
struct StatementEdge {
  ControlFlow* region;
  StatementIterator iterator;
  bool IsUsed() const { return region != NULL; }

  StatementEdge( ControlFlow* r , const StatementIterator& itr ): region(r), iterator(itr) {}
  StatementEdge(): region(NULL), iterator() {}
};


/**
 * Each expression will have 2 types of dependency with regards to other expression.
 *
 * 1. OperandList   , describe the operands used by this expression node
 * 2. EffectList    , describe the observable effects of this expression node
 *
 * A operand list is easy to understand, example like :
 *
 *   a = b + c ; for node a , node b and node c are operands and they are inside of the
 *               operands list
 *
 *   a <= b    ; for node a , its value is implicitly dependend on node b's evaluation.
 *               or in other word, b's evaluation has observable effect that a needs to
 *               say.
 *
 *
 * Essentially effect list are only used to describe dependency that is not esay to
 * be expressed inside of IR graph or inefficient to be described here. Example like :
 *
 * a[10] = 10;
 * return a[10] + 1;
 *
 * We can forward 10 to the return statements, but return statments has effect dependency
 * on a[10] = 10 since a[10] = 10 can *fail* due to a is an C++ side extension. But if
 * we don't describe such dependency, then the problem is return a[10] + 1 can be folded
 * into return 11 and it may be able to schedule before a[10] = 10 , obviously this is
 * incorrect execution flow. With effect list we are able to add a[10] = 10 to be effect
 * dependent of return statment, then the scheduler will consider this facts and also guarantee
 * that a[10] + 1 will be scheduled *after* a[10] = 10
 *
 *
 * All these 2 types of dependency are tracked by OperandRefList, so a substituion of node by
 * using *Replace* function will modify all these 2 dependency list
 */

typedef zone::List<Expr*> DependencyList;
typedef DependencyList::ForwardIterator DependencyIterator;

// OperandList
typedef DependencyList OperandList;
typedef OperandList::ForwardIterator OperandIterator;

// EffectList
typedef DependencyList EffectList;
typedef EffectList::ForwardIterator   EffectIterator;

template< typename ITR >
struct Ref {
  ITR     id;  // iterator used for fast deletion of this Ref it is
               // modified
  Node* node;
  Ref( const ITR& iter , Node* n ): id(iter),node(n) {}
  Ref(): id(), node(NULL) {}
};

typedef Ref<OperandIterator>    OperandRef;
typedef zone::List<OperandRef>  OperandRefList;

typedef zone::List<ControlFlow*>      RegionList;
typedef RegionList::ForwardIterator   RegionListIterator;
typedef Ref<RegionListIterator>       RegionRef;
typedef zone::List<RegionRef>         RegionRefList;

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
  template< typename T > bool Is() const { return type() == MapIRClassToIRType<T>::value; }
  template< typename T > inline T* As();
  template< typename T > inline const T* As() const;

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

/* =========================================================================
 *
 * GVN hash function helper implementation
 *
 * Helper function to implement the GVN hash table function
 *
 *
 * GVN general rules:
 *
 * 1) for any primitive type or type that doesn't have observable
 *    side effect, the GVNHash it generates should *not* take into
 *    consideration of the node identity. Example like : any float64
 *    node with same value should have exactly same GVNHash value and
 *    also the Equal function should behave correctly
 *
 * 2) for any type that has side effect , then the GVNHash value should
 *    take into consideration of its node identity. A generaly rules
 *    is put the node's id() value into the GVNHash generation. Prefer
 *    using id() function instead of use this pointer address as seed.
 *
 * ==========================================================================
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
  virtual std::uint64_t GVNHash()        const { return GVNHash1(type_name(),id()); }
  virtual bool Equal( const Expr* that ) const { return this == that;       }

 public:
  bool  IsStatement() const { return stmt_.IsUsed(); }
  void  set_statement_edge ( const StatementEdge& st ) { stmt_= st; }
  const StatementEdge& statement_edge() const { return stmt_; }

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

  // Effect list
  //
  EffectList* effect_list() { return &effect_list_; }
  const EffectList* effect_list() const { return &effect_list_; }

  inline void AddEffect ( Expr* node );

  // Reference list
  //
  // This list returns a list of Ref object which allow user to
  //   1) get a list of expression that uses this expression, ie who uses me
  //   2) get the corresponding iterator where *me* is inserted into the list
  //      so we can fast modify/remove us from its list
  const OperandRefList* ref_list() const { return &ref_list_; }
  OperandRefList* ref_list() { return &ref_list_; }

  // Add the referece into the reference list
  void AddRef( Node* who_uses_me , const OperandIterator& iter ) {
    ref_list()->PushBack(zone(),OperandRef(iter,who_uses_me));
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
    effect_list_     (),
    ref_list_        (),
    ir_info_         (info),
    stmt_            ()
  {}

 private:
  OperandList operand_list_;
  EffectList  effect_list_;
  OperandRefList     ref_list_;
  IRInfo*     ir_info_;
  StatementEdge  stmt_;
};

/* ---------------------------------------------------
 *
 * Node Definition
 *
 * --------------------------------------------------*/

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
  inline static LString* New( Graph* , const char* , IRInfo* );
  inline static LString* New( Graph* , const zone::String* , IRInfo* );
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
  inline static SString* New( Graph* , const char* , IRInfo* );
  inline static SString* New( Graph* , const zone::String* , IRInfo* );
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

// Helper function to create String node from different types of configuration
inline Expr* NewString           ( Graph* , const char* , IRInfo* );
inline Expr* NewString           ( Graph* , const zone::String* , IRInfo* );
inline Expr* NewStringFromBoolean( Graph* , bool , IRInfo* );
inline Expr* NewStringFromReal   ( Graph* , double , IRInfo* );

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


 public:
  // Helper function to clone the IRList
  static IRList* Clone( Graph* , const IRList& );
  static IRList* CloneExceptLastOne( Graph* , const IRList& );

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

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(IRObjectKV)
};

class IRObject : public Expr {
 public:
  inline static IRObject* New( Graph* , std::size_t size , IRInfo* );

  void Add( Expr* key , Expr* val , IRInfo* info ) {
    AddOperand(IRObjectKV::New(graph(),key,val,info));
  }

  std::size_t Size() const { return operand_list()->size(); }

  IRObject( Graph* graph , std::uint32_t id , std::size_t size , IRInfo* info ):
    Expr  (IRTYPE_OBJECT,id,graph,info)
  {
    (void)size;
  }

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

 private:
  std::uint32_t ref_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(LoadCls);
};

// ----------------------------------------------------------------
// Arithmetic
// ----------------------------------------------------------------
class Unary : public Expr {
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

 protected:
  Unary( IRType type , Graph* graph , std::uint32_t id , Expr* opr ,
                                                         Operator op ,
                                                         IRInfo* info ):
    Expr  (type,id,graph,info),
    op_   (op)
  {
    AddOperand(opr);
  }


 private:
  Operator   op_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Unary)
};

class Binary : public Expr {
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
    OR ,

    // bitwise operators , used during the lower phase
    BAND,
    BOR ,
    BXOR,
    BSHL,
    BSHR,
    BROL,
    BROR
  };

  inline static bool        IsComparisonOperator( Operator );
  inline static bool        IsArithmeticOperator( Operator );
  inline static bool        IsBitwiseOperator   ( Operator );
  inline static bool        IsLogicOperator     ( Operator );
  inline static Operator    BytecodeToOperator( interpreter::Bytecode );
  inline static const char* GetOperatorName( Operator );

 public:
  // Create a binary node
  inline static Binary* New( Graph* , Expr* , Expr* , Operator , IRInfo* );

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

 protected:
  Binary( IRType irtype , Graph* graph , std::uint32_t id , Expr* lhs , Expr* rhs ,
                                                                        Operator op,
                                                                        IRInfo* info ):
    Expr  (irtype,id,graph,info),
    op_   (op)
  {
    AddOperand(lhs);
    AddOperand(rhs);
  }

 private:
  Operator op_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Binary)
};

class Ternary: public Expr {
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
  Expr* lhs      () const { return operand_list()->Index(1); }
  Expr* rhs      () const { return operand_list()->Last(); }


 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Ternary)
};

// -------------------------------------------------------------------------
// Specific Arithmetic
//
// Specific arithmetic node means the node has already been properly guarded
// and type hint will be provided inside of the node for code generation
// -------------------------------------------------------------------------


// -------------------------------------------------------------------------
// upvalue get/set
// -------------------------------------------------------------------------
class UGet : public Expr {
 public:
  inline static UGet* New( Graph* , std::uint8_t );

  std::uint8_t index() const { return index_; }

  UGet( Graph* graph , std::uint32_t id , std::uint8_t index ):
    Expr  (IRTYPE_UGET,id,graph,NULL),
    index_(index)
  {}

  virtual std::uint64_t GVNHash() const {
    return GVNHash1(type_name(),index());
  }

  virtual bool Equal( const Expr* that ) const {
    return that->IsUGet() && that->AsUGet()->index() == index();
  }

 private:
  std::uint8_t index_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(UGet)
};

class USet : public Expr {
 public:
  inline static USet* New( Graph* , std::uint32_t , Expr* opr , IRInfo* ,
                                                                ControlFlow* );

  std::uint32_t method() const { return method_; }
  Expr* value() const { return operand_list()->First();  }

  virtual std::uint64_t GVNHash() const {
    return GVNHash2(type_name(),method(),value()->GVNHash());
  }

  virtual bool Equal( const USet* that ) const {
    if(that->IsUSet()) {
      auto that_uset = that->AsUSet();
      return that_uset->method() == method() && that_uset->value()->Equal(value());
    }
    return false;
  }

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
class PGet : public Expr {
 public:
  inline static PGet* New( Graph* , Expr* , Expr* , IRInfo* , ControlFlow* );
  Expr* object() const { return operand_list()->First(); }
  Expr* key   () const { return operand_list()->Last (); }

  virtual std::uint64_t GVNHash() const {
    return GVNHash2(type_name(),object()->GVNHash(),key()->GVNHash());
  }

  virtual bool Equal( const Expr* that ) const {
    if(that->IsPGet()) {
      auto that_pget = that->AsPGet();
      return object()->Equal(that_pget->object()) && key()->Equal(that_pget->key());
    }
    return false;
  }

  PGet( Graph* graph , std::uint32_t id , Expr* object , Expr* index ,
                                                         IRInfo* info ):
    Expr  (IRTYPE_PGET,id,graph,info)
  {
    AddOperand(object);
    AddOperand(index );
  }

 protected:
  PGet( IRType type , Graph* graph , std::uint32_t id , Expr* object ,
                                                        Expr* index  ,
                                                        IRInfo* info ):
    Expr(type,id,graph,info)
  {
    AddOperand(object);
    AddOperand(index );
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(PGet)
};

class PSet : public Expr {
 public:
  inline static PSet* New( Graph* , Expr* , Expr* , Expr* , IRInfo* ,
                                                            ControlFlow* );
  Expr* object() const { return operand_list()->First(); }
  Expr* key   () const { return operand_list()->Index(1);}
  Expr* value () const { return operand_list()->Last (); }

  virtual std::uint64_t GVNHash() const {
    return GVNHash3(type_name(),object()->GVNHash(),
                                key()->GVNHash(),
                                value()->GVNHash());
  }

  virtual bool Equal( const Expr* that ) const {
    if(that->IsPSet()) {
      auto that_pset = that->AsPSet();
      return object()->Equal(that_pset->object()) &&
             key   ()->Equal(that_pset->key())       &&
             value ()->Equal(that_pset->value());
    }
    return false;
  }

  PSet( Graph* graph , std::uint32_t id , Expr* object , Expr* index ,
                                                         Expr* value ,
                                                         IRInfo* info ):
    Expr  (IRTYPE_PSET,id,graph,info)
  {
    AddOperand(object);
    AddOperand(index );
    AddOperand(value );
  }

 protected:
  PSet( IRType type , Graph* graph , std::uint32_t id , Expr* object ,
                                                        Expr* index  ,
                                                        Expr* value  ,
                                                        IRInfo* info ):
    Expr(type,id,graph,info)
  {
    AddOperand(object);
    AddOperand(index );
    AddOperand(value );
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(PSet)
};

class IGet : public Expr {
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
    return GVNHash2(type_name(),object()->GVNHash(),index()->GVNHash());
  }

  virtual bool Equal( const Expr* that ) const {
    if(that->IsIGet()) {
      auto that_iget = that->AsIGet();
      return object()->Equal(that_iget->object()) &&
             index ()->Equal(that_iget->index());
    }
    return false;
  }

 protected:
  IGet( IRType type , Graph* graph , std::uint32_t id , Expr* object ,
                                                        Expr* index ,
                                                        IRInfo* info ):
    Expr(type,id,graph,info)
  {
    AddOperand(object);
    AddOperand(index );
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(IGet)
};

class ISet : public Expr {
 public:
  inline static ISet* New( Graph* , Expr* , Expr* , Expr* , IRInfo* ,
                                                            ControlFlow* );

  Expr* object() const { return operand_list()->First(); }
  Expr* index () const { return operand_list()->Index(1);}
  Expr* value () const { return operand_list()->Last (); }

  virtual std::uint64_t GVNHash() const {
    return GVNHash3(type_name(),object()->GVNHash(),
                                index ()->GVNHash(),
                                value ()->GVNHash());
  }

  virtual bool Eqaul( const Expr* that ) const {
    if(that->IsISet()) {
      auto that_iset = that->AsISet();
      return object()->Equal(that_iset->object()) &&
             index ()->Equal(that_iset->index())  &&
             value ()->Equal(that_iset->value());
    }
    return false;
  }

  ISet( Graph* graph , std::uint32_t id , Expr* object , Expr* index ,
                                                         Expr* value ,
                                                         IRInfo* info ):
    Expr(IRTYPE_ISET,id,graph,info)
  {
    AddOperand(object);
    AddOperand(index );
    AddOperand(value );
  }

 protected:
  ISet( IRType type , Graph* graph , std::uint32_t id , Expr* object ,
                                                        Expr* index  ,
                                                        Expr* value  ,
                                                        IRInfo* info ):
    Expr(type,id,graph,info)
  {
    AddOperand(object);
    AddOperand(index );
    AddOperand(value );
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ISet)
};

// -------------------------------------------------------------------------
// global set/get (side effect)
// -------------------------------------------------------------------------
class GGet : public Expr {
 public:
  inline static GGet* New( Graph* , Expr* , IRInfo* , ControlFlow* );
  Expr* key() const { return operand_list()->First(); }

  GGet( Graph* graph , std::uint32_t id , Expr* name , IRInfo* info ):
    Expr  (IRTYPE_GGET,id,graph,info)
  {
    AddOperand(name);
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(GGet)
};

class GSet : public Expr {
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

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(GSet)
};

// -------------------------------------------------------------------------
// Iterator node (side effect)
// -------------------------------------------------------------------------
class ItrNew : public Expr {
 public:
  inline static ItrNew* New( Graph* , Expr* , IRInfo* , ControlFlow* );
  Expr* operand() const { return operand_list()->First(); }

  ItrNew( Graph* graph , std::uint32_t id , Expr* operand , IRInfo* info ):
    Expr  (IRTYPE_ITR_NEW,id,graph,info)
  {
    AddOperand(operand);
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ItrNew)
};

class ItrNext : public Expr {
 public:
  inline static ItrNext* New( Graph* , Expr* , IRInfo* , ControlFlow* );
  Expr* operand() const { return operand_list()->First(); }

  ItrNext( Graph* graph , std::uint32_t id , Expr* operand , IRInfo* info ):
    Expr  (IRTYPE_ITR_NEXT,id,graph,info)
  {
    AddOperand(operand);
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ItrNext)
};

class ItrTest : public Expr {
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

class ItrDeref : public Expr {
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
  inline Phi( Graph* , std::uint32_t , ControlFlow* , IRInfo* );
 private:
  ControlFlow* region_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Phi)
};

class Call : public Expr {
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

  Expr* GetArgument( std::uint8_t arg ) {
    lava_debug(NORMAL,lava_verify(arg < operand_list()->size()););
    return operand_list()->Index(arg);
  }

  // intrinsic call method index
  interpreter::IntrinsicCall ic() const { return ic_; }

  // whether this call is a tail call
  bool tail_call() const { return tail_call_; }

  // Global value numbering
  virtual std::uint64_t GVNHash() const;
  virtual bool Equal( const Expr* ) const;

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

class InitCls : public Expr {
 public:
  inline static InitCls* New( Graph* , Expr* , IRInfo* );

  InitCls( Graph* graph , std::uint32_t id , Expr* key , IRInfo* info ):
    Expr (IRTYPE_INIT_CLS,id,graph,info)
  {
    AddOperand(key);
  }

  Expr* key() const { return operand_list()->First(); }

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

// Alias
//
// A alias node will represents a certain operations's alias effect towards
// the receiver. It is used to guard certain optimization cross boundary.
// Example like this:
//
// var a = [1,2,3,4];
//
// g = a; // set a to g as a global variable , this gset has a alias effect
//        // and potentially this operation can mutate a's value since it is
//        // in C++ side and the global variable supports introspection
//
// return a[0] + 10; // the optimizer may *incorrectly* fold a[0] + 10 => 11 due
//                   // to we don't know the potential effect
//
// With alias node, the node a will be guarded by this alias which prevents any
// optimization happened
class Alias : public Expr {
 public:
  inline static Alias* New( Graph* , Expr* , Expr* , IRInfo* );

  Expr* alias() const { return operand_list()->First(); }
  Expr* value() const { return operand_list()->Last (); }

  Alias ( Graph* graph , std::uint32_t id , Expr* alias,
                                            Expr* value,
                                            IRInfo* info ):
    Expr(IRTYPE_ALIAS,id,graph,info)
  {
    AddOperand(alias);
    AddOperand(value);
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Alias)
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
// 2) a upvalue state , represented by UGetSlot node
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
  inline void AddUGetSlot ( Expr* , std::uint32_t );

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

// UGetSlot node
//
// Represent a value must be flushed back to the upvalue array of the
// calling closure
class UGetSlot : public Expr {
 public:
  inline static UGetSlot* New( Graph* , Expr* , std::uint32_t );
  std::uint32_t index() const { return index_; }
  Expr* expr() const { return operand_list()->First(); }

  UGetSlot( Graph* graph , std::uint32_t id , Expr* expr , std::uint32_t index ):
    Expr(IRTYPE_UGET_SLOT,id,graph,NULL),
    index_(index)
  {
    AddOperand(expr);
  }
 private:
  std::uint32_t index_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(UGetSlot)
};

/* -------------------------------------------------------
 * Low level operations
 * ------------------------------------------------------*/

class TestType : public Expr {
 public:
  inline static TestType* New( Graph* , TypeKind , Expr* , IRInfo* );

  TypeKind type_kind() const { return type_kind_; }

  const char* type_kind_name() const { return GetTypeKindName(type_kind_); }

  Expr* object() const { return operand_list()->First(); }

  TestType( Graph* graph , std::uint32_t id , TypeKind tc ,
                                              Expr* obj,
                                              IRInfo* info ):
    Expr(IRTYPE_TEST_TYPE,id,graph,info),
    type_kind_(tc)
  {
    AddOperand(obj);
  }

 private:
  TypeKind type_kind_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(TestType)
};

class TestListOOB : public Expr {
 public:
  inline static TestListOOB* New( Graph* , Expr* , Expr* , IRInfo* );

  Expr* object() const { return operand_list()->First(); }
  Expr* index () const { return operand_list()->Last (); }

  TestListOOB( Graph* graph , std::uint32_t id , Expr* obj , Expr* idx ,
                                                             IRInfo* info ):
    Expr(IRTYPE_TEST_LISTOOB,id,graph,info)
  {
    AddOperand(obj);
    AddOperand(idx);
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(TestListOOB)
};

/* -------------------------------------------------------
 * Low level operations
 * ------------------------------------------------------*/

class Float64Negate  : public Expr {
 public:
  inline static Float64Negate* New( Graph* , Expr* , IRInfo* );

  Expr* operand() const { return operand_list()->First(); }

  Float64Negate( Graph* graph , std::uint32_t id , Expr* opr , IRInfo* info ):
    Expr(IRTYPE_FLOAT64_NEGATE,id,graph,info)
  {
    AddOperand(opr);
  }

 public:
  virtual std::uint64_t GVNHash() const {
    return GVNHash1(type_name(),operand()->GVNHash());
  }

  virtual bool Equal( const Expr* that ) const {
    if(that->IsFloat64Negate()) {
      auto that_negate = that->AsFloat64Negate();
      return operand()->Equal(that_negate->operand());
    }
    return false;
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Float64Negate)
};

namespace detail {

template< typename T > struct Float64BinaryGVNImpl {
 protected:
  std::uint64_t GVNHashImpl() const;
  bool EqualImpl( const Expr* that ) const;
};

} // namespace detail

class Float64Arithmetic : public Binary , public detail::Float64BinaryGVNImpl<Float64Arithmetic> {
 public:
  using Binary::Operator;

  inline static Float64Arithmetic* New( Graph* , Expr*, Expr*,
                                                        Operator,
                                                        IRInfo* );

  Float64Arithmetic( Graph* graph , std::uint32_t id , Expr* lhs,
                                                       Expr* rhs,
                                                       Operator op,
                                                       IRInfo* info ):
    Binary(IRTYPE_FLOAT64_ARITHMETIC,graph,id,lhs,rhs,op,info)
  {
    lava_debug(NORMAL,lava_verify(Binary::IsArithmeticOperator(op)););
  }

 public:
  virtual std::uint64_t GVNHash()        const { return GVNHashImpl(); }
  virtual bool Equal( const Expr* that ) const { return EqualImpl(that); }

 private:

  LAVA_DISALLOW_COPY_AND_ASSIGN(Float64Arithmetic)
};

class Float64Bitwise: public Binary , public detail::Float64BinaryGVNImpl<Float64Bitwise> {
 public:
  using Binary::Operator;

  inline static Float64Bitwise* New( Graph* , Expr*, Expr*,
                                                     Operator,
                                                     IRInfo* );

  Float64Bitwise( Graph* graph , std::uint32_t id , Expr* lhs,
                                                    Expr* rhs,
                                                    Operator op,
                                                    IRInfo* info ):
    Binary(IRTYPE_FLOAT64_BITWISE,graph,id,lhs,rhs,op,info)
  {
    lava_debug(NORMAL,lava_verify(Binary::IsBitwiseOperator(op)););
  }

 public:
  virtual std::uint64_t GVNHash()        const { return GVNHashImpl(); }
  virtual bool Equal( const Expr* that ) const { return EqualImpl(that); }

 private:

  LAVA_DISALLOW_COPY_AND_ASSIGN(Float64Bitwise)
};

class Float64Compare : public Binary , public detail::Float64BinaryGVNImpl<Float64Compare> {
 public:
  using Binary::Operator;

  inline static Float64Compare* New( Graph* , Expr* , Expr* ,
                                                      Operator,
                                                      IRInfo* );

  Float64Compare( Graph* graph , std::uint32_t id , Expr* lhs ,
                                                    Expr* rhs ,
                                                    Operator op ,
                                                    IRInfo* info ):
    Binary(IRTYPE_FLOAT64_COMPARE,graph,id,lhs,rhs,op,info)
  {
    lava_debug(NORMAL,lava_verify(Binary::IsComparisonOperator(op)););
  }

 public:
  virtual std::uint64_t GVNHash()        const { return GVNHashImpl(); }
  virtual bool Equal( const Expr* that ) const { return EqualImpl(that); }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Float64Compare)
};

class StringCompare : public Binary , public detail::Float64BinaryGVNImpl<StringCompare> {
 public:
  using Binary::Operator;

  inline static StringCompare* New( Graph* , Expr* , Expr* ,
                                                     Operator ,
                                                     IRInfo* );

  StringCompare( Graph* graph , std::uint32_t id , Expr* lhs ,
                                                   Expr* rhs ,
                                                   Operator op ,
                                                   IRInfo* info ):
    Binary(IRTYPE_STRING_COMPARE,graph,id,lhs,rhs,op,info)
  {
    lava_debug(NORMAL,lava_verify(Binary::IsComparisonOperator(op)););
  }
 public:
  virtual std::uint64_t GVNHash()        const { return GVNHashImpl(); }
  virtual bool Equal( const Expr* that ) const { return EqualImpl(that); }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(StringCompare);
};

class SStringEq : public Binary , public detail::Float64BinaryGVNImpl<SStringEq> {
 public:

  inline static SStringEq* New( Graph* , Expr* , Expr* ,
                                                 IRInfo* );

  SStringEq( Graph* graph , std::uint32_t id , Expr* lhs ,
                                               Expr* rhs ,
                                               IRInfo* info ):
    Binary(IRTYPE_SSTRING_EQ,graph,id,lhs,rhs,Binary::EQ,info)
  {}

 public:
  virtual std::uint64_t GVNHash()        const { return GVNHashImpl(); }
  virtual bool Equal( const Expr* that ) const { return EqualImpl(that); }
};

class SStringNe : public Binary , public detail::Float64BinaryGVNImpl<SStringNe> {
 public:

  inline static SStringNe* New( Graph* , Expr* , Expr* ,
                                                 IRInfo* );

  SStringNe( Graph* graph , std::uint32_t id , Expr* lhs ,
                                               Expr* rhs ,
                                               IRInfo* info ):
    Binary(IRTYPE_SSTRING_EQ,graph,id,lhs,rhs,Binary::NE,info)
  {}

 public:
  virtual std::uint64_t GVNHash()        const { return GVNHashImpl(); }
  virtual bool Equal( const Expr* that ) const { return EqualImpl(that); }
};

class ObjectGet : public PGet {
 public:
  inline static ObjectGet* New( Graph* , Expr* , Expr* , IRInfo* );

  ObjectGet( Graph* graph , std::uint32_t id , Expr* object ,
                                               Expr* key,
                                               IRInfo* info ):
    PGet(IRTYPE_OBJECT_GET,graph,id,object,key,info)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ObjectGet)
};

class ObjectSet : public PSet {
 public:
  inline static ObjectSet* New( Graph* , Expr* , Expr* , Expr* , IRInfo* );

  ObjectSet( Graph* graph , std::uint32_t id , Expr* object ,
                                               Expr* key,
                                               Expr* value,
                                               IRInfo* info ):
    PSet(IRTYPE_OBJECT_SET,graph,id,object,key,value,info)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ObjectSet)
};

class ListGet : public IGet {
 public:
  inline static ListGet* New( Graph* , Expr* , Expr* , IRInfo* );

  ListGet( Graph* graph , std::uint32_t id, Expr* object,
                                            Expr* index ,
                                            IRInfo* info ):
    IGet(IRTYPE_LIST_GET,graph,id,object,index,info)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ListGet)
};

class ListSet : public ISet {
 public:
  inline static ListSet* New( Graph* , Expr* , Expr* , Expr* , IRInfo* );

  ListSet( Graph* graph , std::uint32_t id , Expr* object ,
                                             Expr* index  ,
                                             Expr* value  ,
                                             IRInfo* info ):
    ISet(IRTYPE_LIST_SET,graph,id,object,index,value,info)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ListSet)
};

class ExtensionGet : public IGet {
 public:
  inline static ExtensionGet* New( Graph* , Expr* , Expr* , IRInfo* );

  ExtensionGet( Graph* graph , std::uint32_t id , Expr* extension ,
                                                  Expr* index,
                                                  IRInfo* info ):
    IGet(IRTYPE_EXTENSION_GET,graph,id,extension,index,info)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ExtensionGet)
};

class ExtensionSet : public ISet {
 public:
  inline static ExtensionSet* New( Graph* , Expr* , Expr* , Expr* , IRInfo* );

  ExtensionSet( Graph* graph , std::uint32_t id , Expr* extension ,
                                                  Expr* index,
                                                  Expr* value,
                                                  IRInfo* info ):
    ISet(IRTYPE_EXTENSION_SET,graph,id,extension,index,value,info)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ExtensionSet)
};

// -------------------------------------------------------------------------
//  Box/Unbox
// -------------------------------------------------------------------------
class Box : public Expr {
 public:
  inline static Box* New( Graph* , Expr* , TypeKind , IRInfo* );

  Expr* value() const { return operand_list()->First(); }

  TypeKind type_kind() const { return type_kind_; }

  Box( Graph* graph , std::uint32_t id , Expr* object , TypeKind tk ,
                                                        IRInfo* info ):
    Expr(IRTYPE_BOX,id,graph,info),
    type_kind_(tk)
  {
    AddOperand(object);
  }

 public:
  virtual std::uint64_t GVNHash() const {
    return GVNHash1(type_name(),value()->GVNHash());
  }

  virtual bool Equal( const Expr* that ) const {
    if(that->IsBox()) {
      auto that_box = that->AsBox();
      return value()->Equal(that_box->value());
    }
    return false;
  }

 private:
  TypeKind type_kind_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Box)
};

class Unbox : public Expr {
 public:
  inline static Unbox* New( Graph* , Expr* , TypeKind , IRInfo* );

  Expr* value() const { return operand_list()->First(); }

  TypeKind type_kind() const { return type_kind_; }

  Unbox( Graph* graph , std::uint32_t id , Expr* object , TypeKind tk ,
                                                          IRInfo* info ):
    Expr(IRTYPE_UNBOX,id,graph,info),
    type_kind_(tk)
  {
    AddOperand(object);
  }

 public:
  virtual std::uint64_t GVNHash() const {
    return GVNHash1(type_name(),value()->GVNHash());
  }

  virtual bool Equal( const Expr* that ) const {
    if(that->IsUnbox()) {
      auto that_unbox = that->AsUnbox();
      return value()->Equal(that_unbox->value());
    }
    return false;
  }

 private:
  TypeKind type_kind_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Unbox)
};

// -----------------------------------------------------------------------
//
// Guard
//
// -----------------------------------------------------------------------
class TypeGuard : public Expr {
 public:
  inline static TypeGuard* New( Graph* , Expr* , TypeKind , Checkpoint* , IRInfo* );

  Expr*       node      () const { return operand_list()->First(); }
  Checkpoint* checkpoint() const { return operand_list()->Last()->AsCheckpoint(); }
  TypeKind    type_kind () const { return type_kind_; }

  TypeGuard( Graph* graph , std::uint32_t id , Expr* node , TypeKind type,
                                                            Checkpoint* checkpoint ,
                                                            IRInfo* info ):
    Expr      (IRTYPE_TYPE_GUARD,id,graph,info),
    type_kind_(type)
  {
    AddOperand(node);
    AddOperand(checkpoint);
  }

 public:
  virtual std::uint64_t GVNHash() const {
    return GVNHash2(type_name(),type_kind(),node()->GVNHash());
  }

  virtual bool Equal( const Expr* that ) const {
    if(that->IsTypeGuard()) {
      auto n = that->AsTypeGuard();
      return type_kind() == n->type_kind() && node()->Equal(n->node());
    }
    return false;
  }

 private:
  TypeKind type_kind_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(TypeGuard)
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
  const RegionList* backward_edge() const {
    return &backward_edge_;
  }

  RegionList* backward_edge() {
    return &backward_edge_;
  }

  // special case that only one backward edge we have
  ControlFlow* parent() const {
    lava_debug(NORMAL,lava_verify(backward_edge()->size() == 1););
    return backward_edge()->First();
  }

  void AddBackwardEdge( ControlFlow* edge ) {
    AddBackwardEdgeImpl(edge);
    edge->AddForwardEdgeImpl(this);
  }

  void RemoveBackwardEdge( ControlFlow* );
  void RemoveBackwardEdge( std::size_t index );

  void ClearBackwardEdge () { backward_edge()->Clear(); }

  const RegionList* forward_edge() const {
    return &forward_edge_;
  }

  RegionList* forward_edge() {
    return &forward_edge_;
  }

  void AddForwardEdge ( ControlFlow* edge ) {
    AddForwardEdgeImpl(edge);
    edge->AddBackwardEdgeImpl(this);
  }

  void RemoveForwardEdge( ControlFlow* edge );
  void RemoveForwardEdge( std::size_t index );

  void ClearForwardEdge () { forward_edge()->Clear(); }

  const RegionRefList* ref_list() const {
    return &ref_list_;
  }

  RegionRefList* ref_list() {
    return &ref_list_;
  }

  // Add the referece into the reference list
  void AddRef( ControlFlow* who_uses_me , const RegionListIterator& iter ) {
    ref_list()->PushBack(zone(),RegionRef(iter,who_uses_me));
  }

  // Effective expression doesn't belong to certain expression chain
  //
  // Like free function invocation, they are not part certain expression chain
  // but they have visiable effects.
  //
  // All these types of expressions are stored inside of the effect_expr list
  // to be used later on for code generation
  const StatementList* statement_list() const {
    return &stmt_expr_;
  }

  StatementList* statement_list() {
    return &stmt_expr_;
  }

  void AddStatement( Expr* node ) {
    auto itr = stmt_expr_.PushBack(zone(),node);
    node->set_statement_edge(StatementEdge(this,itr));
  }

  void RemoveStatement( const StatementEdge& ee ) {
    lava_debug(NORMAL,lava_verify(ee.region == this););
    statement_list()->Remove(ee.iterator);
  }

  void MoveStatement( ControlFlow* );

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

 public:

  /**
   * This function will replace |this| node with another control flow
   * node.
   *
   * NOTES:
   *
   * The replace function will only take care of the control flow edge.
   * As with statement list and operand list , they will not be covered
   * in this function
   */
  virtual void Replace( ControlFlow* );

 public:

  ControlFlow( IRType type , std::uint32_t id , Graph* graph , ControlFlow* parent = NULL ):
    Node(type,id,graph),
    backward_edge_   (),
    forward_edge_    (),
    ref_list_        (),
    stmt_expr_       (),
    operand_list_    ()
  {
    if(parent) AddBackwardEdge(parent);
  }

 private:
  void AddBackwardEdgeImpl ( ControlFlow* cf ) {
    auto itr = backward_edge_.PushBack(zone(),cf);
    cf->AddRef(this,itr);
  }

  void AddForwardEdgeImpl( ControlFlow* cf ) {
    auto itr = forward_edge_.PushBack(zone(),cf);
    cf->AddRef(this,itr);
  }

 private:
  RegionList                 backward_edge_;
  RegionList                 forward_edge_;
  RegionRefList              ref_list_;
  StatementList              stmt_expr_;
  OperandList                operand_list_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(ControlFlow)
};

class Region : public ControlFlow {
 public:
  inline static Region* New( Graph* );
  inline static Region* New( Graph* , ControlFlow* );

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

  ControlFlow* merge() const { return merge_; }
  void set_merge( ControlFlow* merge ) { merge_ = merge; }

  LoopHeader( Graph* graph , std::uint32_t id , ControlFlow* region ):
    ControlFlow(IRTYPE_LOOP_HEADER,id,graph,region)
  {}

 private:
  ControlFlow* merge_;
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

  ControlFlow* merge() const { return merge_; }
  void set_merge( ControlFlow* merge ) { merge_ = merge; }

  If( Graph* graph , std::uint32_t id , Expr* cond , ControlFlow* region ):
    ControlFlow(IRTYPE_IF,id,graph,region),
    merge_(NULL)
  {
    AddOperand(cond);
  }

 private:
  ControlFlow* merge_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(If)
};

class IfTrue : public ControlFlow {
 public:
  static const std::size_t kIndex = 1;

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
  static const std::size_t kIndex = 0;

  inline static IfFalse* New( Graph* , ControlFlow* );
  inline static IfFalse* New( Graph* );

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

  Success( Graph* graph , std::uint32_t id ):
    ControlFlow  (IRTYPE_SUCCESS,id,graph)
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
 public:
  inline static Trap* New( Graph* , Checkpoint* , ControlFlow* );

  Checkpoint* checkpoint() const { return operand_list()->First()->AsCheckpoint(); }

  Trap( Graph* graph , std::uint32_t id , Checkpoint* cp ,
                                          ControlFlow* region ):
    ControlFlow(IRTYPE_TRAP,id,graph,region)
  {
    AddOperand(cp);
  }

 private:
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
  Graph();
  ~Graph();
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

  // Get all control flow nodes
  void GetControlFlowNode( std::vector<ControlFlow*>* ) const;

 public:
  std::uint32_t AddPrototypeInfo( const Handle<Prototype>& proto ,
      std::uint32_t base ) {
    prototype_info_.Add(zone(),PrototypeInfo(base,proto));
    return static_cast<std::uint32_t>(prototype_info_.size()-1);
  }

  const PrototypeInfo& GetProrotypeInfo( std::uint32_t index ) const {
    return prototype_info_[index];
  }

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
  bool Has( const Node* n ) const { return existed_[n->id()]; }
  bool empty() const { return array_.empty(); }
  std::size_t size() const { return array_.size(); }
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
  bool Has( const Node* n ) const { return existed_[n->id()]; }
  bool empty() const { return array_.empty(); }
  std::size_t size() const { return array_.size(); }
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

// -------------------------------------------------------------------------------------
// A graph node visitor.
//
// Just a forward BFS iterator. This is a cheap way to iterate each node and its main
// goal is to tell which control flow nodes are inside of the graph
class ControlFlowBFSIterator: public ControlFlowIterator {
 public:
  ControlFlowBFSIterator( const Graph& graph ):
    stack_(graph),
    graph_(&graph),
    next_ (NULL)
  {
    stack_.Push(graph.start());
    Move();
  }

  bool HasNext() const { return next_ != NULL; }
  bool Move();
  ControlFlow* value() const { lava_debug(NORMAL,lava_verify(HasNext());); return next_; }

 private:
  OnceList     stack_;
  const Graph* graph_;
  ControlFlow* next_;
};

// -------------------------------------------------------------------------------------
// A graph node post order iterator. It will only visit a node once all its children
// are visited. Basically visit as many children as possible
//
// This algorithm visit the graph in forward direction basically end up with a backwards
// edge output
class ControlFlowPOIterator : public ControlFlowIterator {
 public:
  ControlFlowPOIterator( const Graph& graph ):
    stack_(graph),
    graph_(&graph),
    next_ (NULL)
  {
    stack_.Push(graph.start());
    Move();
  }

  bool HasNext() const { return next_ != NULL; }
  bool Move();
  ControlFlow* value() const { lava_debug(NORMAL,lava_verify(HasNext());); return next_; }

 private:
  OnceList     stack_;
  const Graph* graph_;
  ControlFlow* next_;
};

// -------------------------------------------------------------------------------
// A graph node RPO iterator
//
// This iterator will visit each node in order that all its predecessor has been visited
// then this node will be visited. The loop's back edge is ignored
class ControlFlowRPOIterator : public ControlFlowIterator {
 public:
  ControlFlowRPOIterator( const Graph& graph ):
    mark_ (graph.MaxID()),
    stack_(graph),
    graph_(&graph),
    next_ (NULL)
  {
    stack_.Push(graph.end());
    Move();
  }

  bool HasNext() const { return next_ != NULL; }

  bool Move();

  ControlFlow* value() const { lava_debug(NORMAL,lava_verify(HasNext());); return next_; }
 private:
  DynamicBitSet mark_;
  OnceList     stack_;
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
//
namespace detail {

template< typename T >
std::uint64_t Float64BinaryGVNImpl<T>::GVNHashImpl() const {
  auto self = static_cast<const T*>(this);
  return GVNHash3(self->type_name(),self->op(),
                                    self->lhs()->GVNHash(),
                                    self->rhs()->GVNHash());
}

template< typename T >
bool Float64BinaryGVNImpl<T>::EqualImpl( const Expr* that ) const {
  auto self = static_cast<const T*>(this);
  if(that->Is<T>()) {
    auto n = that->As<T>();
    return self->op() == n->op() && self->lhs()->Equal(n->lhs()) &&
                                    self->rhs()->Equal(n->rhs());
  }
  return false;
}


} // namespace detail

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

template< typename T >
inline T* Node::As() { lava_debug(NORMAL,lava_verify(Is<T>());); return static_cast<T*>(this); }

template< typename T >
inline const T* Node::As() const {
  lava_debug(NORMAL,lava_verify(Is<T>()););
  return static_cast<const T*>(this);
}

inline const zone::String& Node::AsZoneString() const {
  lava_debug(NORMAL,lava_verify(IsString()););
  return IsLString() ? *AsLString()->value() :
                       *AsSString()->value() ;
}

inline void Expr::AddOperand( Expr* node ) {
  auto itr = operand_list()->PushBack(zone(),node);
  node->AddRef(this,itr);
}

inline void Expr::AddEffect ( Expr* node ) {
  auto itr = effect_list()->PushBack(zone(),node);
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

inline LString* LString::New( Graph* graph , const char* data , IRInfo* info ) {
  auto str = zone::String::New(graph->zone(),data);
  lava_debug(NORMAL,lava_verify(!str->IsSSO()););
  return graph->zone()->New<LString>(graph,graph->AssignID(),str,info);
}

inline LString* LString::New( Graph* graph , const zone::String* str , IRInfo* info ) {
  lava_debug(NORMAL,lava_verify(!str->IsSSO()););
  return graph->zone()->New<LString>(graph,graph->AssignID(),str,info);
}

inline SString* SString::New( Graph* graph , const SSO& str , IRInfo* info ) {
  return graph->zone()->New<SString>(graph,graph->AssignID(),
      zone::String::New(graph->zone(),static_cast<const char*>(str.data()),str.size()),info);
}

inline SString* SString::New( Graph* graph , const char* data , IRInfo* info ) {
  auto str = zone::String::New(graph->zone(),data);
  lava_debug(NORMAL,lava_verify(str->IsSSO()););
  return graph->zone()->New<SString>(graph,graph->AssignID(),str,info);
}

inline SString* SString::New( Graph* graph , const zone::String* str , IRInfo* info ) {
  lava_debug(NORMAL,lava_verify(str->IsSSO()););
  return graph->zone()->New<SString>(graph,graph->AssignID(),str,info);
}

inline Expr* NewString( Graph* graph , const zone::String* str , IRInfo* info ) {
  return str->IsSSO() ? static_cast<Expr*>(SString::New(graph,str,info)) :
                        static_cast<Expr*>(LString::New(graph,str,info)) ;
}

inline Expr* NewString( Graph* graph , const char* data , IRInfo* info ) {
  auto str = zone::String::New(graph->zone(),data);
  return NewString(graph,str,info);
}

inline Expr* NewStringFromBoolean( Graph* graph , bool value , IRInfo* info ) {
  std::string temp;
  LexicalCast(value,&temp);
  auto str = zone::String::New(graph->zone(),temp.c_str(),temp.size());
  return NewString(graph,str,info);
}

inline Expr* NewStringFromReal( Graph* graph , double value , IRInfo* info ) {
  std::string temp;
  LexicalCast(value,&temp);
  auto str = zone::String::New(graph->zone(),temp.c_str(),temp.size());
  return NewString(graph,str,info);
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

inline bool Binary::IsComparisonOperator( Operator op ) {
  switch(op) {
    case LT : case LE : case GT : case GE : case EQ : case NE :
      return true;
    default:
      return false;
  }
}

inline bool Binary::IsArithmeticOperator( Operator op ) {
  switch(op) {
    case ADD : case SUB : case MUL : case DIV : case MOD : case POW :
      return true;
    default:
      return false;
  }
}

inline bool Binary::IsBitwiseOperator( Operator op ) {
  switch(op) {
    case BAND: case BOR: case BXOR: case BSHL: case BSHR: case BROL: case BROR:
      return true;
    default:
      return false;
  }
}

inline bool Binary::IsLogicOperator( Operator op ) {
  return op == AND || op == OR;
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
    case BAND:    return "band";
    case BOR :    return "bor";
    case BXOR:    return "bxor";
    case BSHL:    return "bshl";
    case BSHR:    return "bshr";
    case BROL:    return "brol";
    case BROR:    return "bror";
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

inline UGet* UGet::New( Graph* graph , std::uint8_t index ) {
  return graph->zone()->New<UGet>(graph,graph->AssignID(),index);
}

inline USet* USet::New( Graph* graph , std::uint32_t method , Expr* opr , IRInfo* info ,
                                                                          ControlFlow* region ) {
  auto ret = graph->zone()->New<USet>(graph,graph->AssignID(),method,opr,info);
  return ret;
}

inline PGet* PGet::New( Graph* graph , Expr* obj , Expr* key , IRInfo* info ,
                                                               ControlFlow* region ) {
  auto ret = graph->zone()->New<PGet>(graph,graph->AssignID(),obj,key,info);
  return ret;
}

inline PSet* PSet::New( Graph* graph , Expr* obj , Expr* key , Expr* value ,
                                                               IRInfo* info,
                                                               ControlFlow* region ) {
  auto ret = graph->zone()->New<PSet>(graph,graph->AssignID(),obj,key,value,info);
  return ret;
}

inline IGet* IGet::New( Graph* graph , Expr* obj, Expr* key , IRInfo* info ,
                                                              ControlFlow* region ) {
  auto ret = graph->zone()->New<IGet>(graph,graph->AssignID(),obj,key,info);
  return ret;
}

inline ISet* ISet::New( Graph* graph , Expr* obj , Expr* key , Expr* val ,
                                                               IRInfo* info ,
                                                               ControlFlow* region ) {
  auto ret = graph->zone()->New<ISet>(graph,graph->AssignID(),obj,key,val,info);
  return ret;
}

inline GGet* GGet::New( Graph* graph , Expr* key , IRInfo* info , ControlFlow* region ) {
  auto ret = graph->zone()->New<GGet>(graph,graph->AssignID(),key,info);
  return ret;
}

inline GSet* GSet::New( Graph* graph , Expr* key, Expr* value , IRInfo* info ,
                                                                ControlFlow* region ) {
  auto ret = graph->zone()->New<GSet>(graph,graph->AssignID(),key,value,info);
  return ret;
}

inline ItrNew* ItrNew::New( Graph* graph , Expr* operand , IRInfo* info ,
                                                           ControlFlow* region ) {
  auto ret = graph->zone()->New<ItrNew>(graph,graph->AssignID(),operand,info);
  return ret;
}

inline ItrNext* ItrNext::New( Graph* graph , Expr* operand , IRInfo* info ,
                                                             ControlFlow* region ) {
  auto ret = graph->zone()->New<ItrNext>(graph,graph->AssignID(),operand,info);
  return ret;
}

inline ItrTest* ItrTest::New( Graph* graph , Expr* operand , IRInfo* info ,
                                                             ControlFlow* region ) {
  auto ret = graph->zone()->New<ItrTest>(graph,graph->AssignID(),operand,info);
  return ret;
}

inline ItrDeref* ItrDeref::New( Graph* graph , Expr* operand , IRInfo* info ,
                                                               ControlFlow* region ) {
  auto ret = graph->zone()->New<ItrDeref>(graph,graph->AssignID(),operand,info);
  return ret;
}

inline Phi::Phi( Graph* graph , std::uint32_t id , ControlFlow* region , IRInfo* info ):
  Expr           (IRTYPE_PHI,id,graph,info),
  region_        (region)
{
  region->AddOperand(this);
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

inline Alias* Alias::New( Graph* graph , Expr* receiver , Expr* applier ,
                                                          IRInfo* info ) {
  return graph->zone()->New<Alias>(graph,graph->AssignID(),receiver,applier,info);
}

inline Checkpoint* Checkpoint::New( Graph* graph ) {
  return graph->zone()->New<Checkpoint>(graph,graph->AssignID());
}

inline void Checkpoint::AddStackSlot( Expr* val , std::uint32_t index ) {
  AddOperand(StackSlot::New(graph(),val,index));
}

inline void Checkpoint::AddUGetSlot ( Expr* val , std::uint32_t index ) {
  AddOperand(UGetSlot::New(graph(),val,index));
}

inline TestType* TestType::New( Graph* graph , TypeKind tc , Expr* object ,
                                                                 IRInfo* info ) {
  return graph->zone()->New<TestType>(graph,graph->AssignID(),tc,object,info);
}

inline TestListOOB* TestListOOB::New( Graph* graph , Expr* object , Expr* key ,
                                                                      IRInfo* info ) {
  return graph->zone()->New<TestListOOB>(graph,graph->AssignID(),object,key,info);
}

inline Float64Negate* Float64Negate::New( Graph* graph , Expr* opr , IRInfo* info ) {
  return graph->zone()->New<Float64Negate>(graph,graph->AssignID(),opr,info);
}

inline Float64Arithmetic* Float64Arithmetic::New( Graph* graph , Expr* lhs ,
                                                                 Expr* rhs ,
                                                                 Operator op,
                                                                 IRInfo* info ) {
  return graph->zone()->New<Float64Arithmetic>(graph,graph->AssignID(),lhs,rhs,op,info);
}

inline Float64Bitwise* Float64Bitwise::New( Graph* graph , Expr* lhs ,
                                                           Expr* rhs ,
                                                           Operator op,
                                                           IRInfo* info ) {
  return graph->zone()->New<Float64Bitwise>(graph,graph->AssignID(),lhs,rhs,op,info);
}

inline Float64Compare* Float64Compare::New( Graph* graph , Expr* lhs ,
                                                           Expr* rhs ,
                                                           Operator op,
                                                           IRInfo* info ) {
  return graph->zone()->New<Float64Compare>(graph,graph->AssignID(),lhs,rhs,op,info);
}

inline StringCompare* StringCompare::New( Graph* graph , Expr* lhs , Expr* rhs ,
                                                                     Operator op ,
                                                                     IRInfo* info ) {
  return graph->zone()->New<StringCompare>(graph,graph->AssignID(),lhs,rhs,op,info);
}

inline SStringEq* SStringEq::New( Graph* graph , Expr* lhs , Expr* rhs , IRInfo* info ) {
  return graph->zone()->New<SStringEq>(graph,graph->AssignID(),lhs,rhs,info);
}

inline SStringNe* SStringNe::New( Graph* graph , Expr* lhs , Expr* rhs , IRInfo* info ) {
  return graph->zone()->New<SStringNe>(graph,graph->AssignID(),lhs,rhs,info);
}

inline ListGet* ListGet::New( Graph* graph , Expr* obj , Expr* index , IRInfo* info ) {
  return graph->zone()->New<ListGet>(graph,graph->AssignID(),obj,index,info);
}

inline ListSet* ListSet::New( Graph* graph , Expr* obj , Expr* index , Expr* value ,
                                                                       IRInfo* info ) {
  return graph->zone()->New<ListSet>(graph,graph->AssignID(),obj,index,value,info);
}

inline ObjectGet* ObjectGet::New( Graph* graph , Expr* obj , Expr* key , IRInfo* info ) {
  return graph->zone()->New<ObjectGet>(graph,graph->AssignID(),obj,key,info);
}

inline ObjectSet* ObjectSet::New( Graph* graph , Expr* obj , Expr* key , Expr* value ,
                                                                         IRInfo* info ) {
  return graph->zone()->New<ObjectSet>(graph,graph->AssignID(),obj,key,value,info);
}

inline ExtensionGet* ExtensionGet::New( Graph* graph , Expr* obj , Expr* key , IRInfo* info ) {
  return graph->zone()->New<ExtensionGet>(graph,graph->AssignID(),obj,key,info);
}

inline ExtensionSet* ExtensionSet::New( Graph* graph , Expr* obj , Expr* key , Expr* value ,
                                                                               IRInfo* info ) {
  return graph->zone()->New<ExtensionSet>(graph,graph->AssignID(),obj,key,value,info);
}

inline Box* Box::New( Graph* graph , Expr* obj , TypeKind tk , IRInfo* info ) {
  return graph->zone()->New<Box>(graph,graph->AssignID(),obj,tk,info);
}

inline Unbox* Unbox::New( Graph* graph , Expr* obj , TypeKind tk , IRInfo* info ) {
  return graph->zone()->New<Unbox>(graph,graph->AssignID(),obj,tk,info);
}

inline StackSlot* StackSlot::New( Graph* graph , Expr* expr , std::uint32_t index ) {
  return graph->zone()->New<StackSlot>(graph,graph->AssignID(),expr,index);
}

inline UGetSlot* UGetSlot::New( Graph* graph , Expr* expr , std::uint32_t index ) {
  return graph->zone()->New<UGetSlot>(graph,graph->AssignID(),expr,index);
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

inline TypeGuard* TypeGuard::New( Graph* graph , Expr* node , TypeKind type ,
                                                              Checkpoint* cp,
                                                              IRInfo*  info ) {
  return graph->zone()->New<TypeGuard>(graph,graph->AssignID(),node,type,cp,info);
}

inline IfTrue* IfTrue::New( Graph* graph , ControlFlow* parent ) {
  lava_debug(NORMAL,lava_verify(
        parent->IsIf() && parent->forward_edge()->size() == 1););

  return graph->zone()->New<IfTrue>(graph,graph->AssignID(),parent);
}

inline IfTrue* IfTrue::New( Graph* graph ) {
  return IfTrue::New(graph,NULL);
}

inline IfFalse* IfFalse::New( Graph* graph , ControlFlow* parent ) {
  lava_debug(NORMAL,lava_verify(
        parent->IsIf() && parent->forward_edge()->size() == 0););

  return graph->zone()->New<IfFalse>(graph,graph->AssignID(),parent);
}

inline IfFalse* IfFalse::New( Graph* graph ) {
  return IfFalse::New(graph,NULL);
}

inline Jump* Jump::New( Graph* graph , const std::uint32_t* pc , ControlFlow* parent ) {
  return graph->zone()->New<Jump>(graph,graph->AssignID(),parent,pc);
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

inline Trap* Trap::New( Graph* graph , Checkpoint* cp , ControlFlow* region ) {
  return graph->zone()->New<Trap>(graph,graph->AssignID(),cp,region);
}

inline OSRStart* OSRStart::New( Graph* graph ) {
  return graph->zone()->New<OSRStart>(graph,graph->AssignID());
}

inline OSREnd* OSREnd::New( Graph* graph , Success* s , Fail* f ) {
  return graph->zone()->New<OSREnd>(graph,graph->AssignID(),s,f);
}

// ---------------------------------------------------------------------
// Helper functions for creation of node
// ---------------------------------------------------------------------
template< typename T , typename ...ARGS >
inline Box* NewBoxNode( Graph* graph , TypeKind tk , IRInfo* irinfo , ARGS ...args ) {
  auto n = T::New(graph,args...);
  return Box::New(graph,n,tk,irinfo);
}

// Create a unbox value from a node that has type inference.
Expr* NewUnboxNode( Graph* , Expr* node , TypeKind tk , IRInfo* );


} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_H_
