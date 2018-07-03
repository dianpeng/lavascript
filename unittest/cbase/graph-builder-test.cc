#include <src/interpreter/bytecode-generate.h>
#include <src/script-builder.h>
#include <src/context.h>
#include <src/zone/zone.h>
#include <src/parser/parser.h>
#include <src/parser/ast/ast.h>
#include <src/trace.h>
#include <src/runtime-trace.h>

#include <src/cbase/hir.h>
#include <src/cbase/dominators.h>
#include <src/cbase/graph-builder.h>
#include <src/cbase/bytecode-analyze.h>
#include <src/cbase/loop-analyze.h>

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

  RuntimeTrace tt;
  Graph graph;

  if(!BuildPrototype(scp,scp->main(),tt,&graph)) {
    std::cerr<<"cannot build graph"<<std::endl;
    return false;
  }

  auto opt = GraphPrinter::Option( GraphPrinter::Option::ALL_CHAIN , false );
  std::cerr << GraphPrinter::Print(graph,opt) << std::endl;
  PrintHeap(graph);

  // generate dominator graph information
  {
    zone::StackZone<10240> stack_zone;
    Dominators dom(&stack_zone,graph);
    std::cerr<< dom.PrintToDotFormat() << std::endl;
  }

  // generate loop analyze information
  {
    zone::StackZone<10240> stack_zone;
    LoopAnalyze la(&stack_zone,graph);
    la.Dump(&dw);
  }
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
  RuntimeTrace tt;
  Graph graph;
  if(!BuildPrototypeOSR(scp,scp->main(),tt,scp->main()->code_buffer() + offset,&graph)) {
    std::cerr<<"cannot build graph"<<std::endl;
    return false;
  }
  std::cerr << GraphPrinter::Print(graph,GraphPrinter::Option(
        GraphPrinter::Option::EFFECT_CHAIN,false)) << std::endl;
  PrintHeap(graph);
  return true;
}

} // namespace

#define CASE(...)         ASSERT_TRUE(CheckGraph   (#__VA_ARGS__))
#define CASE_OSR(IDX,...) ASSERT_TRUE(CheckGraphOSR(#__VA_ARGS__,(IDX)))

TEST(GraphBuilder,Basic) {
  CASE(
      var sum = 0;
      for(var i = 0 ; 1 ; 1 ) { sum = sum + a[i+10]; }
      return sum;
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
