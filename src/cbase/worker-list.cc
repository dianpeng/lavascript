#include "worker-list.h"
#include "ir.h"

namespace lavascript {
namespace cbase {
namespace ir {

WorkerList::WorkerList( const Graph& graph ):
  existed_(graph.MaxID()),
  array_  ()
{}

bool WorkerList::Push( Node* node ) {
  if(!existed_[node->id()]) {
    array_.push_back(node);
    existed_[node->id()] = true;
    return true;
  }
  return false;
}

void WorkerList::Pop() {
  Node* top = Top();
  lava_debug(NORMAL,lava_verify(existed_[top->id()]););
  existed_[top->id()] = false;
  array_.pop_back();
}

} // namespace ir
} // namespace cbase
} // namespace lavascript
