#include <src/parser/parser.h>
#include <src/parser/ast/ast.h>
#include <gtest/gtest.h>
#include <cassert>


#define stringify(...) #__VA_ARGS__

namespace lavascript {
namespace parser {

using namespace ::lavascript::zone;
using namespace ::lavascript::parser;

static const bool kDumpAst = true;

struct Dummy {};

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

/******
 * Helper function for testing the Literal expression
 *****/

bool ExprCheckLiteral( const char* source , ast::Literal** output,
                       Zone* zone , std::string* error ) {
  Parser parser(source,zone,error);
  ast::Root* result = parser.Parse();
  if(result) {
    if(kDumpAst) ast::DumpAst(*result,std::cerr);
    ast::Node* first = result->body->body->Index(0);
    assert( first->IsAssign() );
    assert( first->AsAssign()->rhs->IsLiteral() );
    *output = first->AsAssign()->rhs->AsLiteral();
    return true;
  }
  return false;
}

bool ExprCheckLiteral( const char* source , int ival ,
                       Zone* zone , std::string* error ) {
  ast::Literal* output;
  if(!ExprCheckLiteral(source,&output,zone,error)) return false;
  return (output->IsInteger() && ival == output->int_value);
}

bool ExprCheckLiteral( const char* source , double rval,
                       Zone* zone , std::string* error ) {
  ast::Literal* output;
  if(!ExprCheckLiteral(source,&output,zone,error)) return false;
  return (output->IsReal() && rval == output->real_value);
}

bool ExprCheckLiteral( const char* source , bool bval,
                       Zone* zone , std::string* error ) {
  ast::Literal* output;
  if(!ExprCheckLiteral(source,&output,zone,error)) return false;
  return (output->IsBoolean() && bval == output->bool_value);
}

bool ExprCheckLiteral( const char* source , const char* string ,
                       Zone* zone , std::string* error ) {
  ast::Literal* output;
  if(!ExprCheckLiteral(source,&output,zone,error)) return false;
  return (output->IsString() && strcmp(string,output->str_value->data()) == 0);
}

bool ExprCheckLiteral( const char* source , const Dummy& dummy ,
                       Zone* zone , std::string* error ) {
  (void)dummy;
  ast::Literal* output;
  if(!ExprCheckLiteral(source,&output,zone,error)) return false;
  return (output->IsNull());
}

#define ConstExprCheck(VAL,...) \
  do {                                                                   \
    std::string error;                                                   \
    Zone zone;                                                           \
    ASSERT_TRUE(ExprCheckLiteral(#__VA_ARGS__,VAL,&zone,&error))<<error; \
  } while(false)


TEST(Parser,Expression) {
  /* --------------------------------------
   * Constant folding
   * -------------------------------------*/

  ConstExprCheck(-1,a=-1;);
  ConstExprCheck(Dummy(),a=null;);

  // Unary constant folding
  positive( a = -1; );
  positive( a = --1;);
  positive( a = !true; );
  positive( a = !false;);

  // Binary constant folding
  positive( a = 1 + 2; );
  positive( a = 1 * 2; );
  positive( a = 1 + 2 * 3; );
  positive( a = 2 * 1 / 2; );
  positive( a = (1+2) * 3; );
  positive( a = 2 / 2; );
  positive( a = 2 % 2; );
  positive( a = 2 ^ 3; );
  positive( a = 1+b; );
  positive( a = 0+b; );
  positive( a = b+0; );
  positive( a = b-0; );
  positive( a = 0-b; );
  positive( a = 1*b; );
  positive( a = b*1; );
  positive( a = 0*x; );
  positive( a = x*0; );
  positive( a = 0/x; );
  positive( a = 0 ^ x; );

  positive( a = 1.2 + 2; );
  positive( a = 2.2 + 1; );
  positive( a = 0 + 3.2; );
  positive( a = 0 * 2.4; );
  positive( a = 1.0 / 1.0; );
  positive( a = 2.0 ^ 2.2; );

  positive( a = 1 > 2; );
  positive( a = 1 < 2; );
  positive( a = 1 >=2; );
  positive( a = 2 <=3; );
  positive( a = 1 ==2; );
  positive( a = 2 ==2; );
  positive( a = 1 !=2; );
  positive( a = 2 !=2; );

  positive( a = (1+2) && true; );
  positive( a = 0 && []; );
  positive( a = false && (1+2*a); );
  positive( a = 0 || false; );
  positive( a = false || []; );
  positive( a = "asb" && "bsd"; );
  positive( a = null || c; );
  positive( a = true && null; );


  // Ternary constant folding
  positive( a = true ? 1 : 2 ; );
  positive( a = false? 2 : 1 ; );
}

} // namespace parser
} // namespace lavascript

int main( int argc, char* argv[] ) {
  ::lavascript::core::InitTrace("-");
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}
