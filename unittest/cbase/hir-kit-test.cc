#include <src/zone/zone.h>
#include <src/trace.h>
#include <src/cbase/hir.h>
#include <src/cbase/dominators.h>
#include <src/cbase/graph-printer.h>
#include <src/cbase/hir-kit.h>

#include <gtest/gtest.h>

namespace lavascript {
namespace cbase      {
namespace hir        {
namespace kit        {

// macro to invoke the kit object
#define __ kit.

TEST(HIRKit,Basic) {
  Graph graph;
  ControlFlowKit kit(&graph);

  __ DoStart();
  {
    __ DoIf   (E(&graph,10) + E::GGet(&graph,"A"));
    {
      __ DoElse();
    }
    __ DoEndIf (NULL);
    __ DoReturn(Nil::New(&graph));
  }
  __ DoEnd();

  std::cerr<< GraphPrinter::Print(graph) << std::endl;
}

} // namespace kit
} // namespace hir
} // namespace cbase
} // namespace lavascript

int main( int argc, char* argv[] ) {
  ::lavascript::InitTrace("-");
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}
