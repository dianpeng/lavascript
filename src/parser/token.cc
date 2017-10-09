#include "token.h"
#include "src/trace.h"

#include <cstddef>

namespace lavascript {
namespace parser {

namespace {

int kTokenType [] = {
#define __(A,B,C,D,E) Token::E,
  LAVA_TOKEN_LIST(__)
#undef __ // __
  0
};

const char* kTokenName [] = {
#define __(A,B,C,D,E) B,
  LAVA_TOKEN_LIST(__)
#undef __ // __
  NULL
};

} // namespace

int Token::GetTokenType( int token ) {
  lava_assertF( token >= 0 && token < SIZE_OF_TOKENS , "Unknown token value %d" , token );
  return kTokenType[token];
}

const char* Token::GetTokenName( int token ) {
  lava_assertF( token >= 0 && token < SIZE_OF_TOKENS , "Unknown token value %d" , token );
  return kTokenName[token];
}

/**
 * Definition of all static token instances inside of Token class
 */
#define __(A,B,C,D,E) Token Token::k##D(Token::A);
LAVA_TOKEN_LIST(__)
#undef __ // __

} // namespace parser
} // namespace lavascript
