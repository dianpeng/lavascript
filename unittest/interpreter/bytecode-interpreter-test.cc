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

#include "src/interpreter/bytecode-interpreter.h"

#define stringify(...) #__VA_ARGS__

using namespace lavascript;
using namespace lavascript::parser;
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
  if(!interpreter::GenerateBytecode(context,*result,sb,error)) {
    std::cerr<<"FAILED AT COMPILE:"<<*error<<std::endl;
    return false;
  }
  return true;
}

} // namespace


int main() {
  lavascript::InitTrace("-");
  std::shared_ptr<lavascript::interpreter::AssemblyInterpreter>
    interp( lavascript::interpreter::AssemblyInterpreter::Generate() );
  lava_verify(interp);
  {
    lavascript::DumpWriter writer(NULL);
    interp->Dump(&writer);
  }
  lavascript::interpreter::AssemblyInterpreter::Instance ins(interp);

  Context ctx(TestGCConfig());
  std::string error;
  std::string script(stringify(
        var a = 10;
        var b = 20;
        var c = true;
        var d = c ? a + 10 : b + 10;
        return d;
        ));

  ScriptBuilder sb("a",script);
  assert(Compile(&ctx,script.c_str(),&sb,&error));
  DumpWriter dw;
  sb.Dump(&dw);
  Handle<Script> scp( Script::New(ctx.gc(),&ctx,sb) );
  Handle<Object> obj( Object::New(ctx.gc()) );
  Value ret;
  bool r = ins.Run(&ctx,scp,obj,&error,&ret);
  assert(r);
  assert(ret.IsInteger());
  std::cout<<ret.GetInteger()<<std::endl;
  return r;
}
