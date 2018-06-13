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

#include "node-macro.h"
#include "node-type.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

enum IRType {
#define __(A,B,...) HIR_##B,
  CBASE_HIR_LIST(__)
#undef __ // __
  SIZE_OF_HIR
};
const char* IRTypeGetName( IRType );

// Forward declaration
class Graph;
class Node;
#define __(A,...) class A;
CBASE_HIR_LIST(__)
CBASE_HIR_INTERNAL_NODE(__)
#undef __ // __

// IRType value static mapping
template< typename T > struct HIRTypePredicate {};
template< typename T > struct HIRTypeValue     {};
#define __(A,B,...)                            \
  template<> struct HIRTypeValue<A> {          \
    static const std::size_t Value = HIR_##B;  \
  };
CBASE_HIR_LIST(__)
#undef __ // __

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

  inline const zone::String& AsZoneString () const;
  inline bool                IsLeaf       () const;
  bool                       IsNoneLeaf   () const { return !IsLeaf(); }
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
