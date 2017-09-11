#include <src/parser/parser.h>
#include <src/parser/ast/ast.h>
#include <gtest/gtest.h>
#include <cassert>


#define stringify(...) #__VA_ARGS__

namespace lavascript {
namespace parser {

using namespace ::lavascript::zone;
using namespace ::lavascript::parser;

static const bool kDumpAst = false;

struct Dummy {};
struct ID {
  ID( const char* n ):name(n) {}
  std::string name;
};

bool show( const char* source , std::string* error ) {
  Zone zone;
  Parser parser(source,&zone,error);
  ast::Root* result = parser.Parse();
  if(result) {
    ast::DumpAst(*result,std::cerr);
    return true;
  }
  return false;
}

#define positive(...)                                             \
  do {                                                            \
    std::string error;                                            \
    ASSERT_TRUE(show(#__VA_ARGS__,&error)) << error << std::endl; \
  } while(false)

#define negative(...)                                             \
  do {                                                            \
    std::string error;                                            \
    ASSERT_FALSE(show(#__VA_ARGS__,&error));                      \
  } while(false)

/******
 * Helper function for testing the Literal expression
 *****/

bool ExprCheckLiteral( const char* source , ast::Node** output,
                       Zone* zone , std::string* error ) {
  Parser parser(source,zone,error);
  ast::Root* result = parser.Parse();
  if(result) {
    if(kDumpAst) ast::DumpAst(*result,std::cerr);
    ast::Node* first = result->body->body->Index(0);
    assert( first->IsAssign() );
    *output = first->AsAssign()->rhs;
    return true;
  }
  return false;
}

bool ExprCheckLiteral( const char* source , int ival ,
                       Zone* zone , std::string* error ) {
  ast::Node* ou;
  if(!ExprCheckLiteral(source,&ou,zone,error)) return false;
  if(!ou->IsLiteral()) return false;

  ast::Literal* output = ou->AsLiteral();
  return (output->IsInteger() && ival == output->int_value);
}

bool ExprCheckLiteral( const char* source , double rval,
                       Zone* zone , std::string* error ) {
  ast::Node* ou;
  if(!ExprCheckLiteral(source,&ou,zone,error)) return false;
  if(!ou->IsLiteral()) return false;

  ast::Literal* output = ou->AsLiteral();
  return (output->IsReal() && rval == output->real_value);
}

bool ExprCheckLiteral( const char* source , bool bval,
                       Zone* zone , std::string* error ) {
  ast::Node* ou;
  if(!ExprCheckLiteral(source,&ou,zone,error)) return false;
  if(!ou->IsLiteral()) return false;

  ast::Literal* output = ou->AsLiteral();
  return (output->IsBoolean() && bval == output->bool_value);
}

bool ExprCheckLiteral( const char* source , const char* string ,
                       Zone* zone , std::string* error ) {
  ast::Node* ou;
  if(!ExprCheckLiteral(source,&ou,zone,error)) return false;
  if(!ou->IsLiteral()) return false;

  ast::Literal* output = ou->AsLiteral();
  return (output->IsString() &&
          strcmp(string,output->str_value->data()) == 0);
}

bool ExprCheckLiteral( const char* source , const Dummy& dummy ,
                       Zone* zone , std::string* error ) {
  (void)dummy;
  ast::Node* ou;
  if(!ExprCheckLiteral(source,&ou,zone,error)) return false;
  if(!ou->IsLiteral()) return false;

  ast::Literal* output = ou->AsLiteral();
  return (output->IsNull());
}

bool ExprCheckLiteral( const char* source , const ID& id ,
                       Zone* zone , std::string* error ) {
  ast::Node* ou;
  if(!ExprCheckLiteral(source,&ou,zone,error)) return false;
  if(!ou->IsVariable()) return false;

  ast::Variable* output = ou->AsVariable();
  return (id.name == (output->name->data()));
}

#define ConstExprCheck(VAL,...) \
  do {                                                                   \
    std::string error;                                                   \
    Zone zone;                                                           \
    ASSERT_TRUE(ExprCheckLiteral(#__VA_ARGS__,VAL,&zone,&error))<<error; \
  } while(false)


TEST(Parser,ConstantFolding) {
  /* --------------------------------------
   * Constant folding
   * -------------------------------------*/
  ConstExprCheck(-1,a=---1;);
  ConstExprCheck(true,a=!false;);
  ConstExprCheck(false,a=!true;);
  ConstExprCheck(1,a=----1;);

  ConstExprCheck(-1,a=-1;);
  ConstExprCheck(Dummy(),a=null;);
  ConstExprCheck(-2,a=-1-1;);
  ConstExprCheck(3,a=1+1*2;);
  ConstExprCheck(7,a=1+2*3;);
  ConstExprCheck(4,a=16/4;);
  ConstExprCheck(5.0,a=2.5+2.5;);
  ConstExprCheck(4,a=2^2;);

  /** Strength reduction */
  ConstExprCheck(ID("a"),a = a*1;);
  ConstExprCheck(ID("a"),a = 1*a;);
  ConstExprCheck(ID("b"),a = 0+b;);
  ConstExprCheck(ID("b"),a = b+0;);
  ConstExprCheck(0,a = a*0;);
  ConstExprCheck(0,a = 0*a;);
  ConstExprCheck(ID("b"),a = b/1;);
  ConstExprCheck(0,a = 0/b;);
  ConstExprCheck(0,a = 0^a;);

  ConstExprCheck(true,a = 1 < 2; );
  ConstExprCheck(true,a = 2 <=2; );
  ConstExprCheck(false,a= 1 > 2; );
  ConstExprCheck(false,a= 3 >=4; );
  ConstExprCheck(false,a= 2 ==3; );
  ConstExprCheck(true, a= 2 !=3; );

  /** loigcal expression */
  ConstExprCheck(true ,a = true && true;);
  ConstExprCheck(false,a = false&& true;);
  ConstExprCheck(false,a = true &&false;);
  ConstExprCheck(false,a = false&&false;);
  ConstExprCheck(true ,a = true || b;);
  ConstExprCheck(ID("b"),a = false ||b;);
  ConstExprCheck("asd",a = "xxx"&&"asd";);
  ConstExprCheck("ddd",a = "ddd"||"xxx";);

  /** ternary expression */
  ConstExprCheck(1,a = true ? 1 : 2;);
  ConstExprCheck(2,a = false? 1 : 2;);
  ConstExprCheck(3,a = 1 == 1 ? 3 : false;);

  /** constant function call */
  ConstExprCheck(1,a = min(1,2););
  ConstExprCheck(2,a = max(1,2););
  ConstExprCheck("string",a = type(""););
  ConstExprCheck("boolean",a = type(true););
  ConstExprCheck("boolean",a = type(false););
  ConstExprCheck("null",a = type(null););
  ConstExprCheck("integer",a = type(1););
  ConstExprCheck("real",a = type(1.0););
  ConstExprCheck(1,a=int(1););
  ConstExprCheck(1,a=int(1.0););
  ConstExprCheck(1234,a=int("1234"););
  ConstExprCheck(true,a=boolean(true););
  ConstExprCheck(false,a=boolean(false););
  ConstExprCheck(true,a=boolean("true"););
  ConstExprCheck(true,a=boolean(1););
  ConstExprCheck(true,a=boolean(1.1););
  ConstExprCheck(false,a=boolean("false"););
  ConstExprCheck("1",a=string(1););
  ConstExprCheck("1.234",a=string(1.234););
  ConstExprCheck(1,a=len("a"););
  ConstExprCheck(2,a=len("ab"););
  ConstExprCheck(0,a=len([]););
  ConstExprCheck(1,a=len([1]););
  ConstExprCheck(0,a=len({}););
  ConstExprCheck(1,a=len({"a":1}););
}

TEST(Parser,Expression) {
  /** ----------------------------------
   *  Expression
   *  --------------------------------*/
  positive(a = 1+b;);
  positive(a = a+b;);
  positive(a = a*b;);
  positive(a = c/b;);
  positive(a = d^f;);
  positive(a = 1+2^d;);
  positive(a = 1*c/d;);
  positive(a = d%f+1;);
  positive(a = d + (1+2+3););
  positive(a = d * (1+2*3););
  positive(a = d / (1+2+c););
  positive(a = d +  1+2+3 ;);
}

} // namespace parser
} // namespace lavascript

int main( int argc, char* argv[] ) {
  ::lavascript::core::InitTrace("-");
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}
