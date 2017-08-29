#ifndef AST_FACTORY_H_
#define AST_FACTORY_H_
#include <src/core/util.h>
#include <src/zone/zone.h>
#include <src/zone/string.h>
#include <src/parser/lexer.h>
#include <map>

namespace lavascript {
namespace parser {
namespace ast {

/**
 * A helper factory class to effciently manage AST nodes allocation
 * on an Zone object.
 *
 * It mainly tries to dedup literal node and also simplify the interface
 * to create most of the Ast node
 */

class AstFactory {
  typedef std::map< int , ast::Literal* > IntLiteralMap;
  typedef std::map< double , ast::Literal* > RealLiteralMap;
 public:
  AstFactory( ::lavascript::zone::Zone* zone ):
    zone_(zone),
    int_literal_map_() ,
    real_literal_map_()
  {}

 public: /** Factory function **/
  inline Literal* NewLiteral( size_t start , size_t end , size_t len , int ival );
  inline Literal* NewLiteral( const Lexer& l , int ival );

  inline Literal* NewLiteral( size_t start , size_t end , size_t len , double rval );
  inline Literal* NewLiteral( const Lexer& l , double rval );

  inline Literal* NewLiteral( size_t start , size_t end , size_t len , bool bval );
  inline Literal* NewLiteral( const Lexer& l , bool bval );

  inline Literal* NewLiteral( size_t start , size_t end , size_t len );
  inline Literal* NewLiteral( const Lexer& l );

  inline Literal* NewLiteral( size_t start , size_t end , size_t len , ::lavascript::zone::String* );
  inline Literal* NewLiteral( const Lexer& l , ::lavascript::zone::String* );

  inline Variable* NewVariable( size_t start , size_t end , size_t len );
  inline Variable* NewVariable( const Lexer& l );

  inline Binary* NewBinary( size_t start , size_t end );
  inline Binary* NewBinary( const Lexer& l );


 private:
  ::lavascript::zone::Zone* zone_;
  IntLiteralMap int_literal_map_;
  RealLiteralMap real_literal_map_;
  Literal* lit_true_;
  Literal* lit_false_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(AstFactory);
};

inline Literal* AstFactory::NewLiteral( size_t start , size_t end , size_t len , int ival ) {
  IntLiteralMap::iterator itr = int_literal_map_.find(ival);
  if( itr == int_literal_map_.end() ) {
    Literal* lit = new (zone_->Malloc<ast::Literal>()) ast::Literal(start,end,len,ival);
    int_literal_map_.insert(std::make_pair(ival,lit));
    return lit;
  } else {
    return itr->second;
  }
}

inline Literal* AstFactory::NewLiteral( const Lexer& l , int ival ) {
  return NewLiteral( l.current().start , l.current().end , l.current().token_length , ival );
}

} // namespace lavascript
} // namespace ast
#endif // AST_FACTORY_H_
