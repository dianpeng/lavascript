#ifndef AST_FACTORY_H_
#define AST_FACTORY_H_
#include <src/core/util.h>
#include <src/zone/zone.h>
#include <src/zone/string.h>
#include <src/parser/lexer.h>
#include <map>

#include "ast.h"

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
 public:
  AstFactory( ::lavascript::zone::Zone* zone ): zone_(zone) {}

  inline Literal* NewLiteral( size_t start , size_t end , size_t len , int ival );
  inline Literal* NewLiteral( const Lexer& l , int ival );

  inline Literal* NewLiteral( size_t start , size_t end , size_t len , double rval );
  inline Literal* NewLiteral( const Lexer& l , double rval );

  inline Literal* NewLiteral( size_t start , size_t end , size_t len , bool bval );
  inline Literal* NewLiteral( const Lexer& l , bool bval );

  inline Literal* NewLiteral( size_t start , size_t end , size_t len );
  inline Literal* NewLiteral( const Lexer& l );

  inline Literal* NewLiteral( size_t start , size_t end , size_t len , ::lavascript::zone::String* );
  inline Literal* NewLiteral( const Lexer& l , ::lavascript::zone::String* )Node;

  inline Variable* NewVariable( size_t start , size_t end , ::lavascript::zone::String* );
  inline Variable* NewVariable( const Lexer& l , ::lavascript::zone::String* );

  inline FuncCall* NewFuncCall( size_t start , size_t end , ::lavascript::zone::Vector<Node*>* );
  inline FuncCall* NewFuncCall( const Lexer& l , ::lavascript::zone::Vector<Node*>* );

  inline Prefix* NewPrefix( size_t start , size_t end ,
                            ::lavascript::zone::Vector<Prefix::Component>* , Node* );

  inline Prefix* NewPrefix( const Lexer& l ,
                            ::lavascript::zone::Vector<Prefix::Component>* , Node* );

  inline Binary* NewBinary( size_t start , size_t end ,size_t ,Token ,Node* , Node* );
  inline Binary* NewBinary( const Lexer& l , size_t , Token ,Node* , Node* );

  inline Unary* NewUnary( size_t start , size_t end , size_t , Token , Node* );
  inline Unary* NewUnary( const Lexer& , size_t , Token , Node* );

  inline Ternary* NewTernary( size_t start , size_t end , size_t , size_t ,
      Node* , Node* , Node* );
  inline Ternary* NewTernary( const Lexer& , size_t , size_t , Node* , Node* , Node* );

  inline List* NewList( size_t start , size_t end , ::lavascript::zone::Vector<Node*>* a = NULL );

  inline Object* NewObject( size_t start , size_t end ,
      ::lavascript::zone::Vector<Object::Entry>* a = NULL );

  inline Var* NewVar( size_t start , size_t end , Variable* v , Node* e );

  inline Assign* NewAssign( size_t start , size_t end , Variable* , Node* );
  inline Assign* NewAssign( size_t start , size_t end , Prefix* , Node* );

  inline Call* NewCall( size_t start , size_t end , Prefix* );

  inline If*   NewIf  ( size_t start , size_t end , ::lavascript::zone::Vector<If::Branch>* );

  inline For*  NewFor ( size_t start , size_t end , Variable* ,
                                                    Node* ,
                                                    Node* ,
                                                    Node* ,
                                                    Chunk* );


  inline ForEach* NewForEach( size_t start , size_t end , Variable* ,
                                                          Node*,
                                                          Chunk* );

  inline Break* NewBreak( size_t start , size_t end );
  inline Continue* NewContinue( size_t start , size_t end );
  inline Return* NewReturn( size_t start , size_t end , Node* );

  inline Require* NewRequire( size_t start , size_t end , size_t , size_t ,
                                                                   Node* ,
                                                                   Variable* );
  inline Chunk* NewChunk( size_t , size_t , ::lavascript::zone::Vector<Node*>* );

  inline Function* NewFunction( size_t , size_t , size_t , Variable* ,
                                                           ::lavascript::zone::Vector<Variable*>*,
                                                           Chunk* );

  inline Root* NewRoot( size_t , size_t , Chunk* );

 private:
  ::lavascript::zone::Zone* zone_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(AstFactory);
};

inline Literal* AstFactory::NewLiteral( size_t start , size_t end , size_t len , int ival ) {
  return new (zone_) Literal(start,end,len,ival);
}

inline Literal* AstFactory::NewLiteral( const Lexer& l , int ival ) {
  return NewLiteral( l.lexeme().start , l.lexeme().end , l.lexeme().token_length , ival );
}

inline Literal* AstFactory::NewLiteral( size_t start , size_t end , size_t len , double rval ) {
  return new (zone_) Literal(start,end,len,rval);
}

inline Literal* AstFactory::NewLiteral( const Lexer& l , double rval ) {
  return NewLiteral( l.lexeme().start , l.lexeme().end , l.lexeme().token_length , rval );
}

inline Literal* AstFactory::NewLiteral( size_t start , size_t end , size_t len , bool bval ) {
  return new (zone_) Literal(start,end,len,bval);
}

inline Literal* AstFactory::NewLiteral( const Lexer& l , bool bval ) {
  return NewLiteral( l.lexeme().start, l.lexeme().end, l.lexeme().token_length, bval);
}

inline Literal* AstFactory::NewLiteral( size_t start , size_t end , size_t len ) {
  return new (zone_) Literal(start,end,len);
}

inline Literal* AstFactory::NewLiteral( const Lexer& l ) {
  return NewLiteral( l.lexeme().start , l.lexeme().end , l.lexeme().token_length );
}

inline Literal* AstFactory::NewLiteral( size_t start , size_t end , size_t len ,
    ::lavascript::zone::String* str ) {
  return new (zone_) Literal(start,end,len,str);
}

inline Literal* AstFactory::NewLiteral( const Lexer& l , ::lavascript::zone::String* str ) {
  return NewLiteral( l.lexeme().start , l.lexeme().end , l.lexeme().token_length , str );
}

inline Variable* AstFactory::NewVariable( size_t start , size_t end , size_t len ,
    ::lavascript::zone::String* v ) {
  return new (zone_) Variable(start,end,v);
}

inline Variable* AstFactory::NewVariable( const Lexer& l , ::lavascript::zone::String* v ) {
  return NewVariable(l.lexeme().start,l.lexeme().end,v);
}

inline FuncCall* AstFactory::NewFuncCall( size_t start , size_t end ,
    ::lavascript::zone::Vector<Node*>* arg ) {
  return new (zone_) FuncCall(start,end,arg);
}

inline FuncCall* AstFactory::NewFuncCall( const Lexer& l , ::lavascript::zone::Vector<Node*>* arg ) {
  return NewFuncCall( l.lexeme().start , l.lexeme().end , arg );
}

inline Prefix* AstFactory::NewPrefix( size_t start , size_t end ,
    ::lavascript::zone::Vector<Prefix::Component>* l , Node* v ) {
  return new (zone_) Prefix(start,end,l,v);
}

inline Prefix* AstFactory::NewPrefix( const Lexer& l ,
    ::lavascript::zone::Vector<Prefix::Component>* list , Node* v ) {
  return NewPrefix(l.lexeme().start,l.lexeme().end,list,v);
}

inline Binary* AstFactory::NewBinary( size_t start , size_t end , size_t opp ,
                                                                  Token op ,
                                                                  Node* l  ,
                                                                  Node* r  ) {
  return new (zone_) Binary(start,end,opp,op,l,r);
}

inline Binary* AstFactory::NewBinary( const Lexer& l , size_t opp , Token op , Node* lhs ,
                                                                               Node* rhs ) {
  return NewBinary(l.lexeme().start,l.lexeme().end,opp,op,lhs,rhs);
}

inline Unary* AstFactory::NewUnary( size_t start , size_t end , size_t opp , Token o ,
                                                                             Node* opr ) {
  return new (zone_) Unary(start,end,opp,o,opr);
}

inline Unary* AstFactory::NewUnary( const Lexer& l , size_t opp , Token o , Node* opr ) {
  return NewUnary(l.lexeme().start,l.lexeme().end,opp,o,opr);
}

inline Ternary* AstFactory::NewTernary( size_t sp , size_t ep , size_t qp , size_t cp ,
                                                                            Node* first,
                                                                            Node* second,
                                                                            Node* third ) {
  return new (zone_) Ternary(sp,ep,qp,cp,first,second,third);
}

inline Ternary* AstFactory::NewTernary( const Lexer& l , size_t qp ,size_t cp , Node* first ,
                                                                                Node* second,
                                                                                Node* third ) {
  return NewTernary(l.lexeme().start,l.lexeme().end,qp,cp,first,second,third);
}

inline List* AstFactory::NewList( size_t start , size_t end , ::lavascript::zone::Vector<Node*>* entry ) {
  if(!entry) {
    // We don't use any NULL pointer to represent an empty collection but allocate
    // an empty vector here, though it costs more memory but it is easy to maintain
    // the code
    entry = ::lavascript::zone::Vector<Node*>::New(zone_);
  }
  return new (zone_) List(start,end,entry);
}

inline Object* AstFactory::NewObject( size_t start, size_t end,
    ::lavascript::zone::Vector<Object::Entry>* entry ) {
  if(!entry) {
    entry = ::lavascript::zone::Vector<Object::Entry>::New(zone_);
  }
  return new (zone_) Object(start,end,entry);
}

inline Var* AstFactory::NewVar( size_t start , size_t end , Variable* v , Node* e ) {
  return new (zone_) Var(start,end,v,e);
}

inline Assign* AstFactory::NewAssign( size_t start , size_t end , Variable* lv, Node* r ) {
  return new (zone_) Assign(start,end,lv,r);
}

inline Assign* AstFactory::NewAssign( size_t start , size_t end , Prefix* lp , Node* r ) {
  return new (zone_) Assign(start,end,lp,r);
}

inline Call* AstFactory::NewCall( size_t start , size_t end , Prefix* c ) {
  return new (zone_) Call(start,end,c);
}

inline If* AstFactory::NewIf( size_t start , size_t end , ::lavascript::zone::Vector<If::Branch>* bl ) {
  return new (zone_) If(start,end,bl);
}

inline For* AstFactory::NewFor( size_t start , size_t end , Variable* v , Node* first ,
                                                                          Node* second,
                                                                          Node* third,
                                                                          Chunk* b) {
  return new (zone_) For(start,end,v,first,second,third,b);
}

inline ForEach* AstFactory::NewForEach( size_t start , size_t end , Variable* v ,
                                                                    Node* i,
                                                                    Chunk* b ) {
  return new (zone_) ForEach(start,end,v,i,b);
}

inline Break* AstFactory::NewBreak( size_t start , size_t end ) {
  return new (zone_) Break(start,end);
}

inline Continue* AstFactory::NewContinue( size_t start , size_t end ) {
  return new (zone_) Continue(start,end);
}

inline Return* AstFactory::NewReturn( size_t start , size_t end , Node* e ) {
  return new (zone_) Return(start,end,e);
}

inline Require* AstFactory::NewRequire( size_t start , size_t end , size_t rp , size_t ap ,
                                                                                Node* re,
                                                                                Variable* av ) {
  return new (zone_) Require(start,end,rp,ap,re,av);
}

inline Chunk* AstFactory::NewChunk( size_t start , size_t end , ::lavascript::zone::Vector<Node*>* b ) {
  return new (zone_) Chunk(start,end,b);
}

inline Function* AstFactory::NewFunction( size_t start , size_t end , size_t fp , Variable* n ,
                ::lavascript::zone::Vector<Variable*>* p,Chunk* b ) {
  return new (zone_) Function(start,end,fp,n,p,b);
}

inline Root* AstFactory::NewRoot( size_t start , size_t end , Chunk* chunk ) {
  return new (zone_) Root(start,end,chunk);
}

} // namespace ast
} // namespace parser
} // namespace lavascript
#endif // AST_FACTORY_H_
