#include "lexer.h"

#include <src/zone/zone.h>
#include <src/zone/string.h>

#include <src/util.h>
#include <src/trace.h>
#include <limits>

namespace lavascript {
namespace parser {

const Lexeme& Lexer::Next() {
  do {
    int c = source_[position_];
    switch(c) {
      case 0  : return Set( Token::kEof , 0 );
      case '+': return Set( Token::kAdd , 1 );
      case '-': return Set( Token::kSub , 1 );
      case '*': return Set( Token::kMul , 1 );
      case '/':
        {
          int nc = source_[position_+1];
          if(nc == '/') { SkipComment(); continue; }
          return Set( Token::kDiv, 1 );
        }
      case '%': return Set( Token::kMod , 1 );
      case '^': return Set( Token::kPow , 1 );
      case '>': return Predicate( '=' , Token::kGT , Token::kGE );
      case '<': return Predicate( '=' , Token::kLT , Token::kLE );
      case '=': return Predicate( '=' , Token::kAssign, Token::kEQ );
      case '!': return Predicate( '=' , Token::kNot , Token::kNE );
      case '&': return Predicate( '&' , Token::kAnd );
      case '|': return Predicate( '|' , Token::kOr  );
      case '?': return Set( Token::kQuestion , 1 );
      case ':': return Set( Token::kColon , 1 );
      case ';': return Set( Token::kSemicolon , 1 );
      case ',': return Set( Token::kComma , 1 );
      case '.': return Predicate( '.' , Token::kDot , Token::kConcat );
      case '[': return Set( Token::kLSqr, 1 );
      case ']': return Set( Token::kRSqr, 1 );
      case '(': return Set( Token::kLPar, 1 );
      case ')': return Set( Token::kRPar, 1 );
      case '{': return Set( Token::kLBra, 1 );
      case '}': return Set( Token::kRBra, 1 );
      case '"': return LexString();
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
        return LexNumber();
      case '\n':
        ++position_;
        continue;
      case '\r':case '\t':case '\f':case '\b': case ' ':
        ++position_;
        continue;
      default:
        return LexKeywordOrId();
    }
  } while(true);

  lava_unreach("Lexer::Next()");
  return lexeme_;
}

void Lexer::SkipComment() {
  int c;
  lava_assert( source_[position_] == '/' && source_[position_+1]=='/' , "" );

  position_ += 2; // Skip first //

  while((c = source_[position_++]))
    if(c == '\n') break;
  if(!c) {
    --position_;
  }
}

namespace {

// Helper class for handling string to *double* or *integer* via normal
// C functions strtol and strtod

bool Str2Int( const std::string& str , std::int32_t* value ) {
  char* pend = NULL;
  errno = 0;
  long int v = ::strtol( str.c_str() , &pend , 10 );

  static_assert( sizeof(long int) >= sizeof(std::int32_t) );

  if(errno || pend != (str.c_str()+str.size())) {
    return false; // Overflow
  } else {
    if( sizeof(long int) == sizeof(std::int32_t) ) {
      *value = v;
      return true;
    } else {
      // Okay ,target machine has long int larger than a int
      long int min = static_cast<long int>(
          std::numeric_limits<std::int32_t>::min());

      long int max = static_cast<long int>(
          std::numeric_limits<std::int32_t>::max());

      if( v < min || v > max ) return false; // Overflow

      *value = v;
      return true;
    }
  }
}


bool Str2Double( const std::string& str , double* value ) {
  char* pend = NULL;
  errno = 0;
  *value = ::strtod( str.c_str() , &pend );
  if( errno || pend != (str.c_str() + str.size()) )
    return false;
  return true;
}

} // namespace

const Lexeme& Lexer::LexNumber() {
  /*
   * Lexing number from :
   *   1) integer
   *   2) real number's floating point rerepsentation
   *
   *
   * Here we do a cheap implementation by using strtol and strtod. This is
   * eaiser for us to do the number lexing here. What we do is we simply
   * scan the source code and find all the character belongs to the number
   * representation. Then we call strtod/strtol accordingly to figure out
   * the actual number value
   */
  enum {
    kNeedEndOrDigitOrDot,
    kNeedDigit,
    kNeedEndOrDigit
  };

  int st = kNeedEndOrDigitOrDot;
  int c;
  int start = position_;
  std::string buffer;

  buffer.push_back( source_[position_] );

  for( position_++ ; ; ++position_ ) {
    c = source_[position_];
    switch(st) {
      case kNeedEndOrDigitOrDot :
        if(c == '.') {
          st = kNeedDigit;
          buffer.push_back('.');
          continue;
        }
        if(std::isdigit(c)) {
          buffer.push_back(c);
          continue;
        }
        goto done;
      case kNeedDigit:
        if(std::isdigit(c)) {
          st = kNeedEndOrDigit;
          buffer.push_back(c);
          continue;
        }
        --position_; // Put back the previous "." character
        goto done;
      case kNeedEndOrDigit:
        if(std::isdigit(c)) {
          buffer.push_back(c);
          continue;
        }
        goto done;
      default: break;
    }
  }

done:
  switch(st) {
    case kNeedDigit: buffer.pop_back(); // fallthrough
    case kNeedEndOrDigitOrDot:
      if(!Str2Int(buffer,&(lexeme_.int_value)))
        return Error("integer literal %s overflow!",buffer.c_str());
      lexeme_.token = Token::kInteger;
      break;
    default:
      if(!Str2Double(buffer,&(lexeme_.real_value)))
        return Error("real literal %s overflow!",buffer.c_str());
      lexeme_.token = Token::kReal;
      break;
  }

  lexeme_.start = start;
  lexeme_.token_length = (position_ - start);
  lexeme_.end = position_;
  return lexeme_;
}

const Lexeme& Lexer::LexString() {
  lava_assert( source_[position_] == '"' , "" );
  int c;
  size_t start = position_;
  std::string buffer;
  buffer.reserve(32);

  for( position_ ++ ; (c = source_[position_]) ; ++position_ ) {
    switch(c) {
      case '\\':
        {
          int nc = source_[position_+1];
          switch(nc) {
            case 'n': buffer.push_back('\n'); ++position_; break;
            case 't': buffer.push_back('\t'); ++position_; break;
            case 'r': buffer.push_back('\r'); ++position_; break;
            case 'f': buffer.push_back('\f'); ++position_; break;
            case 'b': buffer.push_back('\b'); ++position_; break;
            case '"': buffer.push_back('\"'); ++position_; break;
            case '\\':buffer.push_back('\\'); ++position_; break;
            default:  buffer.push_back('\\'); break;
          }
        }
        break;
      case '"': ++position_; goto done;
      /* disallowed characters */
      case '\n': case '\r': case '\t': case '\f': case '\b':
        return Error("string literal cannot contain special characters "
                     "with ansic code %d",(int)c);
      default: buffer.push_back(c); break;
    }

    if(buffer.size() > kMaximumStringLiteralSize) {
      return Error("string literal are too long,cannot be longer than %zu",
          kMaximumStringLiteralSize);
    }
  }

  lava_assert( !c , "reaching here must be string is terminated by EOF" );
  return Error("string literal not closed by \" properly");

done:

  lexeme_.token = Token::kString;
  lexeme_.start = start;
  lexeme_.end = position_;
  lexeme_.token_length = (position_ - start);
  lexeme_.str_value = ::lavascript::zone::String::New( zone_ , buffer );

  return lexeme_;
}

namespace {

/**
 * Unrolled comparison function for comparing the keyword
 */

template< size_t N > bool CompareKeyword( const char* L , const char (&R)[N] ) {
  for( size_t i = 0 ; i < N - 1 ; ++i ) {
    if( L[i] != R[i] ) return false;
  }
  return !Lexer::IsIdRestChar( L[N-1] );
}

} // namespace


const Lexeme& Lexer::LexKeywordOrId() {
  int lookahead = source_[position_];

#define CP(X) CompareKeyword(source_+position_+1,(X))

  /**
   * The following *unrolled* string comparison code is for checking
   * keyword. Any new updating keyword needs to be put in the following
   * large switch check case accordingly
   */

  switch(lookahead) {
    case 'b':
      if(CP("reak")) return Set(Token::kBreak,5);
      break;
    case 'c':
      if(CP("ontinue")) return Set(Token::kContinue,8);
      break;
    case 'e':
      if(CP("lif")) return Set(Token::kElif,4);
      if(CP("lse")) return Set(Token::kElse,4);
      break;
    case 'f':
      if(CP("or")) return Set(Token::kFor,3);
      if(CP("unction")) return Set(Token::kFunction,8);
      if(CP("alse")) return Set(Token::kFalse,5);
      break;
    case 'i':
      if(CP("f")) return Set(Token::kIf,2);
      if(CP("n")) return Set(Token::kIn,2);
      break;
    case 'n':
      if(CP("ull")) return Set(Token::kNull,4);
      break;
    case 'r':
      if(CP("eturn")) return Set(Token::kReturn,6);
      break;
    case 't':
      if(CP("rue")) return Set(Token::kTrue,4);
      break;
    case 'v':
      if(CP("ar")) return Set(Token::kVar,3);
      break;
    default:
      break;
  }

#undef CP // CP

  if(IsIdInitChar(lookahead))
    return LexId();

  return Error("unknown character %c ",lookahead);
}

const Lexeme& Lexer::LexId() {
  size_t start = position_;
  std::string buffer;
  buffer.reserve(32);

  buffer.push_back( source_[position_] );

  for( ++position_ ; ; ++position_ ) {
    int c = source_[position_];
    if(!IsIdRestChar(c)) break;
    buffer.push_back(c);
    if(buffer.size() > kMaximumIdentifierSize) {
      return Error("identifier are not long, cannot be longer than %zu",
          kMaximumIdentifierSize);
    }
  }

  lexeme_.start = start;
  lexeme_.end = position_;
  lexeme_.token_length = (position_ - start);
  lexeme_.token = Token::kIdentifier;
  lexeme_.str_value = ::lavascript::zone::String::New(zone_,buffer);
  return lexeme_;
}

std::string Lexer::EscapeStringLiteral( const zone::String& str ) {
  std::string buffer;
  buffer.reserve(str.size());

  for( size_t i = 0 ; i < str.size() ; ++i ) {
    char c = str[i];
    switch(c) {
      case 'n': buffer.append("\\n"); break;
      case 't': buffer.append("\\t"); break;
      case 'r': buffer.append("\\r"); break;
      case 'f': buffer.append("\\f"); break;
      case 'b': buffer.append("\\b"); break;
      case '"': buffer.append("\\\"");break;
      case '\\':buffer.append("\\\\");break;
      default: buffer.push_back(c);
    }
  }
  return buffer;
}


} // namespace parser
} // namespace lavascript
