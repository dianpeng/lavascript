#ifndef CBASE_WORKER_LIST_H_
#define CBASE_WORKER_LIST_H_
#include "src/stl-helper.h"
#include "src/trace.h"

namespace lavascript {
namespace cbase {
namespace ir {
class Graph;
class Node;

class WorkerList {
 public:
  explicit WorkerList( const Graph& );
  bool Push( Node* node );
  void Pop();
  Node* Top() const {
    return array_.back();
  }
  bool empty() const { return array_.empty(); }
 private:
  DynamicBitSet existed_;
  std::vector<Node*> array_ ;
};

} // namespace ir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_WORKER_LIST_H_
