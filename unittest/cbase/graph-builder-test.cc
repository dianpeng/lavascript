#include <src/interpreter/bytecode-generate.h>
#include <src/script-builder.h>
#include <src/context.h>
#include <src/zone/zone.h>
#include <src/parser/parser.h>
#include <src/parser/ast/ast.h>
#include <src/trace.h>

#include <src/cbase/dot-graph-visualizer.h>
#include <src/cbase/graph-builder.h>
#include <src/cbase/bytecode-analyze.h>

#include <gtest/gtest.h>

#include <cassert>
#include <bitset>
#include <vector>

#define stringify(...) #__VA_ARGS__

namespace lavascript {
namespace cbase {
namespace ir {

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

  GraphBuilder gb(scp);
  Graph graph;

  if(!gb.Build(Closure::New(ctx.gc(),scp->main()),&graph)) {
    std::cerr<<"cannot build graph"<<std::endl;
    return false;
  }

  DotGraphVisualizer dgv;
  std::cerr << dgv.Visualize(graph) << std::endl;
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

  GraphBuilder gb(scp);
  Graph graph;

  if(!gb.BuildOSR(Closure::New(ctx.gc(),scp->main()),scp->main()->code_buffer() + offset ,
                                                     &graph)) {
    std::cerr<<"cannot build graph"<<std::endl;
    return false;
  }

  DotGraphVisualizer dgv;
  std::cerr << dgv.Visualize(graph) << std::endl;
  return true;
}

} // namespace

#define CASE(...) ASSERT_TRUE(CheckGraph(#__VA_ARGS__))
#define CASE_OSR(IDX,...) ASSERT_TRUE(CheckGraphOSR(#__VA_ARGS__,(IDX)))

TEST(BytecodeAnalyze,Basic) {
  CASE_OSR(13,
      var sum = 0;
      for( var j = 0 ; xx.uu ; vv ) {
        for( var i = 0 ; fo.xx ; ba ) {
          sum = sum + 1;
        }
        sum = sum * cc + i + j;
        if(sum == 100) return sum + 200;
      }
      return sum;
  );
}

} // namespace ir
} // namespace cbase
} // namespace lavascript

int main( int argc, char* argv[] ) {
  ::lavascript::InitTrace("-");
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}
