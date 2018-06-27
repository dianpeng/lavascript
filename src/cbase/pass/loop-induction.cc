#include "loop-induction.h"
#include "src/cbase/loop-analyze.h"
#include "src/cbase/hir.h"
#include "src/zone/stl.h"
#include "src/zone/zone.h"


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
  void TypeIV  ( LoopIV* );

 private:
  Graph* graph_;
  zone::Zone temp_zone_;
  zone::stl::ZoneVector<bool> visited_;
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
      TypeIV(phi->As<LoopIV>());
    }
  }
}

void LoopIVTyper::TypeIV( LoopIV* iv ) {
}

} // namespace
} // namespace hir
} // namespace cbase
} // namespace lavascript
