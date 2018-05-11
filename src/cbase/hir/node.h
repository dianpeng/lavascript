#ifndef CBASE_HIR_NODE_H_
#define CBASE_HIR_NODE_H_
#include "src/config.h"
#include "src/util.h"
#include "src/stl-helper.h"
#include "src/zone/zone.h"
#include "src/zone/vector.h"
#include "src/zone/list.h"
#include "src/zone/string.h"
#include "src/cbase/bytecode-analyze.h"
#include "src/cbase/type.h"
#include "src/cbase/type-inference.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

// Constant node
#define CBASE_HIR_CONSTANT(__)                                         \
  __(Float64,FLOAT64     ,"float64"     ,Leaf,NoEffect)                \
  __(LString,LONG_STRING ,"lstring"     ,Leaf,NoEffect)                \
  __(SString,SMALL_STRING,"small_string",Leaf,NoEffect)                \
  __(Boolean,BOOLEAN     ,"boolean"     ,Leaf,NoEffect)                \
  __(Nil    ,NIL         ,"null"        ,Leaf,NoEffect)

// High level HIR node. Used to describe unttyped polymorphic operations
#define CBASE_HIR_EXPRESSION_HIGH(__)                                  \
  /* compound */                                                       \
  __(IRList       ,LIST     ,"list"     ,NoLeaf,NoEffect)              \
  __(IRObjectKV   ,OBJECT_KV,"object_kv",NoLeaf,NoEffect)              \
  __(IRObject     ,OBJECT   ,"object"   ,NoLeaf,NoEffect)              \
  /* closure */                                                        \
  __(Closure      ,CLOSURE  ,"closure"  ,Leaf  ,Effect)                \
  __(InitCls      ,INIT_CLS ,"init_cls" ,NoLeaf,Effect)                \
  /* argument node */                                                  \
  __(Arg          ,ARG      ,"arg"      ,Leaf,NoEffect)                \
  /* arithmetic/comparison node */                                     \
  __(Unary        ,UNARY    ,"unary"    ,NoLeaf,NoEffect)              \
  __(Arithmetic   ,ARITHMETIC,"arithmetic",NoLeaf,Effect)              \
  __(Compare      ,COMPARE  ,"compare"  ,NoLeaf,Effect)                \
  __(Logical      ,LOGICAL   ,"logical" ,NoLeaf,NoEffect)              \
  __(Ternary      ,TERNARY  ,"ternary"  ,NoLeaf,NoEffect)              \
  /* upvalue */                                                        \
  __(UGet         ,UGET     ,"uget"     ,Leaf  ,NoEffect)              \
  __(USet         ,USET     ,"uset"     ,Leaf  ,Effect)                \
  /* property/idx */                                                   \
  __(PGet         ,PGET     ,"pget"     ,NoLeaf,NoEffect)              \
  __(PSet         ,PSET     ,"pset"     ,NoLeaf,Effect)                \
  __(IGet         ,IGET     ,"iget"     ,NoLeaf,NoEffect)              \
  __(ISet         ,ISET     ,"iset"     ,NoLeaf,Effect)                \
  /* gget */                                                           \
  __(GGet         ,GGET     ,"gget"     ,NoLeaf,NoEffect)              \
  __(GSet         ,GSET     ,"gset"     ,NoLeaf,Effect)                \
  /* iterator */                                                       \
  __(ItrNew       ,ITR_NEW  ,"itr_new"  ,NoLeaf,NoEffect)              \
  __(ItrNext      ,ITR_NEXT ,"itr_next" ,NoLeaf,NoEffect)              \
  __(ItrTest      ,ITR_TEST ,"itr_test" ,NoLeaf,NoEffect)              \
  __(ItrDeref     ,ITR_DEREF,"itr_deref",NoLeaf,NoEffect)              \
  /* call     */                                                       \
  __(Call         ,CALL     ,"call"     ,NoLeaf,Effect)                \
  /* intrinsic call */                                                 \
  __(ICall        ,ICALL    ,"icall"    ,NoLeaf,NoEffect)              \
  /* phi */                                                            \
  __(Phi           ,PHI     ,"phi"      ,NoLeaf,NoEffect)              \
  /* misc */                                                           \
  __(Projection   ,PROJECTION      ,"projection",NoLeaf,NoEffect)      \
  /* osr */                                                            \
  __(OSRLoad      ,OSR_LOAD        ,"osr_load"  ,Leaf,Effect)          \
  /* checkpoints */                                                    \
  __(Checkpoint   ,CHECKPOINT      ,"checkpoint" ,NoLeaf,Effect)       \
  __(StackSlot    ,STACK_SLOT      ,"stack_slot" ,NoLeaf,NoEffect)     \
  /* effect */                                                         \
  __(LoopEffect   ,LOOP_EFFECT     ,"loop_effect",NoLeaf,Effect)       \
  __(EffectPhi    ,EFFECT_PHI      ,"effect_phi" ,NoLeaf,Effect)       \
  __(EmptyBarrier ,EMPTY_BARRIER   ,"empty_barrier",NoLeaf,Effect)     \
  __(InitBarrier  ,INIT_BARRIER    ,"init_barrier",NoLeaf,Effect)

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
#define CBASE_HIR_EXPRESSION_LOW_ARITHMETIC_AND_COMPARE(__)                     \
  __(Float64Negate    ,FLOAT64_NEGATE    ,"float64_negate"    ,NoLeaf,NoEffect) \
  __(Float64Arithmetic,FLOAT64_ARITHMETIC,"float64_arithmetic",NoLeaf,NoEffect) \
  __(Float64Bitwise   ,FLOAT64_BITWISE   ,"float64_bitwise"   ,NoLeaf,NoEffect) \
  __(Float64Compare   ,FLOAT64_COMPARE   ,"float64_compare"   ,NoLeaf,NoEffect) \
  __(BooleanNot       ,BOOLEAN_NOT       ,"boolean_not"       ,NoLeaf,NoEffect) \
  __(BooleanLogic     ,BOOLEAN_LOGIC     ,"boolean_logic"     ,NoLeaf,NoEffect) \
  __(StringCompare    ,STRING_COMPARE    ,"string_compare"    ,NoLeaf,NoEffect) \
  __(SStringEq        ,SSTRING_EQ        ,"sstring_eq"        ,NoLeaf,NoEffect) \
  __(SStringNe        ,SSTRING_NE        ,"sstring_ne"        ,NoLeaf,NoEffect) \

#define CBASE_HIR_EXPRESSION_LOW_PROPERTY(__)                         \
  __(ObjectFind   ,OBJECT_FIND   ,"object_find"   ,NoLeaf,NoEffect)   \
  __(ObjectUpdate ,OBJECT_UPDATE ,"object_update" ,NoLeaf,Effect)     \
  __(ObjectInsert ,OBJECT_INSERT ,"object_insert" ,NoLeaf,Effect)     \
  __(ListIndex    ,LIST_INDEX    ,"list_index"    ,NoLeaf,NoEffect)   \
  __(ListInsert   ,LIST_INSERT   ,"list_insert"   ,NoLeaf,Effect)     \
  __(ObjectRefSet ,OBJECT_REF_SET,"object_ref_set",NoLeaf,NoEffect)   \
  __(ObjectRefGet ,OBJECT_REF_GET,"object_ref_get",NoLeaf,NoEffect)   \
  __(ListRefSet   ,LIST_REF_SET  ,"list_ref_set"  ,NoLeaf,NoEffect)   \
  __(ListRefGet   ,LIST_REF_GET  ,"list_ref_get"  ,NoLeaf,NoEffect)


// All the low HIR nodes
#define CBASE_HIR_EXPRESSION_LOW(__)                                  \
  CBASE_HIR_EXPRESSION_LOW_ARITHMETIC_AND_COMPARE(__)                 \
  CBASE_HIR_EXPRESSION_LOW_PROPERTY(__)

// Guard node.
//
// Guard node is an expression node so it is floating and can be GVNed
#define CBASE_HIR_GUARD(__)                                           \
  __(Guard       ,GUARD         ,"guard"          ,NoLeaf,NoEffect)

// Guard conditional node , used to do type guess or speculative inline
//
// A null test is same as TestType(object,NULL) since null doesn't have
// any value. And during graph construction any null test , ie x == null,
// will be *normalized* into a TestType then we could just use predicate
// to do inference and have null redundancy removal automatically
#define CBASE_HIR_TEST(__)                                            \
  __(TestType    ,TEST_TYPE      ,"test_type"      ,NoLeaf,NoEffect)

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
#define CBASE_HIR_BOXOP(__)                                           \
  __(Box  ,BOX  ,"box"  ,NoLeaf,NoEffect)                             \
  __(Unbox,UNBOX,"unbox",NoLeaf,NoEffect)

/**
 * Cast
 * Cast a certain type's value into another type's value. These type cast
 * doesn't perform any test and should be guaranteed to be correct by
 * the compiler
 */
#define CBASE_HIR_CAST(__)                                            \
  __(ConvBoolean ,CONV_BOOLEAN ,"conv_boolean" ,NoLeaf,NoEffect)      \
  __(ConvNBoolean,CONV_NBOOLEAN,"conv_nboolean",NoLeaf,NoEffect)


// All the expression IR nodes
#define CBASE_HIR_EXPRESSION(__)                                      \
  CBASE_HIR_CONSTANT(__)                                              \
  CBASE_HIR_EXPRESSION_HIGH(__)                                       \
  CBASE_HIR_EXPRESSION_LOW (__)                                       \
  CBASE_HIR_TEST(__)                                                  \
  CBASE_HIR_BOXOP(__)                                                 \
  CBASE_HIR_CAST (__)                                                 \
  CBASE_HIR_GUARD(__)                                                 \

// All the control flow IR nodes
#define CBASE_HIR_CONTROL_FLOW(__)                                    \
  __(Start      ,START      ,"start"      ,NoLeaf,NoEffect)           \
  __(End        ,END        ,"end"        ,NoLeaf,NoEffect)           \
  /* osr */                                                           \
  __(OSRStart   ,OSR_START  ,"osr_start"  ,NoLeaf,NoEffect)           \
  __(OSREnd     ,OSR_END    ,"osr_end"    ,NoLeaf,NoEffect)           \
  /* inline */                                                        \
  __(InlineStart,INLINE_START,"inline_start",NoLeaf,NoEffect)         \
  __(InlineEnd  ,INLINE_END  ,"inline_end"  ,NoLeaf,NoEffect)         \
  __(LoopHeader ,LOOP_HEADER,"loop_header",NoLeaf,NoEffect)           \
  __(Loop       ,LOOP       ,"loop"       ,NoLeaf,NoEffect)           \
  __(LoopExit   ,LOOP_EXIT  ,"loop_exit"  ,NoLeaf,NoEffect)           \
  __(If         ,IF         ,"if"         ,NoLeaf,NoEffect)           \
  __(IfTrue     ,IF_TRUE    ,"if_true"    ,NoLeaf,NoEffect)           \
  __(IfFalse    ,IF_FALSE   ,"if_false"   ,NoLeaf,NoEffect)           \
  __(Jump       ,JUMP       ,"jump"       ,NoLeaf,NoEffect)           \
  __(Fail       ,FAIL       ,"fail"       ,Leaf  ,NoEffect)           \
  __(Success    ,SUCCESS    ,"success"    ,NoLeaf,NoEffect)           \
  __(Return     ,RETURN     ,"return"     ,NoLeaf,NoEffect)           \
  __(JumpValue  ,JUMP_VALUE ,"jump_value" ,NoLeaf,NoEffect)           \
  __(Region     ,REGION     ,"region"     ,NoLeaf,NoEffect)           \
  __(CondTrap   ,COND_TRAP  ,"cond_trap"  ,NoLeaf,NoEffect)           \
  __(Trap       ,TRAP       ,"trap"       ,NoLeaf,NoEffect)

#define CBASE_HIR_LIST(__)                                            \
  CBASE_HIR_EXPRESSION(__)                                            \
  CBASE_HIR_CONTROL_FLOW(__)

enum IRType {
#define __(A,B,...) HIR_##B,
  /** expression related IRType **/
  CBASE_HIR_EXPRESSION_START,
  CBASE_HIR_EXPRESSION(__)
  CBASE_HIR_EXPRESSION_END,
  /** control flow related IRType **/
  CBASE_HIR_CONTROL_FLOW_START,
  CBASE_HIR_CONTROL_FLOW(__)
  CBASE_HIR_CONTROL_FLOW_END
#undef __ // __
};
#define SIZE_OF_HIR (CBASE_HIR_STMT_END-6)

// IR classes forward declaration
#define __(A,...) class A;
CBASE_HIR_LIST(__)
#undef __ // __

const char* IRTypeGetName( IRType );

// Forward class declaration
#define __(A,...) class A;
CBASE_HIR_LIST(__)
#undef __ // __

// Other none-leaf node forward declaration
class Graph;
class Node;
class Expr;
class ControlFlow;
class Test;
class ReadEffect;
class WriteEffect;
class EffectBarrier;
class HardBarrier;
class SoftBarrier;
class Binary;
class DynamicBinary;
class SpecializeBinary;
class MemoryOp;
class MemoryNode;

// IRType value static mapping
template< typename T > struct MapIRClassToIRType {};

#define __(A,B,...)                         \
  template<> struct MapIRClassToIRType<A> { \
    static const IRType value = HIR_##B;    \
    static bool Test( IRType type ) {       \
      return type == value;                 \
    }                                       \
  };

CBASE_HIR_LIST(__)

#undef __ // __


// all inline definition for MapIRClassToIRType's specialized class
#include "node-irtype-map-inl.cxx"

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

// ----------------------------------------------------------------------------
// Stmt list
//
// Bunch of statements that are not used by any expression but have observable
// effects. Example like : foo() , a free function call
typedef zone::List<Expr*> StmtList;
typedef StmtList::ForwardIterator StmtIterator;

// This structure is held by *all* the expression. If the region field is not
// NULL then it means this expression has side effect and it is bounded at
// certain control flow region
struct StmtEdge {
  ControlFlow* region;
  StmtIterator iterator;
  bool HasRef() const { return region != NULL; }

  StmtEdge( ControlFlow* r , const StmtIterator& itr ): region(r), iterator(itr) {}
  StmtEdge(): region(NULL), iterator() {}
};

// Reference
template< typename ITR >
struct Ref {
  ITR     id;  // iterator used for fast deletion of this Ref it is modified
  Node* node;
  Ref( const ITR& iter , Node* n ): id(iter),node(n) {}
  Ref(): id(), node(NULL) {}
};

// OperandList
typedef zone::List<Expr*>               OperandList;
typedef OperandList::ForwardIterator    OperandIterator;


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
  IRType type()           const { return type_; }
  // name/string of the type
  const char* type_name() const { return IRTypeGetName(type()); }
  // a unique id for this node , it can be used to indexed into secondary storage
  std::uint32_t id()      const { return id_; }
  // get the belonged graph object
  Graph* graph()          const { return graph_; }
  // get the belonged zone object from graph
  inline zone::Zone* zone() const;
  // check whether 2 nodes are same , pls do not use pointer comparison
  // due to the sick cpp memory layout makes pointer not the same value
  // even they are actually same object. If you want to do comparison ,
  // do something like static_cast<Node*>(a) == static_cast<Node*>(b) which
  // is the same as using IsIdentical function.
  bool IsIdentical( const Node* that ) const { return id() == that->id(); }
 public: // type check and cast
  template< typename T > bool            Is() const;
  template< typename T > inline       T* As();
  template< typename T > inline const T* As() const;

#define __(A,B,...) bool Is##A() const { return Is<A>(); }
  CBASE_HIR_LIST(__)
#undef __ // __

#define __(A,B,...) inline A* As##A(); inline const A* As##A() const;
  CBASE_HIR_LIST(__)
#undef __ // __

  bool                       IsString     () const { return IsSString() || IsLString(); }
  inline bool                IsControlFlow() const;
  inline bool                IsExpr       () const;
  inline bool                IsReadEffect () const;
  inline bool                IsWriteEffect() const;
  inline bool                IsMemoryNode () const;
  inline bool                IsTestNode   () const;
  inline bool                IsLeaf       () const;
  bool                       IsNoneLeaf   () const { return !IsLeaf(); }

  inline Expr*               AsExpr       ();
  inline const Expr*         AsExpr       () const;
  inline ControlFlow*        AsControlFlow();
  inline const ControlFlow*  AsControlFlow() const;
  inline const zone::String& AsZoneString () const;
  inline WriteEffect*        AsWriteEffect();
  inline const WriteEffect*  AsWriteEffect() const;
  inline ReadEffect*         AsReadEffect ();
  inline const ReadEffect*   AsReadEffect () const;
  inline MemoryNode*         AsMemoryNode ();
  inline const MemoryNode*   AsMemoryNode () const;
  inline Test*               AsTest       ();
  inline const Test*         AsTest       () const;

 protected:
  Node( IRType type , std::uint32_t id , Graph* graph ):type_(type),id_(id),graph_(graph) {}
 private:
  IRType        type_;
  std::uint32_t id_;
  Graph*        graph_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(Node)
};

// helper functions for implementing GVN hash table
template< typename T >
std::uint64_t GVNHash0( T* );

template< typename T , typename V >
std::uint64_t GVNHash1( T* , const V& );

template< typename T , typename V1 , typename V2 >
std::uint64_t GVNHash2( T* , const V1& , const V2& );

template< typename T , typename V1, typename V2 , typename V3 >
std::uint64_t GVNHash3( T* , const V1& , const V2& , const V3& );

template< typename T , typename V1, typename V2, typename V3 , typename V4 >
std::uint64_t GVNHash4( T* , const V1& , const V2& , const V3& , const V4& );

} // namespace hir
} // namespace cbase
} // namespace lavascript
#endif // CBASE_HIR_NODE_H_
