#include "loop-induction.h"
#include "src/util.h"
#include "src/cbase/loop-analyze.h"
#include "src/cbase/hir.h"
#include "src/zone/stl.h"
#include "src/zone/zone.h"

#include "src/cbase/fold/fold-arith.h"


namespace lavascript {
namespace cbase      {
namespace hir        {
namespace            {


// The pass here is just typping the loop induction variable to avoid
// dynamic dispatch overheads.
//
// The loop induction variable is not typped after the Graph Building since
// the loop induction variable forms a cycle that it cannot use persemistic
// algorithm to decide the type. But optimistically, loop induction variable
// is typped as long as its [0] and [1] operands has type. Example like this:
//
// for( var i = 0 ; i < 100 ; i = i + 1 ) {
// }
//
// Obviously i is a number/integer
//
// This pass sololy does loop induction variable typping stuff. What it does
// is that it uses LoopAnalyze to get the loop nested tree and work inside out
// by looking at the inner most loop and typped its loop iv and then its sibling
// and outer latter. The algorithm is a simple backwards propogation process.
//
// 1) Type the loop iv
// 2) Type all the use of loop iv and backwards propogate until we cannot do
//    anything.

class LoopIVTyper {
 public:
  LoopIVTyper( Graph* graph ) : graph_(graph) , temp_zone_() {}
  void Run();

 private:
  void RunInner  ( LoopAnalyze* , LoopAnalyze::LoopNode* );
  void RunLoop   ( LoopAnalyze::LoopNode* );
  void TypeLoopIV( LoopAnalyze::LoopNode* , LoopIV* );

  // get a loop induction variable's start and end
  bool GetLinearLoopIVComponent( LoopIV* , Expr** , Expr** );

  // typper for propogating type back
  bool TypeUnary     ( Unary* );
  bool TypeArithmetic( Arithmetic* );
  bool TypeCompare   ( Compare* );
  bool TypeLogical   ( Logical* );

 private: // Helper functions for back propogation of type
  void Enqueue( zone::stl::ZoneVector<bool>* ,
                zone::stl::ZoneQueue<Expr*>* , Expr* );

 private:
  Graph* graph_;
  zone::Zone temp_zone_;
};

void LoopIVTyper::Run() {
  LoopAnalyze la(&temp_zone_,*graph_);
  // iterate through all the top most loops shows up in the function
  for( auto &lp : la.parent_list() ) {
    RunInner(&la,lp);
  }
}

void LoopIVTyper::RunInner( LoopAnalyze* la , LoopAnalyze::LoopNode* node ) {
  // this node should be the start of the loop nested cluster, we use a RPO iterator
  // which guarantees us to iterte the inner most loop first and then outer one
  lava_foreach( auto n ,LoopAnalyze::LoopNodeROIterator(node,la) ) {
    RunLoop(n);
  }
}

void LoopIVTyper::RunLoop( LoopAnalyze::LoopNode* node ) {
  auto body = node->loop_body(); // this node has all the Phi nodes which has LoopIV nodes
  lava_foreach( auto phi , body->phi_list()->GetForwardIterator() ) {
    if(phi.phi()->Is<LoopIV>()) {
      TypeLoopIV(node,phi.phi()->As<LoopIV>());
    }
  }
}

// This is the simplest forms of LoopIV , we only recognize this loop induction variable
// for now, in the future we can extend it to support more types of loop induction variable
bool LoopIVTyper::GetLinearLoopIVComponent( LoopIV* node , Expr** start , Expr** end ) {
  lava_verify(node->operand_list()->size() == 2);
  *start    = node->Operand(0);
  auto incr = node->Operand(1);

  // check whether *end has one component points to *self*
  if(incr->Is<Arithmetic>()) {
    auto arith = incr->As<Arithmetic>();
    if(arith->lhs()->IsIdentical(node) || arith->rhs()->IsIdentical(node)) {
      *end = arith;
      return true;
    }
  }

  // conservatively treate it unknown for us
  return false;
}

void LoopIVTyper::Enqueue( zone::stl::ZoneVector<bool>* visited ,
                           zone::stl::ZoneQueue<Expr*>* queue   , Expr* root ) {
  // walk through the use-def chain to go all the nodes that are using this node
  lava_foreach( auto &r , root->ref_list()->GetForwardIterator() ) {
    auto e = r.node;
    if(visited->at(e->id())) {
      continue;
    }
    queue->push(e->As<Expr>());
  }
}

void LoopIVTyper::TypeLoopIV( LoopAnalyze::LoopNode* loop_node , LoopIV* iv ) {
  ValuePhi* new_iv;
  // 1. get the loop_iv component out and see whether we can do typping for it
  {
    Expr* start , *end;
    TypeKind start_type , end_type;

    if(!GetLinearLoopIVComponent(iv,&start,&end)) return;
    // try to get the type of start
    if(start_type = GetTypeInference(start); !TPKind::IsNumber(start_type))
      return;

    // now we are sure the lhs is a number at least , then move on to check
    // the rhs type by checking the component that is not the loop induction
    // variable.
    auto target = end->As<Arithmetic>()->rhs()->IsIdentical(iv) ?
                  end->As<Arithmetic>()->lhs() : end->As<Arithmetic>()->rhs();

    if(end_type = GetTypeInference(target); !TPKind::IsNumber(end_type)) {
      return;
    }

    // now decide which type should we use here, whether we should use specialized
    // LoopIVInt64 or just normal LoopIVFloat64.
    new_iv = NULL;

    if(start_type == end_type && start_type == TPKIND_INT64) {
      new_iv = LoopIVInt64::New  (graph_,start,end);
    } else {
      new_iv = LoopIVFloat64::New(graph_,start,end);
    }

    // replace the old loop induction variable from the loop body with the new iv
    loop_node->loop_body()->ReplacePhi( PhiNode{iv} , PhiNode{new_iv} );

    // the iv is replaced with the new_node
    iv->Replace(new_iv);
  }

  // 2. When we reach here we could start to do the back propogation of typping
  {
    zone::stl::ZoneVector<bool> visited(&temp_zone_);
    zone::stl::ZoneQueue <Expr*> queue (&temp_zone_);
    visited.resize(graph_->MaxID());

    // enqueue the first/root node
    visited[new_iv->id()] = true;
    Enqueue(&visited,&queue,new_iv);

    while(!queue.empty()) {
      auto top = queue.front();
      queue.pop();

      // mark it as visited
      visited[top->id()] = true;

      switch(top->type()) {
        case HIR_UNARY:
          if(TypeUnary(top->As<Unary>())) Enqueue(&visited,&queue,top);
          break;
        case HIR_ARITHMETIC:
          if(TypeArithmetic(top->As<Arithmetic>())) Enqueue(&visited,&queue,top);
          break;
        case HIR_COMPARE:
          if(TypeCompare(top->As<Compare>())) Enqueue(&visited,&queue,top);
          break;
        case HIR_LOGICAL:
          if(TypeLogical(top->As<Logical>())) Enqueue(&visited,&queue,top);
          break;
        default:
          break;
      }
    }
  }
}

bool LoopIVTyper::TypeUnary( Unary* node ) {
  auto opr = node->operand();
  // 1. try to fold it at first
  if(auto nnode = FoldUnary(graph_,node->op(),opr); nnode) {
    node->Replace(nnode);
    return true;
  }

  // 2. try to specialize the type
  if(auto t = GetTypeInference(opr); TPKind::IsNumber(t)) {
    if(t == TPKIND_FLOAT64 && node->op() == Unary::MINUS) {
      auto new_opr  = NewUnboxNode(graph_,opr,TPKIND_FLOAT64);
      auto new_node = NewBoxNode<Float64Negate>(graph_,TPKIND_FLOAT64,new_opr);
      node->Replace(new_node);
      return true;
    } else if(t == TPKIND_INT64 && node->op() == Unary::MINUS) {
      // cast int64 to float64 in an unboxed version
      auto to_f64   = Int64ToFloat64::New(graph_,NewUnboxNode(graph_,opr,TPKIND_INT64));
      auto new_node = NewBoxNode<Float64Negate>(graph_,TPKIND_FLOAT64,to_f64);
      node->Replace(new_node);
      return true;
    }
  }
  return false;
}

bool LoopIVTyper::TypeArithmetic( Arithmetic* node ) {
  // 1. try to fold the binary at first
  if(auto nnode = FoldBinary(graph_,node->op(),node->lhs(),node->rhs()); nnode) {
    node->Replace(nnode);
    return true;
  }

  // 2. try to specialize the type at least
  TypeKind lhs_type;
  TypeKind rhs_type;

  if(lhs_type = GetTypeInference(node->lhs()); !TPKind::IsNumber(lhs_type)) {
    return false;
  }
  if(rhs_type = GetTypeInference(node->rhs()); !TPKind::IsNumber(rhs_type)) {
    return false;
  }

  // now we know both lhs/rhs are number at least
  if(lhs_type == rhs_type && lhs_type == TPKIND_FLOAT64) {
    auto lnode = NewUnboxNode(graph_,node->lhs(),TPKIND_FLOAT64);
    auto rnode = NewUnboxNode(graph_,node->rhs(),TPKIND_FLOAT64);
    auto nnode = NewBoxNode<Float64Arithmetic>(graph_,TPKIND_FLOAT64,lnode,rnode,node->op());
    node->Replace(nnode);
    return true;
  } else if(lhs_type == rhs_type && lhs_type == TPKIND_INT64) {
    auto lnode = NewUnboxNode(graph_,node->lhs(),TPKIND_INT64);
    auto rnode = NewUnboxNode(graph_,node->rhs(),TPKIND_INT64);
    auto nnode = NewBoxNode<Int64Arithmetic> (graph_,TPKIND_INT64,lnode,rnode,node->op());
    node->Replace(nnode);
    return true;
  } else {
    auto lnode = node->lhs();
    auto rnode = node->rhs();

    if(lhs_type == TPKIND_INT64) {
      auto raw = NewUnboxNode(graph_,lnode,TPKIND_INT64);
      auto f64 = Int64ToFloat64::New(graph_,raw);
      lnode    = f64; // unboxed
      rnode    = NewUnboxNode(graph_,rnode,TPKIND_FLOAT64);
    } else {
      auto raw = NewUnboxNode(graph_,rnode,TPKIND_INT64);
      auto f64 = Int64ToFloat64::New(graph_,raw);
      rnode    = f64; // unboxed
      lnode    = NewUnboxNode(graph_,lnode,TPKIND_FLOAT64);
    }

    auto nnode = NewBoxNode<Float64Arithmetic>(graph_,TPKIND_INT64,lnode,rnode,node->op());
    node->Replace(nnode);
    return true;
  }

  return false;
}

bool LoopIVTyper::TypeCompare( Compare* node ) {
  // 1. try to fold the comparison directly it may not work obviously
  if(auto nnode = FoldBinary(graph_,node->op(),node->lhs(),node->rhs());nnode) {
    node->Replace(nnode);
    return true;
  }

  // 2. try to specialize the type at least
  TypeKind lhs_type;
  TypeKind rhs_type;

  // the possible specialization we can take is float64_compare and string_compare

  if(lhs_type = GetTypeInference(node->lhs()); !TPKind::IsNumber(lhs_type)) {
    return false;
  }

  if(rhs_type = GetTypeInference(node->rhs()); !TPKind::IsNumber(rhs_type)) {
    return false;
  }

  // both are number types
  if(lhs_type == TPKIND_FLOAT64 && rhs_type == TPKIND_FLOAT64) {
    // float64 both
    auto lnode = NewUnboxNode(graph_,node->lhs(),TPKIND_FLOAT64);
    auto rnode = NewUnboxNode(graph_,node->rhs(),TPKIND_FLOAT64);
    auto nnode = NewBoxNode<Float64Compare>(graph_,TPKIND_FLOAT64,lnode,rnode,node->op());
    node->Replace(nnode);
    return true;
  } else if(lhs_type == TPKIND_INT64 && rhs_type == TPKIND_INT64) {
    // int64 both
    auto lnode = NewUnboxNode(graph_,node->lhs(),TPKIND_INT64);
    auto rnode = NewUnboxNode(graph_,node->rhs(),TPKIND_INT64);
    auto nnode = NewBoxNode<Int64Compare>(graph_,TPKIND_INT64,lnode,rnode,node->op());
    node->Replace(nnode);
    return true;
  } else {
    auto lnode = node->lhs();
    auto rnode = node->rhs();
    if(lhs_type == TPKIND_INT64) {
      auto raw = NewUnboxNode(graph_,lnode,TPKIND_INT64);
      auto f64 = Int64ToFloat64::New(graph_,raw);
      lnode    = f64;
      rnode    = NewUnboxNode(graph_,rnode,TPKIND_FLOAT64);
    } else {
      auto raw = NewUnboxNode(graph_,rnode,TPKIND_INT64);
      auto f64 = Int64ToFloat64::New(graph_,raw);
      rnode    = f64;
      lnode    = NewUnboxNode(graph_,lnode,TPKIND_FLOAT64);
    }

    auto nnode = NewBoxNode<Float64Compare>(graph_,TPKIND_FLOAT64,lnode,rnode,node->op());
    node->Replace(nnode);
    return true;
  }

  return false;
}

bool LoopIVTyper::TypeLogical( Logical* node ) {
  if(auto nnode = FoldBinary(graph_,node->op(),node->lhs(),node->rhs()); nnode) {
    node->Replace(nnode);
    return true;
  }
  return false;
}

} // namespace

bool LoopInduction::Perform( Graph* graph , Flag flag ) {
  (void)flag;

  // do the loop induction variable typping and backwards propogation
  LoopIVTyper typer(graph);
  typer.Run();
  return true;
}

} // namespace hir
} // namespace cbase
} // namespace lavascript
