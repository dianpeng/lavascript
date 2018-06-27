#include "loop-induction.h"
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
  void Run();

 private:
  void RunInner( const LoopAnalyze* , LoopAnalyze::LoopNode* );
  void RunLoop ( LoopAnalyze::LoopNode* );
  void TypeLoopIV  ( LoopIV* );

  // get a loop induction variable's start and end
  bool GetLinearLoopIVComponent( const LoopIV& node , Expr** start ,
                                                      Expr** end  );
 private:
  Graph* graph_;
  zone::Zone temp_zone_;
};


void LoopIVTyper::Run() {
  LoopAnalyze la(&temp_zone_,*graph_);
  // iterate through all the top most loops shows up in the function
  for( auto &lp : la.parent_list() ) {
    RunInner(lp);
  }
}

void LoopIVTyper::RunInner( LoopAnalyze* la , LoopAnalyze::LoopNode* node ) {
  // this node should be the start of the loop nested cluster, we use a RPO iterator
  // which guarantees us to iterte the inner most loop first and then outer one
  lava_foreach( auto &n ,LoopAnalyze::LoopNodeROIterator(node,la) ) {
    RunLoop(n);
  }
}

void LoopIVTyper::RunLoop( LoopAnalyze::LoopNode* node ) {
  auto body = node->loop_body(); // this node has all the Phi nodes which has LoopIV nodes
  lava_foreach( auto &phi , body->phi_list() ) {
    if(phi->Is<LoopIV>()) {
      TypeLoopIV(phi->As<LoopIV>());
    }
  }
}

// This is the simplest forms of LoopIV , we only recognize this loop induction variable
// for now, in the future we can extend it to support more types of loop induction variable
bool LoopIVTyper::GetLinearLoopIVComponent( const LoopIV* node , Expr** start ,
                                                                 Expr** end ) {
  lava_verify(node->operand_list()->size() == 2);
  *start    = node->Out(0);
  auto incr = node->Out(1);

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

void LoopIVTyper::TypeLoopIV( LoopIV* iv ) {
  // 1. get the loop_iv component out and see whether we can do typping for it
  {
    Expr* start , *end;
    TypeKind start_type , end_type;

    if(!GetLinearLoopIVComponent(iv,&start,&end)) return;
    // try to get the type of start
    if(start_type = GetTypeInference(start); start_type != TPKIND_FLOAT64 &&
                                             start_type != TPKIND_INT64)
      return false;

    // now we are sure the lhs is a number at least , then move on to check
    // the rhs type by checking the component that is not the loop induction
    // variable.
    auto target = end->As<Arithmetic>()->rhs()->IsIdentical(iv) ?
                  end->As<Arithmetic>()->lhs() : end->As<Arithmetic>()->rhs();

    if(end_type = GetTypeInference(target); end_type != TPKIND_FLOAT64 &&
                                            end_type != TPKIND_INT64) {
      return false;
    }

    // now decide which type should we use here, whether we should use specialized
    // LoopIVInt64 or just normal LoopIVFloat64.
    LoopIV* new_node = NULL;

    if(start_type == end_type && start_type == TPKIND_INT64) {
      new_node = LoopIVInt64::New(graph_,start,end);
    } else {
      new_node = LoopIVIntFloat64::New(graph_,start,end);
    }

    // the iv is replaced with the new_node
    iv->Replace(new_node);
    iv = new_node;
  }

  // 2. When we reach here we could start to do the back propogation of typping
  {
    zone::stl::ZoneVector<bool> visited(&temp_zone_);
    zone::stl::ZoneQueue <Expr*> queue (&temp_zone_);
    visited.resize(graph_->MaxID());
    queue.push_back(iv);

    while(!queue.empty()) {
      auto top = queue.front();
      queue.pop_front();
      if(visited[top->id()])
        continue;
      // mark it as visited
      visited[top->id()] = true;

      switch(top->type()) {
        case HIR_UNARY:
          if(TypeUnary(top->As<Unary>()))
            queue.push_back(top);
          break;
        case HIR_ARITHMETIC:
          if(TypeArithmetic(top->As<Arithmetic>()))
            queue.push_back(top);
          break;
        case HIR_COMPARE:
          if(TypeCompare(top->As<Compare>()))
            queue.push_back(top);
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
  if(auto t = GetTypeInference(opr); t == TPKIND_FLOAT64 || t == TPKIND_INT64 ) {
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
}

} // namespace
} // namespace hir
} // namespace cbase
} // namespace lavascript
