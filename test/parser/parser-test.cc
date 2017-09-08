#include <src/parser/parser.h>
#include <src/parser/ast/ast.h>
#include <gtest/gtest.h>


#define stringify(...) #__VA_ARGS__

namespace lavascript {
namespace parser {

using namespace ::lavascript::zone;
using namespace ::lavascript::parser;

TEST(Parser,Expression) {
  Zone zone;
  std::string error;
  Parser parser(stringify(
        a = -1+2*3/b;
        b = [];
        c = {};
        d = 1 > 2 && 2 == f;
        e = 1 > 2 ? 3 : true;
        f = "";
        g = "asd" + null;
        a = !!!!!b;
        a = ----d;
        a = (1+2)*3;
        d = [1,2,3,4,5];
        d = ["","str",true,false,null,1,1.1];
        e = {"a":b,cccc:d,[expr+100]:"c"};
        ee= function() { a = 10; var c = 100; return 100; };
        function f(a,b,c) {
          if(a) {
            return true;
          } elif(b == false) {
            return null;
          } else {
            for( var a = 10 ; a < 100; 1 ) {
              sum = sum + a;
              if(a %2) break;
              else continue;
            }
            return sum;
          }

          for( var c in [1,2,3,4,5,6] ) {
            print(c);
          }
        }
        ),&zone,&error);

  ast::Root* result = parser.Parse();
  ASSERT_TRUE(result)<<error;
  ast::DumpAst(*result,std::cout);
}

} // namespace parser
} // namespace lavascript

int main( int argc, char* argv[] ) {
  ::lavascript::core::InitTrace("-");
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}
