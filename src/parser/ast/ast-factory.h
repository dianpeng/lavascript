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
  inline Literal* NewLiteral( const Lexer& l , ::lavascript::zone::String* );

  inline Variable* NewVariable( size_t start , size_t end , size_t len ,
      ::lavascript::zone::String* );
  inline Variable* NewVariable( const Lexer& l , ::lavascript::zone::String* );

  inline FuncCall* NewFuncCall( size_t start , size_t end , ::lavascript::zone::Vector<Node*>* );
  inline FuncCall* NewFuncCall( const Lexer& l , ::lavascript::zone::Vector<Node*>* );

  inline Prefix* NewPrefix( size_t start , size_t end , ::lavascript::zone::Vector<Prefix::Component*>* ,
                                                        Variable* );
  inline Prefix* NewPrefix( const Lexer& l , ::lavascript::zone::Vector<Prefix::Component*>* ,
                                             Variable* );

  inline Binary* NewBinary( size_t start , size_t end ,size_t , Token ,
      Node* , Node* );

  inline Binary* NewBinary( const Lexer& l , size_t end , size_t , Token ,
      Node* , Node* );

  inline Unary* NewUnary( size_t start , size_t end , size_t , Token , Node* );
  inline Unary* NewUnary( const Lexer& , size_t , Token , Node* );

  inline Ternary* NewTernary( size_t start , size_t end , size_t , size_t ,
      Node* , Node* , Node* );
  inline Ternary* NewTernary( const Lexer& , size_t , size_t , Node* , Node* , Node* );

  inline List* NewList( size_t start , size_t end , ::lavascript::zone::Vector<Node*>* );
  inline List* NewList( const Lexer& , ::lavascript::zone::Vector<Node*>* );

  inline Object* NewObject( size_t start , size_t end , ::lavascript::zone::Vector<Object::Entry>* );
  inline Object* NewObject( const Lexer& l , ::lavascript::zone::Vector<Object::Entry>* );

  inline Var* NewVar( size_t start , size_t end , Variable* v , Node* e );
  inline Var* NewVar( const Lexer& l , Variable* , Node* );

  inline Assign* NewAssign( size_t start , size_t end , size_t , Variable* ,
                                                                 Prefix*   ,
                                                                 Node*     ,
                                                                 size_t );
  inline Assign* NewAssign( const Lexer& l , size_t , Variable*, Prefix*   ,
                                                                 Node*     ,
                                                                 size_t );

  inline Call* NewCall( size_t start , size_t end , Prefix* );
  inline Call* NewCall( const Lexer& , Prefix* );

  inline If*   NewIf  ( size_t start , size_t end , ::lavascript::zone::Vector<Branch>* );
  inline If*   NewIf  ( const Lexer& , ::lavascript::zone::Vector<Branch>* );

  inline For*  NewFor ( size_t start , size_t end , Variable* ,
                                                    Node* ,
                                                    Node* ,
                                                    Node* ,
                                                    Chunk*,
                                                    size_t );

  inline For*  NewFor ( const Lexer& , Variable* , Node* ,
                                                   Node* ,
                                                   Node* ,
                                                   Chunk*,
                                                   size_t );
                                                                          

  inline ForEach* NewForEach( size_t start , size_t end , Variable* ,
                                                          Node*,
                                                          Chunk*,
                                                          size_t );

  inline ForEach* NewForEach( const Lexer& , Variable* , Node* ,
                                                         Chunk*,
                                                         size_t );

  inline Break* NewBreak( size_t start , size_t end );
  inline Break* NewBreak( const Lexer& );

  inline Continue* NewContinue( size_t start , size_t end );
  inline Continue* NewContinue( const Lexer& );

  inline Return* NewReturn( size_t start , size_t end , size_t , Node* );
  inline Return* NewReturn( const Lexer& , size_t , Node* );

  inline Require* NewRequire( size_t start , size_t end , size_t , size_t ,
                                                                   Node* ,
                                                                   Variable* );
  inline Require* NewRequire( const Lexer& , size_t , size_t , Node* , Variable* );

  inline Chunk* NewChunk( size_t , size_t , ::lavascript::zone::Vector<Node*>* );
  inline Chunk* NewChunk( const Lexer& , ::lavascript::zone::Vector<Node*>* );

  inline Function* NewFunction( size_t , size_t , size_t , Variable* ,
                                                           ::lavascript::zone::Vector<Variable*>*
                                                           Chunk* );

  inline Function* NewFunction( const Lexer& , size_t, Variable* ,
                                                       ::lavascript::zone::Vector<Variable*>*,
                                                       Chunk* );

  inline Root* NewRoot( size_t , size_t , Chunk* );
  inline Root* NewRoot( const Lexer& , Chunk* );


 private:
  ::lavascript::zone::Zone* zone_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(AstFactory);
};

inline Literal* AstFactory::NewLiteral( size_t start , size_t end , size_t len , int ival ) {
  return new (zone_->Malloc<Literal>()) Literal(start,end,len,ival);
}

inline Literal* AstFactory::NewLiteral( const Lexer& l , int ival ) {
  return NewLiteral( l.current().start , l.current().end , l.current().token_length , ival );
}

inline Literal* AstFactory::NewLiteral( size_t start , size_t end , size_t len , double rval ) {
  return new (zone_->Malloc<Literal>()) Literal(start,end,len,rval);
}

inline Literal* AstFactory::NewLiteral( const Lexer& l , double rval ) {
  return NewLiteral( l.current().start , l.current().end , l.current().token_length , rval );
}

inline Literal* AstFactory::NewLiteral( size_t start , size_t end , size_t len , bool bval ) {
  return new (zone_->Malloc<Literal>()) Literal(start,end,len,bval);
}

inline Literal* AstFactory::NewLiteral( const Lexer& l , bool bval ) {
  return NewLiteral( l.current().start, l.current().end, l.current().token_length, bval);
}

inline Literal* AstFactory::NewLiteral( size_t start , size_t end , size_t len ) {
  return new (zone_->Malloc<Literal>()) Literal(start,end,len);
}

inline Literal* AstFactory::NewLiteral( const Lexer& l ) {
  return NewLiteral( l.current().start , l.current().end , l.current().token_length );
}

inline Literal* AstFactory::NewLiteral( size_t start , size_t end , size_t len ,
    ::lavascript::zone::String* str ) {
  return new (zone_->Malloc<Literal>()) Literal(start,end,len,str);
}

inline Literal* AstFactory::NewLiteral( const Lexer& l , ::lavascript::zone::String* str ) {
  return NewLiteral( l.current().start , l.current().end , l.current().token_length , str );
}

inline Variable* AstFactory::NewVariable( size_t start , size_t end , size_t len ,
    ::lavascript::zone::String* v ) {
  return new (zone_->Malloc<Variable>()) Variable(start,end,len,v);
}

inline Variable* AstFactory::NewVariable( const Lexer& l , ::lavascript::zone::String* v ) {
  return NewVariable(l.current().start,l.current().end,l.current().token_length,v);
}

inline FuncCall* AstFactory::NewFuncCall( size_t start , size_t end ,
    ::lavascript::zone::Vector<Node*>* arg ) {
  return new (zone_->Malloc<Variable>()) FuncCall(start,end,arg);
}

inline FuncCall* AstFactory::NewFuncCall( const Lexer& l , ::lavascript::zone::Vector<Node*>* arg ) {
  return NewFuncCall( l.current().start , l.current().end , l.current().token_length , arg );
}

inline Prefix* AstFactory::NewPrefix( size_t start , size_t end ,
    ::lavascript::zone::Vector<Prefix::Component>* l , Variable* v ) {
  return new (zone_->Malloc<Prefix>()) Prefix(start,end,l,v);
}

inline Prefix* AstFactory::NewPrefix( const Lexer& l , ::lavascript::zone::Vector<Prefix::Component>* l ,
                                                       Variable* v ) {
  return NewPrefix(l.current().start,l.current().end,l,v);
}

inline Binary* AstFactory::NewBinary( size_t start , size_t end , size_t opp ,
                                                                  Token op ,
                                                                  Node* l  ,
                                                                  Node* r  ) {
  return new (zone_->Malloc<Binary>()) Binary(start,end,opp,op,l,r);
}

inline Binary* AstFactory::NewBinary( const Lexer& l , size_t opp , Token op , Node* l ,
                                                                               Node* r ) {
  return NewBinary(l.current().start,l.current().end,opp,op,l,r);
}

inline Unary* AstFactory::NewUnary( size_t start , size_t end , size_t opp , Token o ,
                                                                             Node* opr ) {
  return new (zone_->Malloc<Unary>()) Unary(start,end,opp,o,opr);
}

inline Unary* AstFactory::NewUnary( const Lexer& l , size_t opp , Token o , Node* opr ) {
  return NewUnary(l.current().start,l.current().end,opp,o,opr);
}

inline Ternary* AstFactory::NewTernary( size_t sp , size_t ep , size_t qp , size_t cp ,
                                                                            Node* first,
                                                                            Node* second,
                                                                            Node* third ) {
  return new (zone_->Malloc<Ternary>()) Ternary(sp,ep,qp,cp,first,second,third);
}

inline Ternary* AstFactory::NewTernary( const Lexer& l , size_t qp ,size_t cp , Node* first ,
                                                                                Node* second,
                                                                                Node* third ) {
  return NewTernary(l.current().start,l.current().end,qp,cp,first,second,third);
}

inline List* AstFactory::NewList( size_t start , size_t end , ::lavascript::zone::Vector<Node*>* entry ) {
  return new (zone_->Malloc<List>()) List(start,end,entry);
}

inline List* AstFactory::NewList( const Lexer& l , ::lavascript::zone::Vector<Node*>* entry ) {
  return NewList( l.current().start , l.current().end , entry );
}

inline Object* AstFactory::NewObject( size_t start, size_t end,
    ::lavascript::zone::Vector<Object::Entry>* entry ) {
  return new (zone_->Malloc<Object>()) Object(start,end,entry);
}

inline Object* AstFactory::NewObject( const Lexer& l , ::lavascript::zone::Vector<Object::Entry>* entry ) {
  return NewObject(l.current().start,l.current().end,entry);
}

inline Var* AstFactory::NewVar( size_t start , size_t end , Variable* v , Node* e ) {
  return new (zone_->Malloc<Var>()) Var(start,end,v,e);
}

inline Var* AstFactory::NewVar( const Lexer& l , Variable* v , Node* e ) {
  return NewVar(l.current().start,l.current().end,v,e);
}

inline Assign* AstFactory::NewAssign( size_t start , size_t end , size_t lt ,
                                                                  Variable* lv,
                                                                  Prefix* lp,
                                                                  Node* r,
                                                                  size_t apos ) {
  return new (zone_->Malloc<Assign>()) Assign(start,end,lt,lv,lp,r,apos);
}

inline Assign* AstFactory::NewAssign( const Lexer& l , size_t lt , Variable* lv ,
                                                                   Prefix* lp,
                                                                   Node* r,
                                                                   size_t apos ) {
  return NewAssign(l.current().start,l.current().end,lt,lv,lp,r,apos);
}

inline Call* AstFactory::NewCall( size_t start , size_t end , Prefix* c ) {
  return new (zone_->Malloc<Call>()) Call(start,end,c);
}

inline Call* AstFactory::NewCall( const Lexer& l , Prefix* c ) {
  return NewCall(l.current().start,l.current().end,c);
}

inline If* AstFactory::NewIf( size_t start , size_t end , ::lavascript::zone::Vector<If::Branch>* bl ) {
  return new (zone_->Malloc<If>()) If(start,end,bl);
}

inline If* AstFactory::NewIf( const Lexer& l , ::lavascript::zone::Vector<If::Branch>* bl ) {
  return NewIf(l.current().start,l.current().end,bl);
}

inline For* AstFactory::NewFor( size_t start , size_t end , Variable* v , Node* first ,
                                                                          Node* second,
                                                                          Node* third,
                                                                          Chunk* b,
                                                                          size_t fp ) {
  return new (zone_->Malloc<For>()) For(start,end,v,first,second,third,b,fp);
}

inline For* AstFactory::NewFor( const Lexer& l , Variable* v , Node* first , Node* second ,
                                                                             Node* third ,
                                                                             Chunk* b,
                                                                             size_t fp ) {
  return NewFor(l.current().start,l.current().end,v,first,second,third,b,fp);
}

inline ForEach* AstFactory::NewForEach( size_t start , size_t end , Variable* v ,
                                                                    Node* i,
                                                                    Chunk* b,
                                                                    size_t fp ) {
  return new (zone_->Malloc<ForEach>()) ForEach(start,end,v,i,b,fp);
}

inline ForEach* AstFactory::NewForEach( const Lexer& l , Variable* v , Node* i ,
                                                                       Chunk* b,
                                                                       size_t fp ) {
  return NewForEach(l.current().start,l.current().end,v,i,b,fp);
}

inline Break* AstFactory::NewBreak( size_t start , size_t end ) {
  return new (zone_->Malloc<Break>()) Break(start,end);
}

inline Break* AstFactory::NewBreak( const Lexer& l ) {
  return NewBreak( l.current().start , l.current().end );
}

inline Continue* AstFactory::NewContinue( size_t start , size_t end ) {
  return new (zone_->Malloc<Continue>()) Continue(start,end);
}

inline Continue* AstFactory::NewContinue( const Lexer& l ) {
  return NewContinue(l.current().start,l.current().end);
}

inline Return* AstFactory::NewReturn( size_t start , size_t end , size_t rp ,
                                                                  Node* e ) {
  return new (zone_->Malloc<Return>()) Return(start,end,sp,e);
}

inline Return* AstFactory::NewReturn( const Lexer& l , size_t rp , Node* e ) {
  return NewReturn(l.current().start,l.current().end,rp,e);
}

inline Require* AstFactory::NewRequire( size_t start , size_t end , size_t rp , size_t ap ,
                                                                                Node* re,
                                                                                Variable* av ) {
  return new (zone_->Malloc<Require>()) Require(start,end,rp,ap,re,av);
}

inline Require* AstFactory::NewRequire( const Lexer& l , size_t rp , size_t ap ,
                                                                     Node* re,
                                                                     Variable* av ) {
  return NewRequire(l.current().start,l.current().end,rp,ap,re,av);
}

inline Chunk* AstFactory::NewChunk( size_t start , size_t end , ::lavascript::zone::Vector<Node*>* b ) {
  return new (zone_->Malloc<Chunk>()) Chunk(start,end,b);
}

inline Chunk* AstFactory::NewChunk( const Lexer& l , ::lavascript::zone::Vector<Node*>* b ) {
  return NewChunk(l.current().start,l.current().end,b);
}

inline Function* AstFactory::NewFunction( size_t start , size_t end , size_t fp , Variable* n ,
                ::lavascript::zone::Vector<Variable*>* p,Chunk* b ) {
  return new (zone_->Malloc<Function>()) Function(start,end,fp,n,p,b);
}

inline Function* AstFactory::NewFunction( const Lexer& l , size_t fp , Variable* n ,
    ::lavascript::zone::Vector<Variable*>* p , Chunk* b ) {
  return NewFunction( l.current().start , l.current().end , n , p , b );
}

inline Root* AstFactory::NewRoot( size_t start , size_t end , Chunk* chunk ) {
  return new (zone_->Malloc<Root>()) Root(start,end,chunk);
}

inline Root* AstFactory::NewRoot( const Lexer& l , Chunk* chunk ) {
  return NewRoot(l.current().start,l.current().end,chunk);
}


} // namespace lavascript
} // namespace ast
#endif // AST_FACTORY_H_
