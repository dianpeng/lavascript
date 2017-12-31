#include <src/interpreter/bytecode-generate.h>
#include <src/script-builder.h>
#include <src/context.h>
#include <src/zone/zone.h>
#include <src/parser/parser.h>
#include <src/parser/ast/ast.h>
#include <src/trace.h>
#include <src/cbase/bytecode-analyze.h>

#include <gtest/gtest.h>
#include <cassert>

#define stringify(...) #__VA_ARGS__

namespace lavascript {
namespace cbase {

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

bool DumpBytecodeAnalyze( const char* source ) {
  Context ctx;
  std::string error;

  ScriptBuilder sb(":test",source);
  if(!Compile(&ctx,source,&sb,&error)) {
    std::cerr<<error<<std::endl;
    return false;
  }
  Handle<Script> scp( Script::New(ctx.gc(),&ctx,sb) );

  DumpWriter dw; sb.Dump(&dw);

  BytecodeAnalyze ba(scp->main()); ba.Dump(&dw);
  return true;
}

} // namespace

#define CASE(...) ASSERT_TRUE(DumpBytecodeAnalyze(#__VA_ARGS__))

TEST(BytecodeGenerate,Basic) {
  CASE(
      var a = 10;
      var b = 20;
      if(a*b) {
        var c = 20;
        var d = 30;
        var e = 40;
      } else {
        var f = 10;
      }
      return a + b;
      );
}

} // namespace cbase
} // namespace lavascript

int main( int argc, char* argv[] ) {
  ::lavascript::InitTrace("-");
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}
