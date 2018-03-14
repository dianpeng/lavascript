#include "ranger.h"
#include "src/cbase/dominators.h"
#include "src/cbase/value-range.h"

namespace lavascript {
namespace cbase      {
namespace hir        {
namespace {

/**
 * The algorithm visit each *conditional branch* node and setup the constraint
 * table accordingly. Whenver a branch node is visited, it will do following
 * things:
 *
 * 1) translate the condition in the conditional branch into a condition group
 * 2) merge its immediate dominator's condition group if have one
 *
 * This a monotonic algorithm , since all the nested group is a and operation
 * which is monotonic.
 *
 * We will do a graph traversal using RPO which help us to make algorithm have
 * faster convergence.
 */

class Impl {
 public:

};

} // namespace


} // namespace hir
} // namespace cbase
} // namespace lavascript
