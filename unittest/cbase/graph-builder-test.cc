#include <src/interpreter/bytecode-generate.h>
#include <src/script-builder.h>
#include <src/context.h>
#include <src/zone/zone.h>
#include <src/parser/parser.h>
#include <src/parser/ast/ast.h>
#include <src/trace.h>

#include <src/cbase/dominators.h>
#include <src/cbase/graph-builder.h>
#include <src/cbase/bytecode-analyze.h>

#include <src/cbase/optimization/gvn.h>
#include <src/cbase/optimization/dce.h>
#include <src/cbase/graph-printer.h>

#include <gtest/gtest.h>

#include <cassert>
#include <bitset>
#include <vector>

#define stringify(...) #__VA_ARGS__

namespace lavascript {
namespace cbase {
namespace hir {

using namespace ::lavascript::interpreter;
using namespace ::lavascript::parser;
using namespace ::lavascript;

namespace {

bool Compile( Context* context ,const char* source ,
    ScriptBuilder* sb , std::string* error ) {
  zone::Zone zone;
  Parser parser(source,&zone,error);
  ast::Root* result = parser.Parse();
  if(!result) {
    std::cerr<<"FAILED AT PARSE:"<<*error<<std::endl;
    return false;
  }

  // now compile the code
  if(!GenerateBytecode(context,*result,sb,error)) {
    std::cerr<<"FAILED AT COMPILE:"<<*error<<std::endl;
    return false;
  }
  return true;
}

void PrintHeap( const Graph& graph ) {
  std::cerr<<"size:"            <<graph.zone()->size()<<std::endl;
  std::cerr<<"total-bytes:"     <<graph.zone()->total_bytes()<<std::endl;
}

bool CheckGraph( const char* source ) {
  Context ctx;
  std::string error;
  ScriptBuilder sb(":test",source);
  if(!Compile(&ctx,source,&sb,&error)) {
    std::cerr<<error<<std::endl;
    return false;
  }
  Handle<Script> scp( Script::New(ctx.gc(),&ctx,sb) );

  DumpWriter dw;
  sb.Dump(&dw);

  TypeTrace tt;
  GraphBuilder gb(scp,tt);
  Graph graph;

  if(!gb.Build(scp->main(),&graph)) {
    std::cerr<<"cannot build graph"<<std::endl;
    return false;
  }

  std::cerr << GraphPrinter::Print(graph) << std::endl;
  PrintHeap(graph);

  {
    GVN gvn;
    gvn.Perform(&graph,HIRPass::NORMAL);
  }

  {
    DCE dce;
    dce.Perform(&graph,HIRPass::NORMAL);
  }

  std::cerr << GraphPrinter::Print(graph) << std::endl;
  PrintHeap(graph);

  // generate dominator graph information
  Dominators dom;
  dom.Build(graph);

  std::cerr<< dom.PrintToDotFormat() << std::endl;

  return true;
}

bool CheckGraphOSR( const char* source , std::size_t offset ) {
  Context ctx;
  std::string error;
  ScriptBuilder sb(":test",source);
  if(!Compile(&ctx,source,&sb,&error)) {
    std::cerr<<error<<std::endl;
    return false;
  }
  Handle<Script> scp( Script::New(ctx.gc(),&ctx,sb) );

  DumpWriter dw;
  sb.Dump(&dw);

  TypeTrace tt;
  GraphBuilder gb(scp,tt);
  Graph graph;

  if(!gb.BuildOSR(scp->main(), scp->main()->code_buffer() + offset ,
                               &graph)) {
    std::cerr<<"cannot build graph"<<std::endl;
    return false;
  }

  std::cerr << GraphPrinter::Print(graph) << std::endl;
  PrintHeap(graph);
  return true;
}

} // namespace

#define CASE(...)         ASSERT_TRUE(CheckGraph(#__VA_ARGS__))
#define CASE_OSR(IDX,...) ASSERT_TRUE(CheckGraphOSR(#__VA_ARGS__,(IDX)))

TEST(GraphBuilder,Basic) {
  CASE(
      var a = [1,2,3,4,5];
      g.x = 20;
      a[1] = 20;
      g.c = 30;
      a[2] = 30;
      return a[1] + a[2];
    );
}

} // namespace hir
} // namespace cbase
} // namespace lavascript

int main( int argc, char* argv[] ) {
  ::lavascript::InitTrace("-");
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}
