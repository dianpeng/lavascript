#include <src/interpreter/bytecode-generate.h>
#include <src/script-builder.h>
#include <src/context.h>
#include <src/zone/zone.h>
#include <src/parser/parser.h>
#include <src/parser/ast/ast.h>
#include <src/trace.h>
#include <src/os.h>

#include <gtest/gtest.h>
#include <cassert>
#include <iostream>
#include <gtest/gtest.h>
#include <cmath>

#include "src/trace.h"
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

static const bool kShowBytecode = true;

enum { COMP_LE , COMP_LT , COMP_GT , COMP_GE , COMP_EQ , COMP_NE };

template < typename T > bool PrimitiveComp( const T& a1 , const T& a2 , int op ) {
  switch(op) {
    case COMP_LE : return a1 <= a2;
    case COMP_LT : return a1 <  a2;
    case COMP_GT : return a1 >  a2;
    case COMP_GE : return a1 >= a2;
    case COMP_EQ : return a1 == a2;
    case COMP_NE : return a1 != a2;
    default: lava_unreach(""); return false;
  }
}

bool Bench( const char* source ) {
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
  bool r;

  {
    static const std::size_t kTimes = 100;
    std::uint64_t start = ::lavascript::OS::NowInMicroSeconds();
    for ( std::size_t i = 0 ; i < kTimes ; ++i ) {
      r = ins.Run(&ctx,scp,obj,&error,&ret);
    }
    std::uint64_t end   = ::lavascript::OS::NowInMicroSeconds();
    if(r) {
      std::cerr<<"Benchmark result:"<<(end-start)/kTimes<<'\n';
    } else {
      std::cerr<<"Failed!"<<std::endl;
    }
  }
  if(ret.IsReal()) {
    std::cerr<<"Real:"<<static_cast<int>(ret.GetReal())<<std::endl;
  } else if(ret.IsBoolean()) {
    std::cerr<<"Bool:"<<ret.GetBoolean()<<std::endl;
  } else {
    std::cerr<<"Type:"<<ret.type_name()<<std::endl;
  }
  return r;
}

bool PrimitiveComp( const char* source , const Value& primitive , int op ) {
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
      switch(op) {
        case COMP_EQ: return primitive.IsNull();
        case COMP_NE: return !primitive.IsNull();
        default: lava_unreach("null comparison!"); return false; // You cannot compare NULL
      }
    } else if(ret.IsReal()) {
      return primitive.IsReal() ? (PrimitiveComp(ret.GetReal(),primitive.GetReal(),op)) : false;
    } else if(ret.IsBoolean()) {
      switch(op) {
        case COMP_EQ: return primitive.IsBoolean() && (primitive.GetBoolean() == ret.GetBoolean());
        case COMP_NE: return primitive.IsBoolean() && (primitive.GetBoolean() == ret.GetBoolean());
        default: lava_unreach("boolean cannot be compared!"); return false; // You cannot compare boolean
      }
    } else if(ret.IsString()) {
      return primitive.IsString() ? (*ret.GetString() == *primitive.GetString()) : false;
    } else {
      lava_unreachF("not primitive type at all %s|%s",ret.type_name(),primitive.type_name());
      return false;
    }
  } else {
    return false;
  }
}

#define PRIMITIVE_EQ(VALUE,...) \
  ASSERT_TRUE(PrimitiveComp(#__VA_ARGS__,::lavascript::Value(VALUE),COMP_EQ))

#define PRIMITIVE_NE(VALUE,...) \
  ASSERT_TRUE(PrimitiveComp(#__VA_ARGS__,::lavascript::Value(VALUE),COMP_NE))

#define BENCHMARK(...) \
  ASSERT_TRUE(Bench(#__VA_ARGS__))


} // namespace

namespace lavascript {
namespace interpreter {

TEST(Interpreter,Load) {
  PRIMITIVE_EQ(0,return 0;);
  PRIMITIVE_EQ(-1,return -1;);
  PRIMITIVE_EQ(1,return 1;);
}


TEST(Interpreter,ArithXV) {
  PRIMITIVE_EQ(10,var a = 50; return 60-a;);
  PRIMITIVE_EQ(30,var a = 10; return 20+a;);
  PRIMITIVE_EQ(200,var a= 10; return 20*a;);
  PRIMITIVE_EQ(5, var a= 10; return 50/a; );

  PRIMITIVE_EQ(20.0,var a= 10.0; return 10.0+a;);
  PRIMITIVE_EQ(20.0,var a= 10.0; return 30.0-a;);
  PRIMITIVE_EQ(30.0,var a= 10.0; return 3.0*a; );
  PRIMITIVE_EQ(3.0 ,var a= 10.0; return 30.0/a;);

  PRIMITIVE_EQ(10.0,var a=50.0; return 60-a; );
  PRIMITIVE_EQ(30.0,var a=10.0; return 20+a; );
  PRIMITIVE_EQ(200.0,var a=10.0;return 20*a; );
  PRIMITIVE_EQ(5.0, var a= 10.0;return 50/a; );

  PRIMITIVE_EQ(20.0,var a= 10; return 10.0+a;);
  PRIMITIVE_EQ(20.0,var a= 10; return 30.0-a;);
  PRIMITIVE_EQ(30.0,var a= 10; return 3.0*a; );
  PRIMITIVE_EQ(3.0 ,var a= 10; return 30.0/a;);

  // Modula
  PRIMITIVE_EQ(3,var a = 5; return 3 % a;);
}

TEST(Interpreter,ArithVX) {
  PRIMITIVE_EQ(0,var a= 10; return a - 10; );
  PRIMITIVE_EQ(30,var a= 20; return a + 10;);
  PRIMITIVE_EQ(20,var a= 10; return a * 2 ; );
  PRIMITIVE_EQ(10,var a= 20; return a / 2 ; );

  PRIMITIVE_EQ(10.0,var a=6.0;return a + 4.0;);
  PRIMITIVE_EQ(20.0,var a=24.0; return a - 4.0; );
  PRIMITIVE_EQ(20.0,var a=10.0; return a * 2.0; );
  PRIMITIVE_EQ(10.0 ,var a=20.0; return a / 2.0; );

  PRIMITIVE_EQ(-10.0,var a=50.0; return a-60; );
  PRIMITIVE_EQ(30.0,var a=10.0; return a+20; );
  PRIMITIVE_EQ(200.0,var a=10.0;return a*20; );
  PRIMITIVE_EQ(5.0, var a= 250.0;return a/50; );

  PRIMITIVE_EQ(20.0,var a= 10; return a+10.0;);
  PRIMITIVE_EQ(-20.0,var a= 10; return a-30.0;);
  PRIMITIVE_EQ(30.0,var a= 10; return a*3.0; );
  PRIMITIVE_EQ(5.0 ,var a= 10; return a/2.0;);

  PRIMITIVE_EQ(3,var a= 3; return a % 5;);

  PRIMITIVE_EQ(10.0,var a=10.0; return a-0;);
}

TEST(Interpreter,ArithPow) {
  PRIMITIVE_EQ(static_cast<double>(std::pow(2,4)),var a = 4; return 2 ^ a;);
  PRIMITIVE_EQ(static_cast<double>(std::pow(2,4)),var a = 4.0; return 2 ^ a;);
  PRIMITIVE_EQ(static_cast<double>(std::pow(2,4)),var a = 4; return 2.0 ^ a;);
  PRIMITIVE_EQ(static_cast<double>(std::pow(2,4)),var a = 4.0; return 2.0 ^ a;);

  PRIMITIVE_EQ(static_cast<double>(std::pow(2,4)),var a = 2; return a ^ 4;);
  PRIMITIVE_EQ(static_cast<double>(std::pow(2,4)),var a = 2.0; return a ^ 4;);
  PRIMITIVE_EQ(static_cast<double>(std::pow(2,4)),var a = 2; return a ^ 4.0;);
  PRIMITIVE_EQ(static_cast<double>(std::pow(2,4)),var a = 2.0; return a ^ 4.0;);
}

TEST(Interpreter,CompXV) {
  // < or >
  PRIMITIVE_EQ(true,var a = 4; return 2 < a;);
  PRIMITIVE_EQ(false,var b= 3; return 2 > b;);
  PRIMITIVE_EQ(true,var a = 4.0; return 2.0 < a; );
  PRIMITIVE_EQ(false,var b= 3.0; return 2.0 > b; );
  PRIMITIVE_EQ(true,var a = 4; return 2.0 < a; );
  PRIMITIVE_EQ(true,var a= 4.0;return 2  < a;  );
  PRIMITIVE_EQ(false,var b =3; return 2.0 > b; );
  PRIMITIVE_EQ(false,var b =3.0;return 2 > b;  );

  // <= or >=
  PRIMITIVE_EQ(true,var a = 2; return 2 <=a;);
  PRIMITIVE_EQ(true,var a = 2; return 2 >=a;);
  PRIMITIVE_EQ(false,var a =4.0; return 5.0 <=a;);
  PRIMITIVE_EQ(true,var a =4.0; return 5.0 >=a;);
  PRIMITIVE_EQ(true,var a = 2; return 2.0 <=a;);
  PRIMITIVE_EQ(true,var a = 2; return 2.0 >=a;);
  PRIMITIVE_EQ(false,var a= 4.0; return 5 <=a;);
  PRIMITIVE_EQ(true,var a = 4.0; return 5 >=a;);

  // == or !=
  PRIMITIVE_EQ(true,var a = 2; return 3 !=a; );
  PRIMITIVE_EQ(false,var a= 3; return 2 ==a; );
  PRIMITIVE_EQ(true,var a = 2.0; return 3.0 != a; );
  PRIMITIVE_EQ(false,var a = 3.0; return 2.0 == a; );
  PRIMITIVE_EQ(true,var a = 2 ; return 3.0 != a; );
  PRIMITIVE_EQ(false,var a = 3; return 2.0 == a; );
  PRIMITIVE_EQ(true,var a = 2 ; return 3.0 != a; );
  PRIMITIVE_EQ(false,var a = 3; return 2.0 == a; );

}


TEST(Interpreter,CompVX) {
  PRIMITIVE_EQ(true,var a = 4; return a > 2;);
  PRIMITIVE_EQ(true,var a = 4.0; return a > 2.0;);
  PRIMITIVE_EQ(true,var a = 4; return a > 2.0;);
  PRIMITIVE_EQ(true,var a= 4.0; return a > 2; );
  PRIMITIVE_EQ(false, var a= 2; return a > 4;);
  PRIMITIVE_EQ(false, var a=2.0; return a > 4.0;);
  PRIMITIVE_EQ(false, var a=2  ; return a > 4.0;);
  PRIMITIVE_EQ(false, var a=2.0; return a > 4; );

  PRIMITIVE_EQ(true,var a = 2; return a < 4; );
  PRIMITIVE_EQ(true,var a =2.0; return a < 4.0;);
  PRIMITIVE_EQ(true,var a =2.0; return a < 4; );
  PRIMITIVE_EQ(true,var a = 2 ; return a <4.0;);
  PRIMITIVE_EQ(false,var a = 4; return a < 2;);
  PRIMITIVE_EQ(false,var a = 4.0; return a < 2.0;);
  PRIMITIVE_EQ(false,var a = 4; return a < 2.0; );
  PRIMITIVE_EQ(false,var a = 4.0; return a < 2; );

  PRIMITIVE_EQ(true,var a = 4; return a >= 2;);
  PRIMITIVE_EQ(true,var a = 4.0; return a >= 2.0;);
  PRIMITIVE_EQ(true,var a = 4; return a >= 2.0;);
  PRIMITIVE_EQ(true,var a= 4.0; return a >= 2; );
  PRIMITIVE_EQ(false, var a= 2; return a >= 4;);
  PRIMITIVE_EQ(false, var a=2.0; return a >= 4.0;);
  PRIMITIVE_EQ(false, var a=2  ; return a >= 4.0;);
  PRIMITIVE_EQ(false, var a=2.0; return a >= 4; );

  PRIMITIVE_EQ(true,var a = 2; return a <= 4; );
  PRIMITIVE_EQ(true,var a =2.0; return a <= 4.0;);
  PRIMITIVE_EQ(true,var a =2.0; return a <= 4; );
  PRIMITIVE_EQ(true,var a = 2 ; return a <= 4.0;);
  PRIMITIVE_EQ(false,var a = 4; return a <= 2;);
  PRIMITIVE_EQ(false,var a = 4.0; return a <= 2.0;);
  PRIMITIVE_EQ(false,var a = 4; return a <= 2.0; );
  PRIMITIVE_EQ(false,var a = 4.0; return a <= 2; );

  PRIMITIVE_EQ(true,var a = 4; return a == 4;);
  PRIMITIVE_EQ(true,var a = 4.0; return a == 4.0;);
  PRIMITIVE_EQ(true,var a = 4; return a == 4.0;);
  PRIMITIVE_EQ(true,var a= 4.0; return a == 4; );

  PRIMITIVE_EQ(false, var a= 2; return a == 4;);
  PRIMITIVE_EQ(false, var a=2.0; return a == 4.0;);
  PRIMITIVE_EQ(false, var a=2  ; return a == 4.0;);
  PRIMITIVE_EQ(false, var a=2.0; return a == 4; );

  PRIMITIVE_EQ(true,var a = 2; return a != 4; );
  PRIMITIVE_EQ(true,var a =2.0; return a != 4.0;);
  PRIMITIVE_EQ(true,var a =2.0; return a != 4; );
  PRIMITIVE_EQ(true,var a = 2 ; return a != 4.0;);
  PRIMITIVE_EQ(false,var a = 4; return a != 4;);
  PRIMITIVE_EQ(false,var a = 4.0; return a != 4.0;);
  PRIMITIVE_EQ(false,var a = 4; return a != 4.0; );
  PRIMITIVE_EQ(false,var a = 4.0; return a != 4; );
}

TEST(Interpreter,SSOEQ) {
  PRIMITIVE_EQ(true,var a = "a"; return a == "a"; );
  PRIMITIVE_EQ(false,var a = "f"; return a == "a";);
  PRIMITIVE_EQ(true,var a = "f" ; return a != "a";);
  PRIMITIVE_EQ(false,var a = "a"; return a != "a";);

  PRIMITIVE_EQ(true,var a = "a"; return "a" == a; );
  PRIMITIVE_EQ(false,var a = "f"; return "a" == a;);
  PRIMITIVE_EQ(true,var a = "f" ; return "a" != a;);
  PRIMITIVE_EQ(false,var a = "a"; return "a" != a;);
}

TEST(Interpreter,Neg) {
  PRIMITIVE_EQ(-1,var a = 1; return -a;);
  PRIMITIVE_EQ(-1.0,var a = 1.0; return -a;);
}

TEST(Interpreter,Not) {
  PRIMITIVE_EQ(true,var a = false; return !a;);
  PRIMITIVE_EQ(false,var a = true; return !a;);
  PRIMITIVE_EQ(true, var a = null; return !a;);
  PRIMITIVE_EQ(false,var a = "a"; return !a;);
  PRIMITIVE_EQ(false,var a = 0; return !a;);
  PRIMITIVE_EQ(false, var a = 1.0; return !a;);
}

TEST(Interpreter,Logic) {
  PRIMITIVE_EQ(false,var a = true; var b = false; return a && b;);
  PRIMITIVE_EQ(true,var a = true; var b = true; return a && b;);
  PRIMITIVE_EQ(false,var a = false;var b= null; return a && b;);
  PRIMITIVE_EQ(,var a = null; var b = true; return a&&b;);
  PRIMITIVE_EQ(true,var a = 0; return a && true; );
  PRIMITIVE_EQ(false,var a = 1.0; return a && false; );

  PRIMITIVE_EQ(true,var a = false; return a || true; );
  PRIMITIVE_EQ(false,var a = false; return a || false; );
  PRIMITIVE_EQ(0, var a = 0; return a || false; );
  PRIMITIVE_EQ(2.0,var a = 2.0; return a || false; );
  PRIMITIVE_EQ(1, var a = 1; return false || a; );
  PRIMITIVE_EQ(2.0,var a = 2.0; return false || a; );
}

TEST(Interpreter,SimpleLoop) {
	PRIMITIVE_EQ(10,var a = 0; for( var i = 0.0 ; 10.0 ; 1.0 ) { a = a + 1; } return a;);
	PRIMITIVE_EQ(10,var a = 0; for( var i = 0 ; 10 ; 1 ) { a = a + 1; } return a;);
	PRIMITIVE_EQ(10,var a = 0; for( var i = 0 ; 10 ; 1.0 ) { a = a + 1; } return a;);
	PRIMITIVE_EQ(10,var a = 0; for( var i = 0.0 ; 10 ; 1 ) { a = a + 1; } return a;);
  PRIMITIVE_EQ(10,var a = 0; for( var i = 0 ; 10.0 ; 1 ) { a = a + 1; } return a;);
}

TEST(Interpreter,SimpleBranch) {
  PRIMITIVE_EQ(10,
      var a = true;
      if(a) {
        return 10;
      } else {
        return -11;
      }
  );

  PRIMITIVE_EQ(10,
      var a = true;
      if(a) return 10;
      return -11;
  );

  PRIMITIVE_EQ(-11,
      var a = false;
      if(!a) return -11;
      return 1;
  );

  PRIMITIVE_EQ(1,
      var a = true;
      var b = false;
      if(!(a && b)) return 1;
      return -10;
  );

  PRIMITIVE_EQ(true,
      var a = 10;
      if(a > 12) {
      return false;
      }
      return true;
  );

  PRIMITIVE_EQ(true,
      var a = 10;
      if(a) {
        if(a-1) {
          if(a-2) {
            if(a-3) {
              if(a-4) {
                if(a-5) {
                  return true;
                }
              }
            }
          }
        }
      } else {
        return false;
      }
      return 100;
  );

  PRIMITIVE_EQ(false,
      var a = 10;
      if(a == 10) {
        var b = 20;
        if(b == 20) {
          var c = 30;
          if(c != 30) {
            return true;
          }
        }
        return false;
      }
      return 100;
  );

  PRIMITIVE_EQ(,
      var a = 10;
      if( a == 10 ) {
        var b;
        if(!b)
         return null;
      }
      return 100 + 2 * foo();
  );

  PRIMITIVE_EQ(10,
      var a = true;
      var b = false;
      if(!b == a) {
        return 10;
      }
      return -100;
  );
}

TEST(Interpreter,Branch) {

  PRIMITIVE_EQ(10,
      var a = true;
      var b = false;
      if(!b == a) {
        return 10;
      }
      return -100;
  );
}

TEST(Interpreter,FuncCall) {
  PRIMITIVE_EQ(true,
      var foo = function() { return true; };
      var c = foo();
      return c;
      );
  PRIMITIVE_EQ(true,
      var foo = function() { return true; };
      return foo();
      ); // tail call optimization
  PRIMITIVE_EQ(0,
      var foo = function(a,b) {
        if(a <1) return a;
        return b(a-1,b);
      };
      return foo(100,foo);
      ); // tail call optimization
  PRIMITIVE_EQ(5702887,
      var fib = function(a,fib) {
        if(a < 2) return a;
        return fib(a-1,fib) + fib(a-2,fib);
      };
      return fib(34,fib);
     );
}

TEST(Interpreter,ArrayIndexI) {
  PRIMITIVE_EQ(4,
      var bar = [1,2,3,4,5];
      return bar[3];
      );

  PRIMITIVE_EQ(3,
      var bar = [1,2,3,4,5];
      return bar[2];
      );

  PRIMITIVE_EQ(5,
      var bar = [1,2,3,4,5];
      return bar[4];
      );

}

TEST(Interpreter,ArrayIndexVarI) {
  PRIMITIVE_EQ(4,
      var bar = [1,2,3,4,5];
      var idx = 3;
      return bar[idx];
  );
}

TEST(Intepreter,ArrayIndexSetI) {
  PRIMITIVE_EQ(4,
      var bar = [0,0,0,0,0];
      bar[1] = 4;
      return bar[1];
      );
  PRIMITIVE_EQ(4,
      var bar = [0,0,0,0,0];
      var idx = 1;
      bar[idx] = 4;
      return bar[idx];
      );
}


TEST(Interpreter,ObjectSSOGet) {
  PRIMITIVE_EQ(true,
      var b = { "a" : true , "b" : false , "uuvvhhgg" : 3 , "xxvvhhgg" : 4 };
      return b.a;
      );

  PRIMITIVE_EQ(4,
      var b = { "a" : true , "b" : false , "uuvvhhgg" : 3 , "xxvvhhgg" : 4 };
      return b.xxvvhhgg;
      );

  PRIMITIVE_EQ(,
      var b = { "a" : true , "b" : false , "uuvvhhgg" : 3 , "xxvvhhgg" : null };
      return b.xxvvhhgg;
      );
  PRIMITIVE_EQ(true,
      var b = { "a" : true , "b" : false , "uuvvhhgg" : 3 , "xxvvhhgg" : 4 };
      return b.a;
      );
}

TEST(Interpreter,ObjectSSOSet) {
  PRIMITIVE_EQ(200,
      var b = { "a" : 1 };
      b.a = 200;
      return b.a;
      );
}


} // namespace lavascript
} // namespace interpreter

int main( int argc, char* argv[] ) {
  ::lavascript::InitTrace("-");
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}
