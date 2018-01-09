#ifndef CBASE_PASS_H_
#define CBASE_PASS_H_

#include <string>

namespace lavascript {
namespace cbase {
namespace hir {
class Graph;
} // namespace hir

// High level IR optimization/analyze pass. This pass will take a hir::Graph
// object in and mutate/modify it and return it back.
class HIRPass {
 public:
  enum Flag {
    NORMAL,
    DEBUG
  };

  /**
   * API to perform the optimization pass for this high level IR graph
   */
  virtual bool Perform( hir::Graph* , Flag ) = 0;

  // name of the pass, it is also used to dynamically configure the pass
  // needed for a specific compilation
  const std::string& name() const { return name_; }


  virtual ~HIRPass() {}

 private:
  std::string name_; // name of this pass
};

} // namespace cbase
} // namespace lavascript

#endif // CBASE_PASS_H_
