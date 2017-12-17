#include <src/interpreter/bytecode-generate.h>
#include <src/script-builder.h>
#include <src/context.h>
#include <src/zone/zone.h>
#include <src/parser/parser.h>
#include <src/parser/ast/ast.h>
#include <src/trace.h>
#include <gtest/gtest.h>
#include <cassert>

#define stringify(...) #__VA_ARGS__

namespace lavascript {
namespace interpreter{

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

} // namespace

TEST(BytecodeGenerate,Basic) {
  Context ctx;
  std::string error;
  {
    std::string script(stringify(
          var a = 10; var b = 20;
          if(true) {
            var c = 20;
            var d = 30;
          }
          if(false) {
            var a = 20;
            var e = 30;
            var d = 40;
          }

          for( var _ , k in e ) {}
          for( var a = 100 ; 1 ; 2 ) {}
          ));

    ScriptBuilder sb("a",script);
    ASSERT_TRUE(Compile(&ctx,script.c_str(),&sb,&error)) << error;
    DumpWriter dw;
    sb.Dump(&dw);
  }
}

} // namespace lavascript
} // namespace parser

int main( int argc, char* argv[] ) {
  ::lavascript::InitTrace("-");
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}
