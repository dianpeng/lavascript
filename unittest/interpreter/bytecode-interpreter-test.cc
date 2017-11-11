#include <src/interpreter/bytecode-generate.h>
#include "src/trace.h"
#include <src/script-builder.h>
#include <src/context.h>
#include <src/zone/zone.h>
#include <src/parser/parser.h>
#include <src/parser/ast/ast.h>
#include <src/trace.h>
#include <gtest/gtest.h>
#include <cassert>
#include <iostream>
#include <gtest/gtest.h>
#include <cmath>

#include "src/interpreter/bytecode-interpreter.h"

#define stringify(...) #__VA_ARGS__

using namespace lavascript;
using namespace lavascript::parser;
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
  if(!interpreter::GenerateBytecode(context,*result,sb,error)) {
    std::cerr<<"FAILED AT COMPILE:"<<*error<<std::endl;
    return false;
  }
  return true;
}

static const bool kShowBytecode = false;

// Helper function to test *primitive* values
bool PrimitiveEqual( const char* source , const Value& primitive ) {
  // Generate assembler everytime, this is kind of slow but nothing hurts
  std::shared_ptr<lavascript::interpreter::AssemblyInterpreter>
    interp( lavascript::interpreter::AssemblyInterpreter::Generate() );
  lava_verify(interp);
  lavascript::interpreter::AssemblyInterpreter::Instance ins(interp);

  Context ctx;
  std::string error;
  std::string script(source);
  ScriptBuilder sb("a",script);
  lava_verify(Compile(&ctx,script.c_str(),&sb,&error));

  if(kShowBytecode) {
    DumpWriter dw;
    sb.Dump(&dw);
  }

  Handle<Script> scp( Script::New(ctx.gc(),&ctx,sb) );
  Handle<Object> obj( Object::New(ctx.gc()) );
  Value ret;

  std::cout<<"-----------------------------------\n";
  bool r = ins.Run(&ctx,scp,obj,&error,&ret);
  std::cout<<"-----------------------------------\n";

  if(r) {
    if(ret.IsNull()) {
      return primitive.IsNull();
    } else if(ret.IsReal()) {
      return primitive.IsReal() ? (primitive.GetReal() == ret.GetReal()) : false;
    } else if(ret.IsInteger()) {
      return primitive.IsInteger() ? (primitive.GetInteger() == ret.GetInteger()) : false;
    } else if(ret.IsBoolean()) {
      return primitive.IsBoolean() ? (primitive.GetBoolean() == ret.GetBoolean()) : false;
    } else {
      lava_unreachF("not primitive type at all %s|%s",ret.type_name(),primitive.type_name());
      return false;
    }
  } else {
    return false;
  }
}

#define PRIMITIVE_EQUAL(VALUE,...) \
  ASSERT_TRUE(PrimitiveEqual(#__VA_ARGS__,::lavascript::Value(VALUE)))

} // namespace

namespace lavascript {
namespace interpreter {

TEST(Interpreter,ArithXV) {
  PRIMITIVE_EQUAL(10,var a = 50; return 60-a;);
  PRIMITIVE_EQUAL(30,var a = 10; return 20+a;);
  PRIMITIVE_EQUAL(200,var a= 10; return 20*a;);
  PRIMITIVE_EQUAL(5, var a= 10; return 50/a; );

  PRIMITIVE_EQUAL(20.0,var a= 10.0; return 10.0+a;);
  PRIMITIVE_EQUAL(20.0,var a= 10.0; return 30.0-a;);
  PRIMITIVE_EQUAL(30.0,var a= 10.0; return 3.0*a; );
  PRIMITIVE_EQUAL(3.0 ,var a= 10.0; return 30.0/a;);

  PRIMITIVE_EQUAL(10.0,var a=50.0; return 60-a; );
  PRIMITIVE_EQUAL(30.0,var a=10.0; return 20+a; );
  PRIMITIVE_EQUAL(200.0,var a=10.0;return 20*a; );
  PRIMITIVE_EQUAL(5.0, var a= 10.0;return 50/a; );

  PRIMITIVE_EQUAL(20.0,var a= 10; return 10.0+a;);
  PRIMITIVE_EQUAL(20.0,var a= 10; return 30.0-a;);
  PRIMITIVE_EQUAL(30.0,var a= 10; return 3.0*a; );
  PRIMITIVE_EQUAL(3.0 ,var a= 10; return 30.0/a;);

  // Modula
  PRIMITIVE_EQUAL(3,var a = 5; return 3 % a;);
}

TEST(Interpreter,ArithVX) {
  PRIMITIVE_EQUAL(0,var a= 10; return a - 10; );
  PRIMITIVE_EQUAL(30,var a= 20; return a + 10;);
  PRIMITIVE_EQUAL(20,var a= 10; return a * 2 ; );
  PRIMITIVE_EQUAL(10,var a= 20; return a / 2 ; );

  PRIMITIVE_EQUAL(10.0,var a=6.0;return a + 4.0;);
  PRIMITIVE_EQUAL(20.0,var a=24.0; return a - 4.0; );
  PRIMITIVE_EQUAL(20.0,var a=10.0; return a * 2.0; );
  PRIMITIVE_EQUAL(10.0 ,var a=20.0; return a / 2.0; );

  PRIMITIVE_EQUAL(-10.0,var a=50.0; return a-60; );
  PRIMITIVE_EQUAL(30.0,var a=10.0; return a+20; );
  PRIMITIVE_EQUAL(200.0,var a=10.0;return a*20; );
  PRIMITIVE_EQUAL(5.0, var a= 250.0;return a/50; );

  PRIMITIVE_EQUAL(20.0,var a= 10; return a+10.0;);
  PRIMITIVE_EQUAL(-20.0,var a= 10; return a-30.0;);
  PRIMITIVE_EQUAL(30.0,var a= 10; return a*3.0; );
  PRIMITIVE_EQUAL(5.0 ,var a= 10; return a/2.0;);

  PRIMITIVE_EQUAL(3,var a= 3; return a % 5;);

  PRIMITIVE_EQUAL(10.0,var a=10.0; return a-0;);
}

TEST(Interpreter,ArithPow) {
  PRIMITIVE_EQUAL(static_cast<double>(std::pow(2,4)),var a = 4; return 2 ^ a;);
  PRIMITIVE_EQUAL(static_cast<double>(std::pow(2,4)),var a = 4.0; return 2 ^ a;);
  PRIMITIVE_EQUAL(static_cast<double>(std::pow(2,4)),var a = 4; return 2.0 ^ a;);
  PRIMITIVE_EQUAL(static_cast<double>(std::pow(2,4)),var a = 4.0; return 2.0 ^ a;);

  PRIMITIVE_EQUAL(static_cast<double>(std::pow(2,4)),var a = 2; return a ^ 4;);
  PRIMITIVE_EQUAL(static_cast<double>(std::pow(2,4)),var a = 2.0; return a ^ 4;);
  PRIMITIVE_EQUAL(static_cast<double>(std::pow(2,4)),var a = 2; return a ^ 4.0;);
  PRIMITIVE_EQUAL(static_cast<double>(std::pow(2,4)),var a = 2.0; return a ^ 4.0;);
}

} // namespace lavascript
} // namespace interpreter

int main( int argc, char* argv[] ) {
  ::lavascript::InitTrace("-");
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}
