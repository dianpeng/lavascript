#ifndef PARSER_TOKEN_H_
#define PARSER_TOKEN_H_

namespace lavascript {
namespace parser {

/**
 * Do NOT modify the order of tokens and insert new tokens inside
 * of the token table , append the token at very last of each section.
 *
 * Additionally, if new tokens added for 1) Arithmetic 2) Comparison and
 * 3) Logic Operators . Please make sure that the parser's precedence
 * table is updated accordingly
 */

#define LAVA_TOKEN_LIST(__) \
  /* Arithmetic Operators */ \
  __( TK_ADD , "+" , + , Add , ARITHMETIC ) \
  __( TK_SUB , "-" , - , Sub , ARITHMETIC ) \
  __( TK_MUL , "*" , * , Mul , ARITHMETIC ) \
  __( TK_DIV , "/" , / , Div , ARITHMETIC ) \
  __( TK_MOD , "%" , % , Mod , ARITHMETIC ) \
  __( TK_POW , "^" , ^ , Pow , ARITHMETIC ) \
  /* Comparison Operators */ \
  __( TK_LT  , "<" , < , LT , COMPARISON ) \
  __( TK_LE  , "<=", <=, LE , COMPARISON ) \
  __( TK_GT  , ">" , > , GT , COMPARISON ) \
  __( TK_GE  , ">=", >=, GE , COMPARISON ) \
  __( TK_EQ  , "==", ==, EQ , COMPARISON ) \
  __( TK_NE  , "!=", !=, NE , COMPARISON ) \
  /* Logic Operators */ \
  __( TK_AND , "&&", &&, And, LOGIC) \
  __( TK_OR  , "||", ||, Or , LOGIC) \
  __( TK_NOT , "!" , ! , Not, LOGIC) \
  /* Misc */ \
  __( TK_QUESTION,"?",_,Question, MISC ) \
  __( TK_COLON   ,":",_,Colon   , MISC ) \
  __( TK_COMMA   ,",",_,Comma   , MISC ) \
  __( TK_SEMICOLON,";",_,Semicolon,MISC) \
  __( TK_LSQR , "[" , _ , LSqr, MISC ) \
  __( TK_RSQR , "]" , _ , RSqr, MISC ) \
  __( TK_LPAR , "(" , _ , LPar, MISC ) \
  __( TK_RPAR , ")" , _ , RPar, MISC ) \
  __( TK_LBRA , "{" , _ , LBra, MISC ) \
  __( TK_RBRA , "}" , _ , RBra, MISC ) \
  __( TK_DOT  , "." , _ , Dot , MISC ) \
  __( TK_IDENTIFIER , "identifier" , _ , Identifier , MISC ) \
  __( TK_ASSIGN,"=", _ , Assign, MISC) \
  __( TK_CONCAT,"..",_ , Concat, MISC) \
  /* Keyword */ \
  __( TK_IF , "if" , _ , If , KEYWORD ) \
  __( TK_ELIF,"elif",_ ,Elif, KEYWORD ) \
  __( TK_ELSE,"else",_ ,Else, KEYWORD ) \
  __( TK_FOR ,"for" ,_ ,For , KEYWORD ) \
  __( TK_BREAK,"break",_,Break,KEYWORD) \
  __( TK_CONTINUE,"continue",_,Continue,KEYWORD) \
  __( TK_RETURN,"return",_,Return,KEYWORD) \
  __( TK_VAR , "var", _,Var,KEYWORD) \
  __( TK_FUNCTION,"function",_,Function,KEYWORD) \
  __( TK_IN , "in", _, In, KEYWORD) \
  /* Literal */ \
  __( TK_TRUE , "true" , _ , True, LITERAL ) \
  __( TK_FALSE, "false", _ , False,LITERAL ) \
  __( TK_NULL , "null",  _ , Null ,LITERAL ) \
  __( TK_REAL , "real" , _ , Real , LITERAL) \
  __( TK_STRING,"string",_ , String,LITERAL) \
  /* Others */ \
  __( TK_ERROR , "error", _ , Error  , STATUS ) \
  __( TK_EOF   , "eof"  , _ , Eof , STATUS )


/**
 * A simple object represents a token in the lexer. The related attribute (lexeme)
 * are stored in class Lexeme. The token is sololy a integer tells what token type is
 */
class Token {
 public:
  // Static token instance for easy manipulation
#define __(A,B,C,D,E) static Token k##D;
   LAVA_TOKEN_LIST(__)
#undef __ // __

  // Tokens enumeration value
#define __(A,B,C,D,E) A ,
  enum {
    LAVA_TOKEN_LIST(__)
    SIZE_OF_TOKENS
  };
#undef __ // __

  // Token Type
  enum {
    ARITHMETIC ,                 // Arithmetic tokens
    COMPARISON ,                 // Comparison tokens
    LOGIC,                       // Logic tokens
    MISC,                        // Misc tokens ( punction characters )
    KEYWORD,                     // Keyword tokens
    LITERAL,                     // Literal tokens
    STATUS                       // Status tokens for Lexer object
  };

 public: // Public static interfaces for token value
  static int GetTokenType( int token );
  static const char* GetTokenName( int token );

 public:
  Token() : token_( TK_ERROR ) {}
  explicit Token( int tk ): token_(tk) {}
  Token( const Token& tk ): token_(tk.token_) {}
  Token& operator = ( const Token& tk ) { token_ = tk.token_; return *this; }

 public:
  int token_type() const { return GetTokenType(token_); }
  const char* token_name() const { return GetTokenName(token_); }

  bool IsArithmetic() const { return token_type() == ARITHMETIC; }
  bool IsComparison()const { return token_type() == COMPARISON; }
  bool IsLogic() const { return token_type() == LOGIC; }
  bool IsMisc() const { return token_type() == MISC; }
  bool IsLiteral() const { return token_type() == LITERAL; }
  bool IsStatus() const  { return token_type() == STATUS; }
  bool IsPrefixOperator() const {
    return token_ == TK_DOT || token_ == TK_LSQR || token_ == TK_LPAR;
  }
  bool IsBinaryOperator() const {
    return IsArithmetic() ||
           IsComparison() ||
           IsConcat() ||
           token_ == TK_AND ||
           token_ == TK_OR;
  }
  bool IsUnaryOperator () const {
    return token_ == TK_SUB || token_ == TK_NOT;
  }

 public:
  bool operator == ( const Token& tk ) const {
    return token_ == tk.token_;
  }
  bool operator == ( int tk ) const {
    return token_ == tk;
  }

  bool operator != ( const Token& tk ) const {
    return token_ != tk.token_;
  }
  bool operator != ( int tk ) const {
    return token_ != tk;
  }

 public:
#define __(A,B,C,D,E) \
  bool Is##D() const { return token_ == A; }
  LAVA_TOKEN_LIST(__)
#undef __ // __

 public:
  int token() const { return token_; }
  operator int () const { return token_; }

 private:
  int token_;
};

} // namespace parser
} // namespace lavascript

#endif // PARSER_TOKEN_H_
