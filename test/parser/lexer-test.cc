#include <src/parser/lexer.h>
#include <src/zone/string.h>
#include <src/zone/zone.h>
#include <src/core/trace.h>
#include <gtest/gtest.h>

#define stringify(...) #__VA_ARGS__

namespace lavascript {
namespace parser {

using namespace ::lavascript::zone;

TEST(Lexer,Operator) {
  Zone zone;
  Lexer lexer(&zone,stringify(+ - * / % ^ < <= >
                              >= == != && || ! .
                              ? : ; [ ] ( ) } {));

  ASSERT_EQ(Token::kAdd,lexer.Next().token);
  ASSERT_EQ(Token::kSub,lexer.Next().token);
  ASSERT_EQ(Token::kMul,lexer.Next().token);
  ASSERT_EQ(Token::kDiv,lexer.Next().token);
  ASSERT_EQ(Token::kMod,lexer.Next().token);
  ASSERT_EQ(Token::kPow,lexer.Next().token);
  ASSERT_EQ(Token::kLT ,lexer.Next().token);
  ASSERT_EQ(Token::kLE ,lexer.Next().token);
  ASSERT_EQ(Token::kGT ,lexer.Next().token);
  ASSERT_EQ(Token::kGE ,lexer.Next().token);
  ASSERT_EQ(Token::kEQ ,lexer.Next().token);
  ASSERT_EQ(Token::kNE ,lexer.Next().token);
  ASSERT_EQ(Token::kAnd,lexer.Next().token);
  ASSERT_EQ(Token::kOr ,lexer.Next().token);
  ASSERT_EQ(Token::kNot,lexer.Next().token);
  ASSERT_EQ(Token::kDot,lexer.Next().token);
  ASSERT_EQ(Token::kQuestion,lexer.Next().token);
  ASSERT_EQ(Token::kColon,lexer.Next().token);
  ASSERT_EQ(Token::kSemicolon,lexer.Next().token);
  ASSERT_EQ(Token::kLSqr,lexer.Next().token);
  ASSERT_EQ(Token::kRSqr,lexer.Next().token);
  ASSERT_EQ(Token::kLPar,lexer.Next().token);
  ASSERT_EQ(Token::kRPar,lexer.Next().token);
  ASSERT_EQ(Token::kRBra,lexer.Next().token);
  ASSERT_EQ(Token::kLBra,lexer.Next().token);
  ASSERT_EQ(Token::kEof,lexer.Next().token);
}

TEST(Lexer,Keyword) {
  Zone zone;
  Lexer lexer(&zone,stringify(if elif else for break continue return var function true false null));
  ASSERT_EQ(Token::kIf,lexer.Next().token);
  ASSERT_EQ(Token::kElif,lexer.Next().token);
  ASSERT_EQ(Token::kElse,lexer.Next().token);
  ASSERT_EQ(Token::kFor,lexer.Next().token);
  ASSERT_EQ(Token::kBreak,lexer.Next().token);
  ASSERT_EQ(Token::kContinue,lexer.Next().token);
  ASSERT_EQ(Token::kReturn,lexer.Next().token);
  ASSERT_EQ(Token::kVar,lexer.Next().token);
  ASSERT_EQ(Token::kFunction,lexer.Next().token);
  ASSERT_EQ(Token::kTrue,lexer.Next().token);
  ASSERT_EQ(Token::kFalse,lexer.Next().token);
  ASSERT_EQ(Token::kNull,lexer.Next().token);
  ASSERT_EQ(Token::kEof,lexer.Next().token);
}

TEST(Lexer,Id) {
  Zone zone;
  Lexer lexer(&zone,"if_ if _if else else_ _123 _");

  ASSERT_EQ( Token::kIdentifier , lexer.Next().token );
  ASSERT_EQ( *lexer.lexeme().str_value , "if_" );

  ASSERT_EQ( Token::kIf , lexer.Next().token );

  ASSERT_EQ( Token::kIdentifier , lexer.Next().token );
  ASSERT_EQ( *lexer.lexeme().str_value , "_if" );

  ASSERT_EQ( Token::kElse , lexer.Next().token );

  ASSERT_EQ( Token::kIdentifier , lexer.Next().token );
  ASSERT_EQ( *lexer.lexeme().str_value , "else_" );

  ASSERT_EQ( Token::kIdentifier , lexer.Next().token );
  ASSERT_EQ( *lexer.lexeme().str_value , "_123" );

  ASSERT_EQ( Token::kIdentifier , lexer.Next().token );
  ASSERT_EQ( *lexer.lexeme().str_value , "_" );

  ASSERT_EQ( Token::kEof , lexer.Next().token );
}

TEST(Lexer,String) {
  Zone zone;
  Lexer lexer(&zone,stringify("" "\n" "\t" "\r" "a\b\\" "\"" "abc"));

  ASSERT_EQ( Token::kString , lexer.Next().token );
  ASSERT_EQ( *lexer.lexeme().str_value , "" );

  ASSERT_EQ( Token::kString , lexer.Next().token );
  ASSERT_TRUE( *lexer.lexeme().str_value == "\n" );

  ASSERT_EQ( Token::kString , lexer.Next().token );
  ASSERT_TRUE( *lexer.lexeme().str_value == "\t" );

  ASSERT_EQ( Token::kString , lexer.Next().token );
  ASSERT_TRUE( *lexer.lexeme().str_value == "\r" );

  ASSERT_EQ( Token::kString , lexer.Next().token );
  ASSERT_TRUE( *lexer.lexeme().str_value == "a\b\\" );

  ASSERT_EQ( Token::kString , lexer.Next().token );
  ASSERT_TRUE( *lexer.lexeme().str_value == "\"" );

  ASSERT_EQ( Token::kString , lexer.Next().token );
  ASSERT_TRUE( *lexer.lexeme().str_value == "abc" );

  ASSERT_EQ( Token::kEof , lexer.Next().token );
}

TEST(Lexer,Number) {
  Zone zone;
  Lexer lexer(&zone,stringify(123 0 1.0 0.0 1.234 1.));
  ASSERT_EQ( Token::kInteger , lexer.Next().token );
  ASSERT_EQ( 123 , lexer.lexeme().int_value );

  ASSERT_EQ( Token::kInteger , lexer.Next().token );
  ASSERT_EQ( 0 , lexer.lexeme().int_value );

  ASSERT_EQ( Token::kReal , lexer.Next().token);
  ASSERT_EQ( 1.0 , lexer.lexeme().real_value );

  ASSERT_EQ( Token::kReal , lexer.Next().token );
  ASSERT_EQ( 0.0 , lexer.lexeme().real_value );

  ASSERT_EQ( Token::kReal , lexer.Next().token );
  ASSERT_EQ( 1.234,lexer.lexeme().real_value );

  ASSERT_EQ( Token::kInteger , lexer.Next().token );
  ASSERT_EQ( 1 , lexer.lexeme().int_value );

  ASSERT_EQ( Token::kDot , lexer.Next().token );
  ASSERT_EQ( Token::kEof , lexer.Next().token );
}

} // namespace core
} // namespace lavascript

int main( int argc, char* argv[] ) {
  ::lavascript::core::InitTrace("-");
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}
