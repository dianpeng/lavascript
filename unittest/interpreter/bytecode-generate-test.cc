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

GC::GCConfig TestGCConfig() {
  GC::GCConfig config;
  config.heap_init_capacity = 1;
  config.heap_capacity = 1;
  config.gcref_init_capacity = 1;
  config.gcref_capacity =1;
  config.sso_init_slot = 2;
  config.sso_init_capacity = 2;
  config.sso_capacity = 2;
  return config;
}

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
  Context ctx(TestGCConfig());
  std::string error;
  {
    std::string script(stringify(
          var a = 10;
          var b = a + 10;
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
