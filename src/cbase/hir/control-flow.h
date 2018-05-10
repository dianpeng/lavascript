#ifndef CBASE_HIR_CONTROL_FLOW_H_
#define CBASE_HIR_CONTROL_FLOW_H_
#include "node.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

// Control Flow
//
//  The control flow node needs to support one additional important
//  feature , mutation/modification/deletion of existed control flow
//  graph.
class ControlFlow : public Node {
 public:
  // Parental node
  ControlFlow* parent() const {
    lava_debug(NORMAL,lava_verify(backward_edge()->size() == 1););
    return backward_edge()->First();
  }

  // Backward(Successor) ---------------------------------------------------
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

  // shortcuts for backward_edge
  ControlFlow* In( std::size_t index ) const { return backward_edge()->Index(index); }

  // check if a region is a merge region or not. A merge region is a region
  // that points back to at least 2 region. Basically sizeof backward_edge
  // list must be larger or equal than 2
  bool IsMergeRegion() const { return backward_edge()->size() >= 2; }

  // Forward(Predecessor) --------------------------------------------------
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

  // shortcuts for forward_edge
  ControlFlow* Out( std::size_t index ) const { return forward_edge()->Index(index); }

  // Reference List ---------------------------------------------------------
  const RegionRefList* ref_list() const {
    return &ref_list_;
  }
  // Add the referece into the reference list
  void AddRef( ControlFlow* who_uses_me , const RegionListIterator& iter ) {
    ref_list_.PushBack(zone(),RegionRef(iter,who_uses_me));
  }

  // Stmt --------------------------------------------------------------------
  // A list that is used to record those operations that cannot be categorized
  // as input/data dependency. Things like side effect operation, function call,
  // property set and index set , even checkpoint
  const StmtList* stmt_list() const {
    return &stmt_expr_;
  }
  void AddStmt( Expr* node ) {
    auto itr = stmt_expr_.PushBack(zone(),node);
    node->set_stmt_edge(StmtEdge(this,itr));
  }
  void RemoveStmt( const StmtEdge& ee ) {
    lava_debug(NORMAL,lava_verify(ee.region == this););
    stmt_expr_.Remove(ee.iterator);
  }
  void MoveStmt( ControlFlow* );

  // OperandList -------------------------------------------------------------
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
  bool RemoveOperand( Expr* node );

  // Clear all the operand from this control flow node
  void ClearOperand ();

  // Replace *this* with the input node. Internally this function only
  // modifies the input and output edge for the input node. For operand
  // list, the input node's operand list will be used.
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
  RegionList           backward_edge_;
  RegionList           forward_edge_;
  RegionRefList        ref_list_;
  StmtList             stmt_expr_;
  OperandList          operand_list_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(ControlFlow)
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_CONTROL_FLOW_H_
