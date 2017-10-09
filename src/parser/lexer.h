#ifndef PARSER_LEXER_H_
#define PARSER_LEXER_H_
#include "token.h"
#include <cctype>
#include <cstdint>
#include <string>

#include "src/zone/zone.h"
#include "src/error-report.h"
#include "src/util.h"

namespace lavascript {

namespace zone {
class String;
} // namespace zone

namespace parser {

/**
 * Lexeme represents a token's related information and attributes
 */
struct Lexeme {
  Token token;                    // Token name
  size_t token_length;            // Token's length
  size_t start;                   // Start position in source code
  size_t end  ;                   // End position in source code
  // Actual value
  std::int32_t int_value;         // If token is a TK_INTEGER , then the actual value
  double real_value;              // If token is a TK_REAL , then the actual value
  zone::String* str_value;        // If token is a TK_IDENTIFIER/TK_STRING , then the actual string value
  std::string error_description;  // If token is a TK_ERROR , then this is a error description

  Lexeme():
    token(),
    token_length(0),
    start(0),
    end(0),
    int_value(0),
    real_value(0.0),
    str_value(NULL),
    error_description()
  {}
};


/**
 * Tokenizer/Scanner/Lexer , whatever you call it. It is the piece of
 * code that chop the word from the input character stream. The core
 * data structure is lexeme which holds all the attribute value for a
 * certain token . Lexeme represents all string value as zone::String
 * which means parser can directly steal the pointer of zone::String
 */

class Lexer {
 public:
  static const size_t kMaximumIdentifierSize = 256;
  static const size_t kMaximumStringLiteralSize = 1024;
  static_assert( kMaximumIdentifierSize < zone::Zone::kMaximum );
  static_assert( kMaximumStringLiteralSize < zone::Zone::kMaximum );

  static bool IsIdInitChar( char c ) {
    return c == '_' || std::isalpha(c);
  }

  static bool IsIdRestChar( char c ) {
    return IsIdInitChar(c) || std::isdigit(c);
  }

  static std::string EscapeStringLiteral( const zone::String& );

 public:
  inline Lexer( zone::Zone* zone , const char* source );

  zone::Zone* zone() const { return zone_; }
  const char* source() const { return source_; }
  size_t position() const { return position_; }
  const Lexeme& lexeme() const { return lexeme_; }

 public:

  /**
   * Grab the next token starting from the *position_* cursor and then
   * store the lexeme into the *lexeme_* field
   */
  const Lexeme& Next();

  /**
   * Check if the *current lexeme* is pointed to the token *tk*. If so,
   * move the current cursor to next position and return true ; otherwise
   * do nothing returns false
   */
  inline bool Expect( const Token& tk );

  /**
   * Move the cursor one token ahead and check if the new token is the
   * specified token *tk*
   */
  inline bool Try   ( const Token& tk );

 private:
  inline const Lexeme& Set( const Token& tk , size_t length );
  inline const Lexeme& Predicate( char , const Token& , const Token& );
  inline const Lexeme& Predicate( char , const Token& );
  inline const Lexeme& Error( const char* format , ... );

 private:
  void SkipComment();
  const Lexeme& LexNumber();
  const Lexeme& LexString();
  const Lexeme& LexKeywordOrId();
  const Lexeme& LexId();

 private:
  zone::Zone* zone_;                 // Zone allocator
  const char* source_;               // Source code
  size_t position_;                  // Current position/cursor
  Lexeme lexeme_;                    // Previous lexeme
};

inline Lexer::Lexer( zone::Zone* zone , const char* source ):
  zone_(zone),
  source_(source),
  position_(0),
  lexeme_()
{}

inline const Lexeme& Lexer::Error( const char* format , ... ) {
  va_list vl; va_start(vl,format);
  lexeme_.token = Token::kError;
  lexeme_.token_length = 0;
  ReportError(&(lexeme_.error_description),"lexer",source_,
                                                   position_,
                                                   position_,
                                                   format,
                                                   vl);
  return lexeme_;
}

inline const Lexeme& Lexer::Set( const Token& tk , size_t length ) {
  lexeme_.token = tk;
  lexeme_.token_length = length;
  lexeme_.start = position_;
  lexeme_.end = position_ + length;
  position_ += length;
  return lexeme_;
}

inline const Lexeme& Lexer::Predicate( char c , const Token& tk1 , const Token& tk2 ) {
  int nc = source_[position_+1];
  if(nc == c) {
    return Set(tk2,2);
  } else {
    return Set(tk1,1);
  }
}

inline const Lexeme& Lexer::Predicate( char c , const Token& tk ) {
  if(source_[position_+1] == c)
    return Set(tk,2);
  return Error("unrecognize token starting from %c ,do you mean %s ?",c,tk.token_name());
}

inline bool Lexer::Expect( const Token& tk ) {
  if( lexeme_.token == tk ) {
    Next();
    return true;
  } else {
    return false;
  }
}

inline bool Lexer::Try( const Token& tk ) {
  return Next().token == tk;
}

} // namespace parser
} // namespace lavascript

#endif // PARSER_LEXER_H_
