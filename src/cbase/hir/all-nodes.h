#ifndef CBASE_HIR_ALL_NODES_H_
#define CBASE_HIR_ALL_NODES_H_
// base
#include "node.h"
// expression node
#include "expr.h"
#include "arith.h"
#include "cast.h"
#include "const.h"
#include "itr.h"
#include "node.h"
#include "upvalue.h"
#include "box.h"
#include "checkpoint.h"
#include "global.h"
#include "memory.h"
#include "phi.h"
#include "call.h"
#include "cls.h"
#include "effect.h"
#include "guard.h"
#include "prop.h"
// control flow node
#include "control-flow.h"
#include "region.h"
#include "loop.h"
#include "branch.h"
#include "jump.h"
#include "trap.h"

#include <map>
#include <vector>
#include <deque>
#include <stack>
#include <functional>

namespace lavascript {
namespace cbase      {
namespace hir        {

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
// Helper
// -------------------------------------------------------------------------
inline Expr* NewString           ( Graph* , const void* , std::size_t length );
inline Expr* NewString           ( Graph* , const char* );
inline Expr* NewString           ( Graph* , const zone::String* );
inline Expr* NewStringFromBoolean( Graph* , bool );
inline Expr* NewStringFromReal   ( Graph* , double );

// ---------------------------------------------------------------------
// Helper functions for creation of node
// ---------------------------------------------------------------------
template< typename T , typename ...ARGS >
inline Box* NewBoxNode( Graph* graph , TypeKind tk , ARGS ...args ) {
  auto n = T::New(graph,args...);
  return Box::New(graph,n,tk);
}

// Create a unbox value from a node that has type inference.
Expr* NewUnboxNode( Graph* , Expr* node , TypeKind tk );

} // namespace hir
} // namespace cbase
} // namespace lavascript

#include "all-nodes-inl.h"

#endif // CBASE_HIR_ALL_NODES_H_
