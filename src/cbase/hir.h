#ifndef CBASE_HIR_H_
#define CBASE_HIR_H_
#include "src/config.h"
#include "src/tagged-ptr.h"
#include "src/util.h"
#include "src/stl-helper.h"
#include "src/zone/zone.h"
#include "src/zone/vector.h"
#include "src/zone/list.h"
#include "src/zone/string.h"
#include "src/interpreter/intrinsic-call.h"

#include "bytecode-analyze.h"
#include "type.h"
#include "type-inference.h"

#include <memory>
#include <type_traits>
#include <map>
#include <vector>
#include <deque>
#include <stack>
#include <functional>

namespace lavascript {
namespace cbase {
namespace hir {

// High level HIR node. Used to describe unttyped polymorphic
// operations
#define CBASE_IR_EXPRESSION_HIGH(__)                                   \
  /* const    */                                                       \
  __(Float64,FLOAT64,"float64",true)                                   \
  __(LString,LONG_STRING,"lstring",true)                               \
  __(SString,SMALL_STRING,"small_string",true)                         \
  __(Boolean,BOOLEAN,"boolean",true)                                   \
  __(Nil,NIL,"null",true)                                              \
  /* compound */                                                       \
  __(IRList,LIST,   "list",false)                                      \
  __(IRObjectKV,OBJECT_KV,"object_kv",false)                           \
  __(IRObject,OBJECT, "object",false)                                  \
  /* closure */                                                        \
  __(LoadCls,LOAD_CLS,"load_cls",true)                                 \
  /* argument node */                                                  \
  __(Arg,ARG,"arg",true)                                               \
  /* arithmetic/comparison node */                                     \
  __(Unary,UNARY ,"unary",false)                                       \
  __(Binary,BINARY,"binary",false)                                     \
  __(Ternary,TERNARY,"ternary",false)                                  \
  /* upvalue */                                                        \
  __(UGet,UGET,"uval",true)                                            \
  __(USet,USET  ,"uset",true)                                          \
  /* property/idx */                                                   \
  __(PGet,PGET  ,"pget",false)                                         \
  __(PSet,PSET  ,"pset",false)                                         \
  __(IGet,IGET  ,"iget",false)                                         \
  __(ISet,ISET  ,"iset",false)                                         \
  /* gget */                                                           \
  __(GGet,GGET  , "gget",false)                                        \
  __(GSet,GSET  , "gset",false)                                        \
  /* iterator */                                                       \
  __(ItrNew ,ITR_NEW ,"itr_new",false)                                 \
  __(ItrNext,ITR_NEXT,"itr_next",false)                                \
  __(ItrTest,ITR_TEST,"itr_test",false)                                \
  __(ItrDeref,ITR_DEREF,"itr_deref",false)                             \
  /* call     */                                                       \
  __(Call,CALL   ,"call",false)                                        \
  /* intrinsic call */                                                 \
  __(ICall,ICALL ,"icall",false)                                       \
  /* phi */                                                            \
  __(Phi,PHI,"phi",false)                                              \
  __(WriteEffectPhi,WRITE_EFFECT_PHI,"write_effect_phi",false)         \
  __(ReadEffectPhi, READ_EFFECT_PHI ,"read_effect_phi" ,false)         \
  __(NoReadEffect,NO_READ_EFFECT,"no_read_effect",true)                \
  __(NoWriteEffect,NO_WRITE_EFFECT,"no_write_effect",true)             \
  /* statement */                                                      \
  __(InitCls,INIT_CLS,"init_cls",false)                                \
  __(Projection,PROJECTION,"projection",false)                         \
  /* osr */                                                            \
  __(OSRLoad,OSR_LOAD,"osr_load",true)                                 \
  /* checkpoints */                                                    \
  __(Checkpoint,CHECKPOINT,"checkpoint",false)                         \
  __(StackSlot ,STACK_SLOT, "stackslot",false)

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
  __(BooleanNot,BOOLEAN_NOT,"boolean_not",false)                      \
  __(BooleanLogic ,BOOLEAN_LOGIC ,"boolean_logic" ,false)             \
  __(StringCompare,STRING_COMPARE,"string_compare",false)             \
  __(SStringEq,SSTRING_EQ,"sstring_eq",false)                         \
  __(SStringNe,SSTRING_NE,"sstring_ne",false)                         \

#define CBASE_IR_EXPRESSION_LOW_PROPERTY(__)                          \
  __(ObjectGet    ,OBJECT_GET    ,"object_get"   ,false)              \
  __(ObjectSet    ,OBJECT_SET    ,"object_set"   ,false)              \
  __(ListGet      ,LIST_GET      ,"list_get"     ,false)              \
  __(ListSet      ,LIST_SET      ,"list_set"     ,false)

// All the low HIR nodes
#define CBASE_IR_EXPRESSION_LOW(__)                                   \
  CBASE_IR_EXPRESSION_LOW_ARITHMETIC_AND_COMPARE(__)                  \
  CBASE_IR_EXPRESSION_LOW_PROPERTY(__)

// Guard conditional node , used to do type guess or speculative inline
//
// A null test is same as TestType(object,NULL) since null doesn't have
// any value. And during graph construction any null test , ie x == null,
// will be *normalized* into a TestType then we could just use predicate
// to do inference and have null redundancy removal automatically
#define CBASE_IR_EXPRESSION_TEST(__)                                  \
  __(TestType    ,TEST_TYPE      ,"test_type"      , false)           \
  __(TestListOOB ,TEST_LISTOOB   ,"test_listobb"   , false)

/**
 * Annotation
 *
 * An anntiona is an *expression* node that attaches to an expression
 * annotate the expression's certain aspects. Usually this is for type
 * annotation node to decorate the expression's type
 *
 */

#define CBASE_IR_ANNOTATION(__)                                       \
  __(TypeAnnotation,TYPE_ANNOTATION,"type_annotation",false)

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
#define CBASE_IR_BOXOP(__)                                            \
  __(Box,BOX,"box",false)                                             \
  __(Unbox,UNBOX,"unbox",false)

/**
 * Cast
 * Cast a certain type's value into another type's value. These type cast
 * doesn't perform any test and should be guaranteed to be correct by
 * the compiler
 */
#define CBASE_IR_CAST(__)                                             \
  __(CastToBoolean,CAST_TO_BOOLEAN,"cast_to_boolean",false)

// All the expression IR nodes
#define CBASE_IR_EXPRESSION(__)                                       \
  CBASE_IR_EXPRESSION_HIGH(__)                                        \
  CBASE_IR_EXPRESSION_LOW (__)                                        \
  CBASE_IR_EXPRESSION_TEST(__)                                        \
  CBASE_IR_BOXOP(__)                                                  \
  CBASE_IR_CAST (__)                                                  \
  CBASE_IR_ANNOTATION(__)

// All the control flow IR nodes
#define CBASE_IR_CONTROL_FLOW(__)                                     \
  __(Start,START,"start",false)                                       \
  __(End,END  , "end" ,false)                                         \
  __(LoopHeader,LOOP_HEADER,"loop_header",false)                      \
  __(Loop,LOOP ,"loop",false)                                         \
  __(LoopExit,LOOP_EXIT,"loop_exit",false)                            \
  __(Guard,GUARD,"guard",false)                                       \
  __(If,IF,"if",false)                                                \
  __(IfTrue,IF_TRUE,"if_true",false)                                  \
  __(IfFalse,IF_FALSE,"if_false",false)                               \
  __(Jump,JUMP,"jump",false)                                          \
  __(Fail ,FAIL,"fail" ,true)                                         \
  __(Success,SUCCESS,"success",false)                                 \
  __(Return,RETURN,"return",false)                                    \
  __(Region,REGION,"region",false)                                    \
  __(Trap,TRAP, "trap",false)                                         \
  /* osr */                                                           \
  __(OSRStart,OSR_START,"osr_start",false)                            \
  __(OSREnd  ,OSR_END  ,"osr_end"  ,false)

#define CBASE_IR_LIST(__)                                             \
  CBASE_IR_EXPRESSION(__)                                             \
  CBASE_IR_CONTROL_FLOW(__)

enum IRType {
#define __(A,B,...) HIR_##B,
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

#define __(A,B,...)         \
  template<> struct MapIRClassToIRType<A> { static const IRType value = HIR_##B; };
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
class MemoryOp;
class MemoryWrite;
class MemoryRead ;
class MemoryNode;
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

typedef std::function<IRInfo*()> IRInfoProvider;

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
  bool HasRef() const { return region != NULL; }

  StatementEdge( ControlFlow* r , const StatementIterator& itr ): region(r), iterator(itr) {}
  StatementEdge(): region(NULL), iterator() {}
};

/**
 * Each expression will have 2 types of dependency with regards to other expression.
 *
 * 1) a data dependency, expressed via the code , things like c = a + b basically means
 *    c is data dependent on a and b. The ir node's operand is designed to express this
 *    sort of relationship
 * 2) effect dependenty, EffectList in expr node are used to represent this. In general
 *    effect represents alias information. Weak type language cannot do too much alias.
 *    example like:
 *      g.f = 1; // g is a global variable
 *      e.b = 2; // e is a global variable
 *
 *    these 2 statements has a effect dependency , the latter one depend on first one since
 *    g and e alias the same underly memory based on conservative guess. To express this
 *    types of information we just need to add g.f = 1's ir node into e.b's effect list then
 *    scheduler will take care of it.
 *
 *    another situation is the control flow , example like :
 *
 *    if(cond) { g.c = 200; } else { g.d = 200; g.e = 300; }
 *    return g.ret;
 *
 *    this example basically says that the expression g.ret should be executed *after* that
 *    control flow branch, however we cannot express this by simply add certain expression
 *    into our effect list. here to merge the side effect brought by this branch, we need
 *    a node called EffectPhi node , this node is used to represent that there're multiple
 *    expression executed which could bring us potential side effect ; and this side effect
 *    should be observed at statement of g.ret. We use EffectPhi node to merge the side
 *    effect in each branch, basically EffectPhi(g.e=300;g.c=200) , and then g.ret depend on
 *    this EffectPhi node.
 */

// OperandList
typedef zone::List<Expr*>               OperandList;
typedef OperandList::ForwardIterator    OperandIterator;

// EffectList
typedef zone::List<Expr*>               EffectList;
typedef EffectList::ForwardIterator     EffectIterator;

template< typename ITR >
struct Ref {
  ITR     id;  // iterator used for fast deletion of this Ref it is modified
  Node* node;
  Ref( const ITR& iter , Node* n ): id(iter),node(n) {}
  Ref(): id(), node(NULL) {}
};

typedef Ref<OperandIterator>            OperandRef;
typedef zone::List<OperandRef>          OperandRefList;
typedef zone::List<ControlFlow*>        RegionList;
typedef RegionList::ForwardIterator     RegionListIterator;
typedef Ref<RegionListIterator>         RegionRef;
typedef zone::List<RegionRef>           RegionRefList;

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
  // check whether 2 nodes are same , pls do not use pointer comparison
  // due to the sick cpp memory layout makes pointer not the same value
  // even they are actually same object. If you want to do comparison ,
  // do something like static_cast<Node*>(a) == static_cast<Node*>(b) which
  // is the same as using IsSame function.
  bool IsSame( Node* that ) const { return id() == that->id(); }
 public: // type check and cast
  template< typename T > bool Is() const { return type() == MapIRClassToIRType<T>::value; }
  template< typename T > inline T* As();
  template< typename T > inline const T* As() const;

#define __(A,B,...) bool Is##A() const { return type() == HIR_##B; }
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
  Node( IRType type , std::uint32_t id , Graph* graph ):type_(type),id_(id),graph_(graph) {}

 private:
  IRType        type_;
  std::uint32_t id_;
  Graph*        graph_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(Node)
};

/**
 * GVN hash function helper implementation
 *
 * Helper function to implement the GVN hash table function
 *
 * GVN general rules:
 *
 * 1) for any primitive type or type that doesn't have observable side effect, the GVNHash
 *    it generates should *not* take into consideration of the node identity. Example like:
 *    any float64 node with same value should have exactly same GVNHash value and also the
 *    Equal function should behave correctly
 *
 * 2) for any type that has side effect , then the GVNHash value should take into consideration
 *    of its node identity. A generaly rules is put the node's id() value into the GVNHash
 *    generation. Prefer using id() function instead of use this pointer address as seed.
 *
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
std::uint64_t GVNHash3( T* ptr , const V1& v1 , const V2& v2 , const V3& v3 ) {
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

// ==========================================================================
//
// Expr
//
//   This node is the mother all other expression node and its solo
//   goal is to expose def-use and use-def chain into different types
//
// ==========================================================================

class Expr : public Node {
  static const std::uint32_t kHasSideEffect = 1;
  static const std::uint32_t kNoSideEffect  = 0;
 public: // GVN hash value and hash function
  virtual std::uint64_t GVNHash()        const { return GVNHash1(type_name(),id()); }
  virtual bool Equal( const Expr* that ) const { return this == that;       }
 public:
  // Default operation to test whether 2 nodes are identical or not. it should be prefered
  // when 2 nodes are compared against identity. it means if they are equal , then one node
  // can be used to replace other
  bool IsReplaceable( const Expr* that ) const {
    if(!HasSideEffect() && !that->HasSideEffect())
      return this == that || Equal(that);
    return false;
  }
 public: // The statement in our IR is still an expression, we could use
         // following function to test whether it is an actual statement
         // pint to a certain region
  bool  IsStatement()                   const { return stmt_.HasRef(); }
  void  set_statement_edge ( const StatementEdge& st ) { stmt_= st; }
  const StatementEdge& statement_edge() const { return stmt_; }
 public: // patching function helps to mutate any def-use and use-def
  // Replace *this* node with the input expression node. This replace
  // will only change all the node that *reference* this node but not
  // touch all the operands' reference list
  virtual void Replace( Expr* );
 public:
  // Operand list ----------------------------------------------------------
  //
  // This list returns a list of operands used by this Expr/IR node. Most
  // of time operand_list will return at most 3 operands except for call
  // function
  const OperandList* operand_list() const { return &operand_list_; }
  // This function will add the input node into this node's operand list and
  // it will take care of the input node's ref list as well
  inline void AddOperand( Expr* node );
  // Replace an existed operand with input operand at position
  inline void ReplaceOperand( std::size_t , Expr* );
  // Effect list -----------------------------------------------------------
  // Used to develop dependency between expression which cannot be expressed
  // as data flow operation. Mainly used to order certain operations
  //
  // Effect list is essentially loosed and it will have duplicated node. To
  // avoid too much duplicated node we can use AddEffectIfNotExist to check
  // whether we have that value added ; but it does a linear search so it is
  // not performant. The effect list maintain is a best effort in terms of
  // dedup.
  const EffectList* effect_list() const { return &effect_list_; }
  // add a node into the effect list, it will refuse to add effect node when
  // it is either NoMemoryRead/NoMemoryWrite since these nodes are just placeholders
  inline void AddEffect   ( Expr* node );
  // add a node only when the effect node not show up inside of the effect list.
  // this function should be invoked with cautious since it is time costy due to
  // the linear finding internally
  void AddEffectIfNotExist( Expr* );
  // Reference list -------------------------------------------------------
  //
  // This list returns a list of Ref object which allow user to
  //   1) get a list of expression that uses this expression, ie who uses me
  //   2) get the corresponding iterator where *me* is inserted into the list
  //      so we can fast modify/remove us from its list
  const OperandRefList* ref_list() const { return &ref_list_; }
  // Add the referece into the reference list
  void AddRef( Node* who_uses_me , const OperandIterator& iter ) {
    ref_list_.PushBack(zone(),OperandRef(iter,who_uses_me));
  }
  // Remove a reference from the reference list whose ID == itr
  bool RemoveRef( const OperandIterator& itr , Expr* who_uses_me );
  // check if this expression is used by any other expression, basically
  // check whether ref_list is empty or not
  //
  // this check may not be accurate once the node is deleted/removed since
  // once a node is removed, we don't clean its ref_list but it is not used
  // essentially
  bool HasRef() const { return !ref_list()->empty(); }
 public:
  // check if this expression node has side effect if it is used as operand
  inline bool IsMemoryWrite() const;
  inline bool IsMemoryRead () const;
  inline bool IsMemoryOp   () const;
  // check if this expression node is a memory node
  inline bool IsMemoryNode () const;
  // check if this expression node is either NoMemoryRead or NoMemoryWrite
  inline bool IsNoMemoryEffectNode() const;
  // check if this node is any sort of Phi node , this includes normal expression
  // phi , ie Phi or read/write effect phi node
  inline bool IsPhiNode    () const;
  // Check if this Expression is a Leaf node or not
  inline bool IsLeaf()     const;
  bool        IsNoneLeaf() const { return !IsLeaf(); }
  IRInfo*     ir_info()    const { return ir_info_.ptr(); }
  // cast
  inline MemoryWrite*       AsMemoryWrite();
  inline const MemoryWrite* AsMemoryWrite() const;
  inline MemoryRead *       AsMemoryRead ();
  inline const MemoryRead*  AsMemoryRead () const;
  inline MemoryOp*          AsMemoryOp   ();
  inline const MemoryOp*    AsMemoryOp   () const;
  inline MemoryNode*        AsMemoryNode ();
  inline const MemoryNode*  AsMemoryNode () const;
  // Check whether this expression has side effect , or namely one of its descendent
  // operands has a none empty effect list
  bool HasSideEffect()     const { return ir_info_.state() == kHasSideEffect; }
  // constructor
  Expr( IRType type , std::uint32_t id , Graph* graph , IRInfo* info ):
    Node             (type,id,graph),
    operand_list_    (),
    effect_list_     (),
    ref_list_        (),
    stmt_            (),
    ir_info_         (info)
  {}
 private:
  // mark this node has side effect
  void SetHasSideEffect() { ir_info_.set_state( kHasSideEffect); }

  OperandList        operand_list_;
  EffectList         effect_list_;
  OperandRefList     ref_list_;
  StatementEdge      stmt_;
  TaggedPtr<IRInfo>  ir_info_;
};

// MemoryNode
// This node represents those nodes that is 1) mutable and 2) stay on heap or potentially
// stay on heap.
// 1. Arg
// 2. GGet
// 3. UGet
// 4. IRList
// 5. IRObject
//
// The above nodes are memory node since the mutation on these objects generates a observable
// side effect which must be serialized. For each operation , we will find its memory node
// if applicable and then all the operations will be serialized during graph building phase to
// ensure correct program behavior
class MemoryNode : public Expr {
 public:
  MemoryNode( IRType type , std::uint32_t id , Graph* g , IRInfo* info ): Expr(type,id,g,info){}
};

// MemoryOp
// This node demonstrats those operations which can cause side effect from perspectives of
// 1) read and 2) write.
class MemoryOp : public Expr {
 public:
  MemoryOp( IRType type , std::uint32_t id , Graph* g , IRInfo* info ): Expr(type,id,g,info){}
};

// MemoryWrite
// this node represents all possible memory store operation which essentially
// can cause observable side effect inside of program. The node that is MemoryWrite
// are :
// 1. USet, this node will have side effect since we don't know its alias situation
// 2. Arg , same as UGet
// 3. GSet , in current implementation global are treated very simple it will
//    generate a side effect so it is always ordered
// 4. ISet/PSet/ObjectSet/ListSet
// 5. WriteEffectPhi
class MemoryWrite : public MemoryOp {
 public:
  MemoryWrite( IRType type , std::uint32_t id , Graph* g , IRInfo* info ): MemoryOp(type,id,g,info){}
};

// MemoryRead
//
// this node represents all possible memory load operation which needs to depend
// on certain memory read node and also *be* depended on by other memory write
// node. The following IR nodes are type of MemoryRead node :
// 1. IGet
// 2. PGet
// 3. ObjectGet
// 4. ListGet
class MemoryRead : public MemoryOp {
 public:
  MemoryRead( IRType type , std::uint32_t id , Graph* g , IRInfo* info ): MemoryOp(type,id,g,info) {}
};

/* ---------------------------------------------------
 *
 * Node Definition
 *
 * --------------------------------------------------*/

class Arg : public MemoryNode {
 public:
  inline static Arg* New( Graph* , std::uint32_t );
  std::uint32_t index() const { return index_; }

  Arg( Graph* graph , std::uint32_t id , std::uint32_t index ):
    MemoryNode (HIR_ARG,id,graph,NULL),
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
    Expr  (HIR_FLOAT64,id,graph,info),
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
    Expr  (HIR_BOOLEAN,id,graph,info),
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
    Expr  (HIR_LONG_STRING,id,graph,info),
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
    Expr (HIR_SMALL_STRING,id,graph,info),
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
    Expr(HIR_NIL,id,graph,info)
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

class IRList : public MemoryNode {
 public:
  inline static IRList* New( Graph* , std::size_t size , IRInfo* );
  void Add( Expr* node ) { AddOperand(node); }
  std::size_t Size() const { return operand_list()->size(); }
  IRList( Graph* graph , std::uint32_t id , std::size_t size , IRInfo* info ):
    MemoryNode(HIR_LIST,id,graph,info)
  {
    (void)size; // implicit indicated by the size of operand_list()
  }

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
  Expr* value() const { return operand_list()->Last (); }
  void set_key  ( Expr* key ) { lava_debug(NORMAL,lava_verify(key->IsString());); ReplaceOperand(0,key); }
  void set_value( Expr* val ) { ReplaceOperand(1,val); }
  IRObjectKV( Graph* graph , std::uint32_t id , Expr* key , Expr* val , IRInfo* info ):
    Expr(HIR_OBJECT_KV,id,graph,info)
  {
    lava_debug(NORMAL,lava_verify(key->IsString()););
    AddOperand(key);
    AddOperand(val);
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(IRObjectKV)
};

class IRObject : public MemoryNode {
 public:
  inline static IRObject* New( Graph* , std::size_t size , IRInfo* );
  void Add( Expr* key , Expr* val , IRInfo* info ) {
    lava_debug(NORMAL,lava_verify(key->IsString()););
    AddOperand(IRObjectKV::New(graph(),key,val,info));
  }
  std::size_t Size() const { return operand_list()->size(); }
  IRObject( Graph* graph , std::uint32_t id , std::size_t size , IRInfo* info ):
    MemoryNode(HIR_OBJECT,id,graph,info)
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
    Expr (HIR_LOAD_CLS,id,graph,info),
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
  Expr*       operand() const { return operand_list()->First(); }
  Operator       op  () const { return op_;      }
  const char* op_name() const { return GetOperatorName(op()); }

  Unary( Graph* graph , std::uint32_t id , Expr* opr , Operator op , IRInfo* info ):
    Expr  (HIR_UNARY,id,graph,info),
    op_   (op)
  {
    AddOperand(opr);
  }

 protected:
  Unary( IRType type , Graph* graph , std::uint32_t id , Expr* opr , Operator op , IRInfo* info ):
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
    // arithmetic
    ADD, SUB, MUL, DIV, MOD, POW,
    // comparison
    LT , LE , GT , GE , EQ , NE ,
    // logic
    AND, OR ,
    // bitwise operators
    BAND, BOR , BXOR, BSHL, BSHR, BROL, BROR
  };

  inline static bool        IsComparisonOperator( Operator );
  inline static bool        IsArithmeticOperator( Operator );
  inline static bool        IsBitwiseOperator   ( Operator );
  inline static bool        IsLogicOperator     ( Operator );
  inline static Operator    BytecodeToOperator  ( interpreter::Bytecode );
  inline static const char* GetOperatorName     ( Operator );

 public:
  // Create a binary node
  inline static Binary* New( Graph* , Expr* , Expr* , Operator , IRInfo* );
  Expr*           lhs() const { return operand_list()->First(); }
  Expr*           rhs() const { return operand_list()->Last (); }
  Operator         op() const { return op_;  }
  const char* op_name() const { return GetOperatorName(op()); }

  Binary( Graph* graph , std::uint32_t id , Expr* lhs , Expr* rhs , Operator op , IRInfo* info ):
    Expr  (HIR_BINARY,id,graph,info),
    op_   (op)
  {
    AddOperand(lhs);
    AddOperand(rhs);
  }
 protected:
  Binary( IRType irtype ,Graph* graph ,std::uint32_t id ,Expr* lhs ,Expr* rhs ,Operator op,IRInfo* info ):
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
  Ternary( Graph* graph , std::uint32_t id , Expr* cond , Expr* lhs , Expr* rhs , IRInfo* info ):
    Expr  (HIR_TERNARY,id,graph,info)
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
// upvalue get/set
// -------------------------------------------------------------------------
class UGet : public MemoryNode {
 public:
  inline static UGet* New( Graph* , std::uint8_t , std::uint32_t , IRInfo* );
  std::uint8_t index()   const { return index_;  }
  std::uint32_t method() const { return method_; }

  UGet( Graph* graph , std::uint32_t id , std::uint8_t index , std::uint32_t method , IRInfo* info ):
    MemoryNode (HIR_UGET,id,graph,info),
    index_ (index),
    method_(method)
  {}
 public:
  virtual std::uint64_t GVNHash() const {
    return GVNHash1(type_name(),index());
  }
  virtual bool Equal( const Expr* that ) const {
    return that->IsUGet() && that->AsUGet()->index() == index();
  }
 private:
  std::uint8_t index_;
  std::uint32_t method_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(UGet)
};

class USet : public Expr {
 public:
  inline static USet* New( Graph* , std::uint8_t , std::uint32_t , Expr* opr , IRInfo* );
  std::uint32_t method() const { return method_; }
  std::uint8_t  index () const { return index_ ; }
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

  USet( Graph* graph , std::uint8_t id , std::uint8_t index , std::uint32_t method , Expr* value, IRInfo* info ):
    Expr    (HIR_USET,id,graph,info),
    index_  (index),
    method_ (method)
  {
    AddOperand(value);
  }
 private:
  std::uint8_t  index_;
  std::uint32_t method_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(USet)
};
// -------------------------------------------------------------------------
// property set/get (side effect)
// -------------------------------------------------------------------------
class PGet : public MemoryRead {
 public:
  inline static PGet* New( Graph* , Expr* , Expr* , IRInfo* , ControlFlow* );
  Expr* object() const { return operand_list()->First(); }
  Expr* key   () const { return operand_list()->Last (); }
  virtual Expr* Memory() const { return object(); }

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
  PGet( Graph* graph , std::uint32_t id , Expr* object , Expr* index , IRInfo* info ):
    MemoryRead (HIR_PGET,id,graph,info)
  {
    AddOperand(object);
    AddOperand(index );
  }
 protected:
  PGet( IRType type , Graph* graph , std::uint32_t id , Expr* object , Expr* index  , IRInfo* info ):
    MemoryRead(type,id,graph,info)
  {
    AddOperand(object);
    AddOperand(index );
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(PGet)
};

class PSet : public MemoryWrite {
 public:
  inline static PSet* New( Graph* , Expr* , Expr* , Expr* , IRInfo* , ControlFlow* );
  Expr* object() const { return operand_list()->First(); }
  Expr* key   () const { return operand_list()->Index(1);}
  Expr* value () const { return operand_list()->Last (); }
 public:
  virtual std::uint64_t GVNHash() const {
    return GVNHash3(type_name(),object()->GVNHash(),
                                key()->GVNHash(),
                                value()->GVNHash());
  }
  virtual bool Equal( const Expr* that ) const {
    if(that->IsPSet()) {
      auto that_pset = that->AsPSet();
      return object()->Equal(that_pset->object()) &&
             key   ()->Equal(that_pset->key())    &&
             value ()->Equal(that_pset->value());
    }
    return false;
  }
  PSet( Graph* graph , std::uint32_t id , Expr* object , Expr* index , Expr* value , IRInfo* info ):
    MemoryWrite(HIR_PSET,id,graph,info)
  {
    AddOperand(object);
    AddOperand(index );
    AddOperand(value );
  }

 protected:
  PSet(IRType type,Graph* graph,std::uint32_t id,Expr* object,Expr* index,Expr* value,IRInfo* info):
    MemoryWrite(type,id,graph,info)
  {
    AddOperand(object);
    AddOperand(index );
    AddOperand(value );
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(PSet)
};

class IGet : public MemoryRead {
 public:
  inline static IGet* New( Graph* , Expr* , Expr* , IRInfo* , ControlFlow* );
  Expr* object() const { return operand_list()->First(); }
  Expr* index () const { return operand_list()->Last (); }
  virtual Expr* Memory() const { return object(); }

  IGet( Graph* graph , std::uint32_t id , Expr* object , Expr* index , IRInfo* info ):
    MemoryRead (HIR_IGET,id,graph,info)
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
  IGet( IRType type , Graph* graph , std::uint32_t id , Expr* object , Expr* index , IRInfo* info ):
    MemoryRead(type,id,graph,info)
  {
    AddOperand(object);
    AddOperand(index );
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(IGet)
};

class ISet : public MemoryWrite {
 public:
  inline static ISet* New( Graph* , Expr* , Expr* , Expr* , IRInfo* , ControlFlow* );
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

  ISet( Graph* graph , std::uint32_t id , Expr* object , Expr* index , Expr* value , IRInfo* info ):
    MemoryWrite(HIR_ISET,id,graph,info)
  {
    AddOperand(object);
    AddOperand(index );
    AddOperand(value );
  }

 protected:
  ISet(IRType type,Graph* graph,std::uint32_t id,Expr* object,Expr* index,Expr* value,IRInfo* info):
    MemoryWrite(type,id,graph,info)
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
class GGet : public MemoryNode {
 public:
  inline static GGet* New( Graph* , Expr* , IRInfo* , ControlFlow* );
  Expr* key() const { return operand_list()->First(); }

  GGet( Graph* graph , std::uint32_t id , Expr* name , IRInfo* info ):
    MemoryNode (HIR_GGET,id,graph,info)
  {
    AddOperand(name);
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(GGet)
};

class GSet : public Expr {
 public:
  inline static GSet* New( Graph* , Expr* key , Expr* value , IRInfo* , ControlFlow* );
  Expr* key () const { return operand_list()->First(); }
  Expr* value()const { return operand_list()->Last() ; }

  GSet( Graph* graph , std::uint32_t id , Expr* key , Expr* value , IRInfo* info ):
    Expr(HIR_GSET,id,graph,info)
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
    Expr  (HIR_ITR_NEW,id,graph,info)
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
    Expr  (HIR_ITR_NEXT,id,graph,info)
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
    Expr  (HIR_ITR_TEST,id,graph,info)
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
    Expr   (HIR_ITR_DEREF,id,graph,info)
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

  // Remove the phi node from its belonged region. The reason this one is just
  // a static function is because this function *doesn't* touch its ref_list,
  // so its ref_list will still have its belonged region's reference there and
  // it is invalid. This function should be used under strict condition.
  static inline void RemovePhiFromRegion( Phi* );
  // Get the boundede region
  ControlFlow* region() const { return region_; }
  // Check if this Phi node is not used. We cannot use HasRef function since
  // a Phi node may added to a region during setup time and there will be one
  // ref inside of the RefList. We just need to check that
  bool IsUsed() const {
    return !(region() ? ref_list()->size() == 1 : (ref_list()->empty()));
  }
  // Check if this Phi node is in intermediate state. A phi node will generated
  // at the front the loop and it will only have on operand then. If phi is in
  // this stage, then it is an intermediate state
  bool IsIntermediateState() const { return operand_list()->size() == 1; }

  // Bounded control flow region node.
  // Each phi node is bounded to a control flow regional node
  // and by this we can easily decide which region contributs
  // to a certain input node of Phi node
  inline Phi( Graph* , std::uint32_t , ControlFlow* , IRInfo* );
 private:
  ControlFlow* region_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Phi)
};

// -------------------------------------------------------------------------
// EffectPhi
//
// A phi node that is used to merge effect right after the control flow. It
// will only be used inside of some expression's effect list
//
// -------------------------------------------------------------------------
class ReadEffectPhi : public MemoryRead {
 public:
  inline static ReadEffectPhi* New( Graph* , ControlFlow* , IRInfo* );
  inline static ReadEffectPhi* New( Graph* , MemoryRead* , MemoryRead* , ControlFlow* , IRInfo* );
  ControlFlow* region() const { return region_; }
  inline ReadEffectPhi( Graph* , std::uint32_t , ControlFlow* , IRInfo* );
 private:
  ControlFlow* region_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(ReadEffectPhi);
};

class WriteEffectPhi : public MemoryWrite {
 public:
  inline static WriteEffectPhi* New( Graph* , ControlFlow* , IRInfo* );
  inline static WriteEffectPhi* New( Graph* , MemoryWrite* , MemoryWrite* , ControlFlow* , IRInfo* );
  ControlFlow* region() const { return region_; }
  inline WriteEffectPhi( Graph* , std::uint32_t , ControlFlow* , IRInfo* );
 private:
  ControlFlow* region_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(WriteEffectPhi);
};

// placeholder for empty read/write effect to avoid checking NULL pointer
class NoReadEffect: public MemoryRead {
 public:
  inline static NoReadEffect* New( Graph* );
  NoReadEffect( Graph* graph , std::uint32_t id ): MemoryRead(HIR_NO_READ_EFFECT,id,graph,NULL) {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(NoReadEffect);
};

class NoWriteEffect: public MemoryWrite {
 public:
  inline static NoWriteEffect* New( Graph* );
  NoWriteEffect( Graph* graph , std::uint32_t id ): MemoryWrite(HIR_NO_WRITE_EFFECT,id,graph,NULL) {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(NoWriteEffect);
};

// -------------------------------------------------------------------------
// Function Call node
// -------------------------------------------------------------------------
class Call : public Expr {
 public:
  inline static Call* New( Graph* graph , Expr* , std::uint8_t , std::uint8_t , IRInfo* );
  Call( Graph* graph , std::uint32_t id , Expr* obj , std::uint8_t base , std::uint8_t narg , IRInfo* info ):
    Expr  (HIR_CALL,id,graph,info),
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
  inline static ICall* New( Graph* , interpreter::IntrinsicCall , bool tail , IRInfo* );
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

  ICall( Graph* graph , std::uint32_t id , interpreter::IntrinsicCall ic , bool tail , IRInfo* info ):
    Expr(HIR_ICALL,id,graph,info),
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
  Projection( Graph* graph , std::uint32_t id , Expr* operand , std::uint32_t index , IRInfo* info ):
    Expr  (HIR_PROJECTION,id,graph,info),
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
    Expr (HIR_INIT_CLS,id,graph,info)
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
  virtual std::uint64_t GVNHash() const {
    return GVNHash1(type_name(),index());
  }
  virtual bool Equal( const Expr* that ) const {
    return that->IsOSRLoad() && (that->AsOSRLoad()->index() == index());
  }
  OSRLoad( Graph* graph , std::uint32_t id , std::uint32_t index ):
    Expr  ( HIR_OSR_LOAD , id , graph , NULL ),
    index_(index)
  {}
 private:
  std::uint32_t index_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(OSRLoad)
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
  Checkpoint( Graph* graph , std::uint32_t id ):
    Expr(HIR_CHECKPOINT,id,graph,NULL)
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
    Expr(HIR_STACK_SLOT,id,graph,NULL),
    index_(index)
  {
    AddOperand(expr);
  }

 private:
  std::uint32_t index_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(StackSlot)
};

/* -------------------------------------------------------
 * Testing node , used with Guard node or If node
 * ------------------------------------------------------*/

class TestType : public Expr {
 public:
  inline static TestType* New( Graph* , TypeKind , Expr* , IRInfo* );

  TypeKind type_kind() const { return type_kind_; }
  const char* type_kind_name() const { return GetTypeKindName(type_kind_); }
  Expr* object() const { return operand_list()->First(); }

  TestType( Graph* graph , std::uint32_t id , TypeKind tc , Expr* obj, IRInfo* info ):
    Expr(HIR_TEST_TYPE,id,graph,info),
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

  TestListOOB( Graph* graph , std::uint32_t id , Expr* obj , Expr* idx , IRInfo* info ):
    Expr(HIR_TEST_LISTOOB,id,graph,info)
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
    Expr(HIR_FLOAT64_NEGATE,id,graph,info)
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
  inline static Float64Arithmetic* New( Graph* , Expr*, Expr*, Operator, IRInfo* );
  Float64Arithmetic( Graph* graph , std::uint32_t id , Expr* lhs, Expr* rhs, Operator op, IRInfo* info ):
    Binary(HIR_FLOAT64_ARITHMETIC,graph,id,lhs,rhs,op,info)
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
  inline static Float64Bitwise* New( Graph* , Expr*, Expr*, Operator, IRInfo* );
  Float64Bitwise( Graph* graph , std::uint32_t id , Expr* lhs, Expr* rhs, Operator op, IRInfo* info ):
    Binary(HIR_FLOAT64_BITWISE,graph,id,lhs,rhs,op,info)
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
  inline static Float64Compare* New( Graph* , Expr* , Expr* , Operator, IRInfo* );
  Float64Compare( Graph* graph , std::uint32_t id , Expr* lhs , Expr* rhs , Operator op , IRInfo* info ):
    Binary(HIR_FLOAT64_COMPARE,graph,id,lhs,rhs,op,info)
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
  inline static StringCompare* New( Graph* , Expr* , Expr* , Operator , IRInfo* );
  StringCompare( Graph* graph , std::uint32_t id , Expr* lhs , Expr* rhs , Operator op , IRInfo* info ):
    Binary(HIR_STRING_COMPARE,graph,id,lhs,rhs,op,info)
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
  inline static SStringEq* New( Graph* , Expr* , Expr* , IRInfo* );
  SStringEq( Graph* graph , std::uint32_t id , Expr* lhs , Expr* rhs , IRInfo* info ):
    Binary(HIR_SSTRING_EQ,graph,id,lhs,rhs,Binary::EQ,info)
  {}

 public:
  virtual std::uint64_t GVNHash()        const { return GVNHashImpl(); }
  virtual bool Equal( const Expr* that ) const { return EqualImpl(that); }
};

class SStringNe : public Binary , public detail::Float64BinaryGVNImpl<SStringNe> {
 public:
  inline static SStringNe* New( Graph* , Expr* , Expr* , IRInfo* );
  SStringNe( Graph* graph , std::uint32_t id , Expr* lhs , Expr* rhs , IRInfo* info ):
    Binary(HIR_SSTRING_EQ,graph,id,lhs,rhs,Binary::NE,info)
  {}

 public:
  virtual std::uint64_t GVNHash()        const { return GVNHashImpl(); }
  virtual bool Equal( const Expr* that ) const { return EqualImpl(that); }
};

// Specialized logic operator, its lhs is type fixed by boolean. ie, we are
// sure its lhs operand outputs boolean in an unboxed format.
class BooleanNot: public Expr {
 public:
  inline static BooleanNot* New( Graph* , Expr* , IRInfo* );
  Expr* operand() const { return operand_list()->First(); }
  BooleanNot( Graph* graph , std::uint32_t id , Expr* opr , IRInfo* info ):
    Expr(HIR_BOOLEAN_NOT,id,graph,info)
  {
    AddOperand(opr);
  }

 public:
  virtual std::uint64_t GVNHash() const {
    return GVNHash1(type_name(),operand()->GVNHash());
  }

  virtual bool Equal( const Expr* that ) const {
    if(that->IsBooleanNot()) {
      auto that_negate = that->AsBooleanNot();
      return operand()->Equal(that_negate->operand());
    }
    return false;
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(BooleanNot)
};

class BooleanLogic : public Binary {
 public:
   inline static BooleanLogic* New( Graph* , Expr* , Expr* , Operator op , IRInfo* );
   BooleanLogic( Graph* graph , std::uint32_t id , Expr* lhs , Expr* rhs , Operator op, IRInfo* info ):
     Binary(HIR_BOOLEAN_LOGIC,graph,id,lhs,rhs,op,info)
  {
    lava_debug(NORMAL,lava_verify( GetTypeInference(lhs) == TPKIND_BOOLEAN &&
                                   GetTypeInference(rhs) == TPKIND_BOOLEAN ););
  }

 public:
  virtual std::uint64_t GVNHash()        const {
    return GVNHash3(type_name(), lhs()->GVNHash(), op (), rhs()->GVNHash());
  }
  virtual bool Equal( const Expr* that ) const {
    if(that->IsBooleanLogic()) {
      auto bl = that->AsBooleanLogic();
      return bl->op() == op() && bl->lhs()->Equal(lhs()) && bl->rhs()->Equal(rhs());
    }
    return false;
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(BooleanLogic)
};

class ObjectGet : public PGet {
 public:
  inline static ObjectGet* New( Graph* , Expr* , Expr* , IRInfo* );
  ObjectGet( Graph* graph , std::uint32_t id , Expr* object , Expr* key, IRInfo* info ):
    PGet(HIR_OBJECT_GET,graph,id,object,key,info)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ObjectGet)
};

class ObjectSet : public PSet {
 public:
  inline static ObjectSet* New( Graph* , Expr* , Expr* , Expr* , IRInfo* );
  ObjectSet( Graph* graph , std::uint32_t id , Expr* object , Expr* key, Expr* value, IRInfo* info ):
    PSet(HIR_OBJECT_SET,graph,id,object,key,value,info)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ObjectSet)
};

class ListGet : public IGet {
 public:
  inline static ListGet* New( Graph* , Expr* , Expr* , IRInfo* );
  ListGet( Graph* graph , std::uint32_t id, Expr* object, Expr* index , IRInfo* info ):
    IGet(HIR_LIST_GET,graph,id,object,index,info)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ListGet)
};

class ListSet : public ISet {
 public:
  inline static ListSet* New( Graph* , Expr* , Expr* , Expr* , IRInfo* );
  ListSet( Graph* graph , std::uint32_t id , Expr* object , Expr* index  , Expr* value  , IRInfo* info ):
    ISet(HIR_LIST_SET,graph,id,object,index,value,info)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ListSet)
};

// -------------------------------------------------------------------------
//  Box/Unbox
// -------------------------------------------------------------------------
class Box : public Expr {
 public:
  inline static Box* New( Graph* , Expr* , TypeKind , IRInfo* );
  Expr* value() const { return operand_list()->First(); }
  TypeKind type_kind() const { return type_kind_; }
  Box( Graph* graph , std::uint32_t id , Expr* object , TypeKind tk , IRInfo* info ):
    Expr(HIR_BOX,id,graph,info),
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
  Unbox( Graph* graph , std::uint32_t id , Expr* object , TypeKind tk , IRInfo* info ):
    Expr(HIR_UNBOX,id,graph,info),
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
// Cast
//
// -----------------------------------------------------------------------

// Cast an expression node into a *boxed* boolean value. This cast will always be
// successful due to our language's semantic. Any types of value has a corresponding
// boolean value.
class CastToBoolean : public Expr {
 public:
  inline static CastToBoolean* New( Graph* , Expr* , IRInfo* );
  // function to create a cast to boolean but negate its end result. this operation
  // basically means negate(unbox(cast_to_boolean(node)))
  inline static Expr* NewNegateCast( Graph* , Expr* , IRInfo* );
  Expr* value() const { return operand_list()->First(); }

  CastToBoolean( Graph* graph , std::uint32_t id , Expr* value , IRInfo* info ):
    Expr( HIR_CAST_TO_BOOLEAN , id , graph , info )
  { AddOperand(value); }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(CastToBoolean)
};

// -----------------------------------------------------------------------
//
// Annotation
//
// -----------------------------------------------------------------------
class TypeAnnotation : public Expr {
 public:
  inline static TypeAnnotation* New( Graph* , Guard* , IRInfo* );
  TypeKind type_kind () const { return type_kind_; }

  inline TypeAnnotation( Graph* , std::uint32_t , Guard* , IRInfo* );
 private:
  TypeKind type_kind_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(TypeAnnotation)
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
  // special case that only one backward edge we have
  ControlFlow* parent() const {
    lava_debug(NORMAL,lava_verify(backward_edge()->size() == 1););
    return backward_edge()->First();
  }
  // backward
  const RegionList* backward_edge() const {
    return &backward_edge_;
  }
  void AddBackwardEdge( ControlFlow* edge ) {
    AddBackwardEdgeImpl(edge);
    edge->AddForwardEdgeImpl(this);
  }
  void RemoveBackwardEdge( ControlFlow* );
  void RemoveBackwardEdge( std::size_t index );
  void ClearBackwardEdge () { backward_edge_.Clear(); }
  // forward
  const RegionList* forward_edge() const {
    return &forward_edge_;
  }
  void AddForwardEdge ( ControlFlow* edge ) {
    AddForwardEdgeImpl(edge);
    edge->AddBackwardEdgeImpl(this);
  }
  void RemoveForwardEdge( ControlFlow* edge );
  void RemoveForwardEdge( std::size_t index );
  void ClearForwardEdge () { forward_edge_.Clear(); }

  // reflist
  const RegionRefList* ref_list() const {
    return &ref_list_;
  }
  // Add the referece into the reference list
  void AddRef( ControlFlow* who_uses_me , const RegionListIterator& iter ) {
    ref_list_.PushBack(zone(),RegionRef(iter,who_uses_me));
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
  void AddStatement( Expr* node ) {
    auto itr = stmt_expr_.PushBack(zone(),node);
    node->set_statement_edge(StatementEdge(this,itr));
  }
  void RemoveStatement( const StatementEdge& ee ) {
    lava_debug(NORMAL,lava_verify(ee.region == this););
    stmt_expr_.Remove(ee.iterator);
  }
  void MoveStatement( ControlFlow* );

  // OperandList
  //
  // All control flow's related data input should be stored via this list
  // since this list supports expression substitution/replacement. It is
  // used in all optimization pass
  const OperandList* operand_list() const {
    return &operand_list_;
  }
  void AddOperand( Expr* node ) {
    auto itr = operand_list_.PushBack(zone(),node);
    node->AddRef(this,itr);
  }
  bool RemoveOperand( Expr* node ) {
    auto itr = operand_list_.Find(node);
    if(itr.HasNext()) { operand_list_.Remove(itr); return true; }
    return false;
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
  Region( Graph* graph , std::uint32_t id ): ControlFlow(HIR_REGION,id,graph) {}
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
    ControlFlow(HIR_LOOP_HEADER,id,graph,region)
  {}
 private:
  ControlFlow* merge_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(LoopHeader);
};

class Loop : public ControlFlow {
 public:
  inline static Loop* New( Graph* );

  Loop( Graph* graph , std::uint32_t id ):
    ControlFlow(HIR_LOOP,id,graph)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Loop)
};

class LoopExit : public ControlFlow {
 public:
  inline static LoopExit* New( Graph* , Expr* );
  Expr* condition() const { return operand_list()->First(); }

  LoopExit( Graph* graph , std::uint32_t id , Expr* cond ):
    ControlFlow(HIR_LOOP_EXIT,id,graph)
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
// Branch node in HIR basically has 3 types :
//
// 1) If related node , which basically represents a normal written if else
//    branch
//
// 2) Guard node , which is used to generate *guard* during the execution,
//    an assertion is a single pass node which will *not* have any else branch.
//    An assertion failed will trigger a bailout operation which basically
//    fallback to the interpreter
//
// 3) Unconditional jump
// -----------------------------------------------------------------------

class If : public ControlFlow {
 public:
  inline static If* New( Graph* , Expr* , ControlFlow* );
  Expr* condition() const { return operand_list()->First(); }

  ControlFlow* merge() const { return merge_; }
  void set_merge( ControlFlow* merge ) { merge_ = merge; }

  If( Graph* graph , std::uint32_t id , Expr* cond , ControlFlow* region ):
    ControlFlow(HIR_IF,id,graph,region),
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
    ControlFlow(HIR_IF_TRUE,id,graph,region)
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
    ControlFlow(HIR_IF_FALSE,id,graph,region)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(IfFalse)
};

// --------------------------------------------------------------------------
// Guard
//
// This node is mainly generated for type assertion or other assertion to do
// speculative optimization against the source code. The assert node doesn't
// introduce any other basic block except the fall through part. An assertion
// failed during runtime means *bailout* back to interpreter
class Guard : public ControlFlow {
 public:
  inline static Guard* New( Graph* , Expr* , Checkpoint* , ControlFlow* );

  Expr*             test() const { return operand_list()->First(); }
  Checkpoint* checkpoint() const { return operand_list()->Last()->AsCheckpoint(); }

  Guard( Graph* graph , std::uint32_t id , Expr* test , Checkpoint* cp , ControlFlow* region ):
    ControlFlow(HIR_GUARD,id,graph,region)
  {
    AddOperand(test);
    AddOperand(cp);
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Guard)
};

class Jump : public ControlFlow {
 public:
  inline static Jump* New( Graph* , const std::uint32_t* , ControlFlow* );
  // which target this jump jumps to
  ControlFlow* target() const { return target_; }
  inline bool TrySetTarget( const std::uint32_t* , ControlFlow* );

  Jump( Graph* graph , std::uint32_t id , ControlFlow* region , const std::uint32_t* bytecode_bc ):
    ControlFlow(HIR_JUMP,id,graph,region),
    target_(NULL),
    bytecode_pc_(bytecode_bc)
  {}

 private:
  ControlFlow* target_; // where this Jump node jumps to
  const std::uint32_t* bytecode_pc_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Jump)
};

// Represent fallback to interpreter, manually generate this node means we want to
// abort at current stage in the graph.
class Trap : public ControlFlow {
 public:
  inline static Trap* New( Graph* , Checkpoint* , ControlFlow* );
  Checkpoint* checkpoint() const { return operand_list()->First()->AsCheckpoint(); }
  Trap( Graph* graph , std::uint32_t id , Checkpoint* cp ,
                                          ControlFlow* region ):
    ControlFlow(HIR_TRAP,id,graph,region)
  {
    AddOperand(cp);
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Trap)
};

// Fail node represents abnormal way to abort the execution. The most common reason
// is because we failed at type guard or obviouse code bug.
class Fail : public ControlFlow {
 public:
  inline static Fail* New( Graph* );
  Fail( Graph* graph , std::uint32_t id ): ControlFlow(HIR_FAIL,id,graph) {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Fail)
};

class Success : public ControlFlow {
 public:
  inline static Success* New( Graph* );

  Expr* return_value() const { return operand_list()->First(); }

  Success( Graph* graph , std::uint32_t id ):
    ControlFlow  (HIR_SUCCESS,id,graph)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Success)
};

class Return : public ControlFlow {
 public:
  inline static Return* New( Graph* , Expr* , ControlFlow* );
  Expr* value() const { return operand_list()->First(); }
  Return( Graph* graph , std::uint32_t id , Expr* value , ControlFlow* region ):
    ControlFlow(HIR_RETURN,id,graph,region)
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
  Start( Graph* graph , std::uint32_t id ): ControlFlow(HIR_START,id,graph) {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Start)
};

class End : public ControlFlow {
 public:
  inline static End* New( Graph* , Success* , Fail* );
  Success* success() const { return backward_edge()->First()->AsSuccess(); }
  Fail*    fail()    const { return backward_edge()->Last ()->AsFail   (); }
  End( Graph* graph , std::uint32_t id , Success* s , Fail* f ):
    ControlFlow(HIR_END,id,graph)
  {
    AddBackwardEdge(s);
    AddBackwardEdge(f);
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(End)
};

class OSRStart : public ControlFlow {
 public:
  inline static OSRStart* New( Graph* );

  OSRStart( Graph* graph  , std::uint32_t id ):
    ControlFlow(HIR_OSR_START,id,graph)
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
    ControlFlow(HIR_OSR_END,id,graph)
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

 public: // placeholder nodes
  NoReadEffect*  no_read_effect()  const { return no_read_effect_; }
  NoWriteEffect* no_write_effect() const { return no_write_effect_; }

 public: // getter and setter
  ControlFlow*      start() const { return start_; }
  ControlFlow*      end  () const { return end_;   }
  zone::Zone*       zone()        { return &zone_; }
  const zone::Zone* zone()  const { return &zone_; }
  std::uint32_t     MaxID() const { return id_; }
  std::uint32_t     AssignID()    { return id_++; }
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
 private:
  zone::Zone                  zone_;
  ControlFlow*                start_;
  ControlFlow*                end_;
  zone::Vector<PrototypeInfo> prototype_info_;
  std::uint32_t               id_;
  // placeholder nodes, context free nodes basically
  NoReadEffect*               no_read_effect_;
  NoWriteEffect*              no_write_effect_;

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
  typedef     ControlFlow* ValueType;
  typedef       ValueType& ReferenceType;
  typedef ValueType const& ConstReferenceType;

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
  ConstReferenceType value() const { lava_debug(NORMAL,lava_verify(HasNext());); return next_; }
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
  typedef     ControlFlow* ValueType;
  typedef       ValueType& ReferenceType;
  typedef ValueType const& ConstReferenceType;

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
  ConstReferenceType value() const { lava_debug(NORMAL,lava_verify(HasNext());); return next_; }
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
  typedef     ControlFlow* ValueType;
  typedef       ValueType& ReferenceType;
  typedef ValueType const& ConstReferenceType;

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
  ConstReferenceType value() const { lava_debug(NORMAL,lava_verify(HasNext());); return next_; }
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

  typedef       Edge  ValueType;
  typedef       Edge& ReferenceType;
  typedef const Edge& ConstReferenceType;
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
  ConstReferenceType value() const { lava_debug(NORMAL,lava_verify(HasNext());); return next_; }
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
  typedef            Expr* ValueType;
  typedef       ValueType& ReferenceType;
  typedef ValueType const& ConstReferenceType;

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
  ConstReferenceType value() const { lava_debug(NORMAL,lava_verify(HasNext());); return next_; }
 private:
  Expr* root_;
  Expr* next_;
  OnceList stack_;
};

// -------------------------------------------------------------------------
//
// Helper
//
// -------------------------------------------------------------------------
inline Expr* NewString           ( Graph* , const void* , std::size_t length , IRInfo* );
inline Expr* NewString           ( Graph* , const char* , IRInfo* );
inline Expr* NewString           ( Graph* , const zone::String* , IRInfo* );
inline Expr* NewStringFromBoolean( Graph* , bool , IRInfo* );
inline Expr* NewStringFromReal   ( Graph* , double , IRInfo* );

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

#include "hir-inl.h"

#endif // CBASE_HIR_H_
