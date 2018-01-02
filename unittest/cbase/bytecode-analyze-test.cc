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
#include <bitset>
#include <vector>

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

typedef std::bitset<256> RegisterSet;

struct BBInfo {
  std::size_t offset;
  RegisterSet set;
  BBInfo( std::size_t o , std::initializer_list<std::uint8_t> l ): offset(o),set() {
    for( auto & e : l ) {
      set[e] = true;
    }
  }
};

typedef std::vector<BBInfo> RA;

bool DumpBytecodeAnalyze( const RA& bb , const RA& loop , const char* source ) {
  Context ctx;
  std::string error;

  ScriptBuilder sb(":test",source);
  if(!Compile(&ctx,source,&sb,&error)) {
    std::cerr<<error<<std::endl;
    return false;
  }
  Handle<Script> scp( Script::New(ctx.gc(),&ctx,sb) );
  BytecodeAnalyze ba(scp->main());

  // DumpWriter dw;
  // sb.Dump(&dw);
  // ba.Dump(&dw);

  const std::uint32_t* bc_start = scp->main()->code_buffer();

  // do the verification
  for( auto & e : bb ) {
    const std::uint32_t* pc = bc_start + e.offset;
    auto ret = ba.LookUpBasicBlock(pc);
    if(!ret) {
      std::cerr<<"Address "<<bc_start<<" doesn't have basic block"<<std::endl;
      return false;
    }
    for( std::size_t i = 0 ; i < 256 ; ++i ) {
      if(e.set[i] != ret->variable[i]) {
        std::cerr<<"Register "<<i<<" mismatch expect "<<e.set[i]<<" but get "<<ret->variable[i]<<std::endl;
        return false;
      }
    }
  }

  for( auto & e : loop ) {
    const std::uint32_t* pc = bc_start + e.offset;
    auto ret = ba.LookUpLoopHeader(pc);
    if(!ret) {
      std::cerr<<"Address "<<bc_start<<" doesn't have loop block"<<std::endl;
      return false;
    }
    for( std::size_t i = 0 ; i < 256 ; ++i ) {
      if(e.set[i] != ret->phi[i]) {
        std::cerr<<"Register "<<i<<" mismatch expect "<<e.set[i]<<" but get "<<ret->phi[i]<<std::endl;
        return false;
      }
    }
  }

  return true;
}

} // namespace

#define STRINGIFY(...) #__VA_ARGS__

TEST(BytecodeAnalyze,Basic) {
  ASSERT_TRUE(DumpBytecodeAnalyze(
        RA{ BBInfo(0,{0,1,2,3,4,5}) },
        RA{},
        STRINGIFY(
          var a = 10;
          var b = 20;
          var c = 30;
          var d = 40;
          a = 40;
          b = 50;
          var e = 50;
          var f = 60;
          )));
}

TEST(BytecodeGenerate,Branch) {
  // if-elif style branch
  ASSERT_TRUE(DumpBytecodeAnalyze(
      RA{
        BBInfo(0,{0,1,2}),
        BBInfo(5,{3,4}),
        BBInfo(11,{3,4})
      },
      RA{},
      STRINGIFY(
      var a = 10;
      var b = 20;
      var hu = 20;
      if(true) {
        var x = 20;
        var y = 30;
        a = 40;
      } elif (a !=b) {
        var u = 20;
        var vv = 30;
        b = 30;
      }
      return a + b;
      )));

  // if style branch
  ASSERT_TRUE(DumpBytecodeAnalyze(
        RA{
          BBInfo(0,{0,1,2}),
          BBInfo(5,{3,4,5})
        },
        RA{},
        STRINGIFY(
          var a = 10;
          var b = 20;
          var hu= 30;
          if(true) {
            var a = 444;
            var xx = 20;
            var yy = 39;
            hu = a + b;
          }
  )));

  ASSERT_TRUE(DumpBytecodeAnalyze(
        RA{ BBInfo(0,{0,1,2}),
            BBInfo(5,{3,4}),
            BBInfo(8,{3,4,5})
          },
        RA{},
        STRINGIFY(
          var a = 10;
          var b = 20;
          var c = 30;
          if(true) {
            var aa = 20;
            var bb = 30;
          } else {
            var cc = 40;
            var dd = 50;
            var ee = 60;
            a = b + c;
          }
  )));

  ASSERT_TRUE(DumpBytecodeAnalyze(
        RA{ BBInfo(0,{0,1,2}),
            BBInfo(5,{3,4}),
            BBInfo(10,{3,4,5}),
            BBInfo(15,{3,4,5})
          },
        RA{},
        STRINGIFY(
          var a = 10;
          var b = 20;
          var c = 30;
          if(true) {
            var aa = 20;
            var bb = 30;
          } elif(a==b) {
            var cc = 40;
            var dd = 50;
            var ee = 60;
            a = b + c;
          } else {
            var xx = 20;
            var cc = 30;
            var dd = 40;
          }
  )));
}

TEST(BytecodeAnalyze,Loop) {
  ASSERT_TRUE(DumpBytecodeAnalyze(
        RA{ BBInfo(0,{0,1,2,3,4,5}),
            BBInfo(8,{6,7})
          },
        RA{ BBInfo(8,{0,1,2}) },
        STRINGIFY(
          var a = 10;
          var b = 20;
          var c = 30;
          for( var i = 1 ; 100; 1 ) {
            var xx = 20;
            a = b + 1;
            b = c + 1;
            c = a + 1;
            var yy = 30;
          }
          )));

  ASSERT_TRUE(DumpBytecodeAnalyze(
        RA{ BBInfo(0,{0,1,2,3,4,5}),
            BBInfo(8,{6})
          },
        RA{ BBInfo(8,{1,2}) },
        STRINGIFY(
          var a = 10;
          var b = 20;
          var c = 30;
          for( var i = 1 ; 100; 1 ) {
            var a = 20;
            a = b + 1;
            b = c + 1;
            c = a + 1;
          }
          )));
}

} // namespace cbase
} // namespace lavascript

int main( int argc, char* argv[] ) {
  ::lavascript::InitTrace("-");
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}
