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

#include <src/cbase/pass/loop-induction.h>

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

	std::cerr << "Before:" << std::endl;
  std::cerr << GraphPrinter::Print(graph,opt) << std::endl;

	// do the loop induction variable typping
	LoopInduction().Perform(&graph,HIRPass::NORMAL);

	std::cerr << "After:" << std::endl;
  std::cerr << GraphPrinter::Print(graph,opt) << std::endl;

  return true;
}

} // namesapce

#define CASE(...)         ASSERT_TRUE(CheckGraph   (#__VA_ARGS__))
#define CASE_OSR(IDX,...) ASSERT_TRUE(CheckGraphOSR(#__VA_ARGS__,(IDX)))

TEST(GraphBuilder,Basic) {
  CASE(
      var sum = 0;
      for( var i = 0 ; 100 ; 1 ) {
        sum = sum + a[i+10];
      }
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
