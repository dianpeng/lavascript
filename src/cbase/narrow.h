#ifndef CBASE_HIR_NARROW_H_
#define CBASE_HIR_NARROW_H_
#include "fold/folder.h"
#include "hir.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

// Narrow optimization
// This optimization is happened on the fly when we generate the IR graph while
// parsing through the bytecode.
//
// This optimization narrow the floating point number value into a 32 bits integer
// value for a loop induction variable if applicable. The main reason to optimize
// this is for
// 1) help avoiding high latency floating point operations when in the loop
// 2) help avoiding high latency floating point converstion operations when induction
//    variable is used as an index to access array
// 3) enable further and deeper loop related optimization if applicable.
//
// The narrow optimization is a back propogation algorithm since we cannot know
// whether we should optimize/narrow a induction variable during the graph construction.
// The graph construction phase already knows which variables are induction variable,
// so once the induction variable is patched when loop is closed , we will start to
// run the narrow optimization then. The narrow optimization will start at the induction
// variable and backwards propogate the type specialization and narrowing optimization
// along with its use def chain until it cannot optimize out.
//
// To specialize loop induction variable, we must make sure the following condition met:
//
// 1) initial variable can be asserted as integer
// 2) condition variable can be asserted as integer
// 3) step variable can be asserted as integer
class Narrow {
 public:
};


} // namespace hir
} // namespace cbase
} // namespace lavascript


#endif // CBASE_HIR_NARROW_H_
