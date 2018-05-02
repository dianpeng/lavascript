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
  __(LoadCls      ,LOAD_CLS ,"load_cls" ,Leaf,Effect)                  \
  /* argument node */                                                  \
  __(Arg          ,ARG      ,"arg"      ,Leaf,NoEffect)                \
  /* arithmetic/comparison node */                                     \
  __(Unary        ,UNARY    ,"unary"    ,NoLeaf,NoEffect)              \
  __(Binary       ,BINARY   ,"binary"   ,NoLeaf,Effect)                \
  __(Ternary      ,TERNARY  ,"ternary"  ,NoLeaf,NoEffect)              \
  /* upvalue */                                                        \
  __(UGet         ,UGET     ,"uval"     ,Leaf  ,NoEffect)              \
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
  __(Phi           ,PHI             ,"phi"             ,NoLeaf,NoEffect) \
  __(WriteEffectPhi,WRITE_EFFECT_PHI,"write_effect_phi",NoLeaf,NoEffect) \
  __(NoReadEffect  ,NO_READ_EFFECT  ,"no_read_effect"  ,Leaf  ,NoEffect) \
  __(NoWriteEffect ,NO_WRITE_EFFECT ,"no_write_effect" ,Leaf  ,NoEffect) \
  /* statement */                                                      \
  __(InitCls      ,INIT_CLS        ,"init_cls"  ,NoLeaf,Effect)        \
  __(Projection   ,PROJECTION      ,"projection",NoLeaf,NoEffect)      \
  /* osr */                                                            \
  __(OSRLoad      ,OSR_LOAD        ,"osr_load"  ,Leaf,Effect)          \
  /* checkpoints */                                                    \
  __(Checkpoint   ,CHECKPOINT      ,"checkpoint",NoLeaf,Effect)        \
  __(StackSlot    ,STACK_SLOT      ,"stackslot" ,NoLeaf,NoEffect)

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
  __(ObjectGet    ,OBJECT_GET    ,"object_get"   ,NoLeaf,NoEffect)    \
  __(ObjectSet    ,OBJECT_SET    ,"object_set"   ,NoLeaf,Effect)      \
  __(ListGet      ,LIST_GET      ,"list_get"     ,NoLeaf,NoEffect)    \
  __(ListSet      ,LIST_SET      ,"list_set"     ,NoLeaf,Effect)

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
  __(TestType    ,TEST_TYPE      ,"test_type"      ,NoLeaf,NoEffect)  \
  __(TestListOOB ,TEST_LISTOOB   ,"test_listobb"   ,NoLeaf,NoEffect)

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
  __(CastToBoolean,CAST_TO_BOOLEAN,"cast_to_boolean",NoLeaf,NoEffect)

// All the expression IR nodes
#define CBASE_HIR_EXPRESSION(__)                                      \
  CBASE_HIR_CONSTANT(__)                                              \
  CBASE_HIR_EXPRESSION_HIGH(__)                                       \
  CBASE_HIR_EXPRESSION_LOW (__)                                       \
  CBASE_HIR_TEST(__)                                                  \
  CBASE_HIR_BOXOP(__)                                                 \
  CBASE_HIR_CAST (__)                                                 \
  CBASE_HIR_GUARD(__)

// All the control flow IR nodes
#define CBASE_HIR_CONTROL_FLOW(__)                                    \
  __(Start      ,START      ,"start"      ,NoLeaf,NoEffect)           \
  __(End        ,END        ,"end"        ,NoLeaf,NoEffect)           \
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
  __(Region     ,REGION     ,"region"     ,NoLeaf,NoEffect)           \
  __(Trap       ,TRAP       ,"trap"       ,NoLeaf,NoEffect)           \
  /* osr */                                                           \
  __(OSRStart   ,OSR_START  ,"osr_start"  ,NoLeaf,NoEffect)           \
  __(OSREnd     ,OSR_END    ,"osr_end"    ,NoLeaf,NoEffect)

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

class Graph;
class Node;
class Test;
class ReadEffect;
class WriteEffect;
class MemoryOp;
class MemoryWrite;
class MemoryRead ;
class MemoryNode;
class Expr;
class ControlFlow;

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
// Pin list
//
// Bunch of statements that are not used by any expression but have observable
// effects. Example like : foo() , a free function call
typedef zone::List<Expr*> PinList;
typedef PinList::ForwardIterator PinIterator;

// This structure is held by *all* the expression. If the region field is not
// NULL then it means this expression has side effect and it is bounded at
// certain control flow region
struct PinEdge {
  ControlFlow* region;
  PinIterator iterator;
  bool HasRef() const { return region != NULL; }

  PinEdge( ControlFlow* r , const PinIterator& itr ): region(r), iterator(itr) {}
  PinEdge(): region(NULL), iterator() {}
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
  inline bool                IsMemoryWrite() const;
  inline bool                IsMemoryRead () const;
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
  inline MemoryWrite*        AsMemoryWrite();
  inline const MemoryWrite*  AsMemoryWrite() const;
  inline MemoryRead *        AsMemoryRead ();
  inline const MemoryRead*   AsMemoryRead () const;
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

} // namespace hir
} // namespace cbase
} // namespace lavascript
#endif // CBASE_HIR_NODE_H_
