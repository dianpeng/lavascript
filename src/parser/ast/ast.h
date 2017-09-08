#ifndef PARSER_AST_AST_H_
#define PARSER_AST_AST_H_

#include <src/zone/zone.h>
#include <src/zone/string.h>
#include <src/zone/vector.h>
#include <src/parser/token.h>

#include <iostream>

/**
 * A normal AST implementation. We are trying to implement our AST as simple as
 * possible since we don't need too much fancy stuff of AST. Our AST *will not*
 * represent the very detail source code information due to the fact that during
 * parsing we do constant folding and simple strength reduction. The code after
 * parsing is not the source code initially.
 *
 * We are not Clang/LLVM , not desgied for tooling and we are designed for JIT.
 */

namespace lavascript {
namespace parser {
namespace ast {

#define LAVA_AST_LIST(__) \
  /** expression **/ \
  __( LITERAL , Literal , "literal" ) \
  __( VARIABLE, Variable, "variable") \
  __( PREFIX  , Prefix  , "prefix"  ) \
  __( BINARY  , Binary  , "binary"  ) \
  __( UNARY   , Unary   , "unary"   ) \
  __( TERNARY , Ternary , "ternary" ) \
  __( LIST    , List    , "list"    ) \
  __( OBJECT  , Object  , "object"  ) \
  /** statement **/ \
  __( VAR     , Var     , "var"     ) \
  __( ASSIGN  , Assign  , "assign"  ) \
  __( CALL    , Call    , "call"    ) \
  __( IF      , If      , "if"      ) \
  __( FOR     , For     , "for"     ) \
  __( FOREACH , ForEach , "foreach" ) \
  __( BREAK   , Break   , "break"   ) \
  __( CONTINUE, Continue, "continue") \
  __( RETURN  , Return  , "return"  ) \
  __( REQUIRE , Require , "require" ) \
  /** Chunk **/ \
  __( CHUNK   , Chunk   , "chunk"   ) \
  /** Functions **/ \
  __( FUNCTION, Function, "function") \
  __( ROOT    , Root    , "root"    )


#define __(A,B,C) A,
enum AstType {
  LAVA_AST_LIST(__)
  SIZE_OF_ASTS
};
#undef __ // __

#define __(A,B,C) struct B;
LAVA_AST_LIST(__)
#undef __ // __

const char* GetAstTypeName( AstType );

/** Base Ast Node for every Ast derived classes **/
struct Node : zone::ZoneObject {
  AstType type;

  /** Daignostic information for this NODE */
  size_t start_pos;   // Starting position of this AST in source code
  size_t end_pos;     // End position of this AST in source code

  size_t code_length() const { return end_pos - start_pos; }

#define __(A,B,C) inline B* As##B(); inline const B* As##B() const;
  LAVA_AST_LIST(__)
#undef __ // __

#define __(A,B,C) inline bool Is##B() const { return type == A; }
  LAVA_AST_LIST(__)
#undef __ // __

  Node( AstType t , size_t sp , size_t ep ):
    type(t),
    start_pos(sp),
    end_pos(ep)
  {}
};

struct Literal : public Node {
  /** Literal related information **/
  enum {
    kLitInteger,
    kLitReal,
    kLitBoolean,
    kLitString,
    kLitNull
  };
  int literal_type;
  union {
    int int_value;
    double real_value;
    bool bool_value;
    zone::String* str_value;
  };

  /** diagnostic information for this Literal Node **/
  size_t token_length;  // Length of this token assocciated with the *literal*


  Literal( size_t sp , size_t ep , size_t tk_len ):
    Node( LITERAL , sp , ep ) ,
    literal_type( kLitNull  )
  {}

  Literal( size_t sp , size_t ep , size_t tk_len ,
           bool bval ):
    Node( LITERAL , sp , ep ) ,
    literal_type( kLitBoolean )
  { bool_value = bval; }

  Literal( size_t sp , size_t ep , size_t tk_len ,
           int ival ) :
    Node( LITERAL , sp , ep ) ,
    literal_type( kLitInteger )
  { int_value = ival; }

  Literal( size_t sp , size_t ep , size_t tk_len ,
           double rval ):
    Node( LITERAL , sp , ep ) ,
    literal_type( kLitReal )
  { real_value = rval; }

  Literal( size_t sp , size_t ep , size_t tk_len ,
           zone::String* str ):
    Node( LITERAL , sp , ep ) ,
    literal_type( kLitString )
  { str_value = str; }

};


struct Variable : public Node {
  zone::String* name;

  Variable( size_t sp , size_t ep , zone::String* n ):
    Node( VARIABLE , sp , ep ),
    name(n)
  {}
};

struct FuncCall : zone::ZoneObject {
  zone::Vector<Node*>* args;
  FuncCall( size_t sp , size_t ep , zone::Vector<Node*>* a ):
    zone::ZoneObject(),
    args(a)
  {}
};

struct Prefix : public Node {
  struct Component : zone::ZoneObject {
    int t;   // type of component
    union {
      Variable* var;
      Node* expr;
      FuncCall* fc;
    };
    enum { DOT , INDEX , CALL };
    Component( Variable* v ): t(DOT) , var(v) {}
    Component( Node* e )    : t(INDEX) , expr(e) {}
    Component( FuncCall* f) : t(CALL)  , fc(f) {}
    bool IsCall() const { return t == CALL; }
    bool IsDot () const { return t == DOT ; }
    bool IsIndex()const { return t == INDEX;}
  };
  zone::Vector<Component>* list; // List of prefix operations
  Node* var;
  Prefix( size_t sp , size_t ep , zone::Vector<Component>* l , Node* v ):
    Node(PREFIX , sp , ep ),
    list(l),
    var(v)
  {}
};

struct Binary : public Node {
  size_t op_pos;        // Operator's token position in source code
  Token op;             // Operator of this binary
  Node* lhs;            // Left hand side of this binary
  Node* rhs;            // Right hand side of this binary

  Binary( size_t sp , size_t ep , size_t opp ,
      Token o , Node* l , Node* r ):
    Node( BINARY , sp , ep ),
    op_pos(opp),
    op(o),
    lhs(l),
    rhs(r)
  {}

};

struct Unary : public Node {
  size_t op_pos;        // Operator's token position in source code
  Token op;             // Operator for this unary
  Node* opr;            // Operand for this unary

  Unary( size_t sp , size_t ep , size_t opp ,
      Token o , Node* opr ):
    Node( UNARY , sp , ep ),
    op_pos(opp),
    op(o),
    opr(opr)
  {}
};

struct Ternary : public Node {
  size_t quest_pos;     // Question mark position
  size_t colon_pos;     // Colon mark position
  Node* _1st;           // first operand
  Node* _2nd;           // second operand
  Node* _3rd;           // third operand

  Ternary( size_t sp , size_t ep , size_t qp , size_t cp ,
      Node* first , Node* second , Node* third ):
    Node( TERNARY , sp , ep ),
    quest_pos(qp),
    colon_pos(cp),
    _1st(first),
    _2nd(second),
    _3rd(third)
  {}
};

struct List : public Node {
  zone::Vector<Node*>* entry;
  List( size_t sp , size_t ep , zone::Vector<Node*>* e ):
    Node( LIST , sp , ep ),
    entry(e)
  {}
};

struct Object : public Node {
  struct Entry : zone::ZoneObject {
    Node* key;
    Node* val;
    Entry( Node* k , Node* v ) : key(k) , val(v) {}
    Entry() : key(NULL), val(NULL) {}
  };
  zone::Vector<Entry>* entry;
  Object( size_t sp  ,size_t ep , zone::Vector<Entry>* e ):
    Node( OBJECT , sp , ep ),
    entry(e)
  {}
};

/** Statement */
struct Var : public Node {
  Variable* var;
  Node* expr;

  Var( size_t sp , size_t ep , Variable* v , Node* e ):
    Node( VAR , sp , ep ),
    var(v),
    expr(e)
  {}
};

struct Assign : public Node {
  enum { LHS_VAR , LHS_PREFIX };
  Variable* lhs_var;
  Prefix* lhs_pref;
  Node* rhs;
  Assign( size_t sp , size_t ep , Variable* lv , Node* r ):
    Node( ASSIGN , sp , ep ),
    lhs_var(lv),
    lhs_pref(NULL),
    rhs(r)
  {}
  Assign( size_t sp , size_t ep , Prefix* lv , Node* r ):
    Node( ASSIGN , sp , ep ),
    lhs_var(NULL),
    lhs_pref(lv),
    rhs(r)
  {}
};

struct Call : public Node {
  Prefix* call;
  Call( size_t sp , size_t ep , Prefix* c):
    Node( CALL , sp , ep ) ,
    call(c)
  {}
};

struct If : public Node {
  struct Branch : zone::ZoneObject {
    Node* cond;
    Chunk* body;
    size_t kw_pos; // Where if/elif/else keyword position is
    Branch() : cond(NULL) , body(NULL) , kw_pos(0) {}
  };
  zone::Vector<Branch>* br_list;
  If( size_t sp , size_t ep , zone::Vector<Branch>* bl ) :
    Node( IF , sp , ep ) , br_list(bl)
  {}
};

// Normal for with grammar like for ( expr ; expr ; expr )
struct For : public Node {
  Node* _1st;     // Initial assignment for induction variable
  Node* _2nd;     // Condition expression
  Node* _3rd;     // Incremental expression

  bool has_1st() const { return var == NULL && _1st == NULL; }
  bool has_2nd() const { return _2nd == NULL; }
  bool has_3rd() const { return _3rd == NULL; }

  Chunk* body;

  For( size_t sp  ,size_t ep , Variable* v , Node* first ,
      Node* second , Node* third , Chunk* b ):
    Node( FOR , sp , ep ),
    var (v),
    _1st(first),
    _2nd(second),
    _3rd(third),
    body(b)
  {}
};

struct ForEach : public Node {
  Variable* var;
  Node* iter;
  Chunk* body;

  ForEach( size_t sp  ,size_t ep , Variable* v ,
      Node* i , Chunk* b ):
    Node( FOREACH , sp , ep ),
    var(v),
    iter(i),
    body(b)
  {}
};

struct Break : public Node {
  Break( size_t sp , size_t ep ): Node(BREAK,sp,ep) {}
};

struct Continue : public Node {
  Continue( size_t sp , size_t ep ) : Node(CONTINUE,sp,ep) {}
};

struct Return : public Node {
  Node* expr;            // return expression if we have it
  bool has_return_value() const { return expr != NULL; }
  Return( size_t sp , size_t ep , Node* e ) :
    Node(RETURN,sp,ep),expr(e)
  {}
};

struct Require : public Node {
  size_t req_pos;       // Require position
  size_t as_pos;        // As position if we have as
  Node* req_expr;       // Require expression
  Variable* as_var;     // If we have an as, then as_var will be pointed to the variable
  bool has_as() const { return as_var != NULL; }

  Require( size_t sp , size_t ep , size_t rp , size_t ap ,
      Node* re , Variable* av ):
    Node( REQUIRE , sp , ep ),
    req_pos(rp),
    as_pos (ap),
    req_expr(re),
    as_var(av)
  {}
};

struct Chunk : public Node {
  zone::Vector<Node*>* body;
  Chunk( size_t sp , size_t ep , zone::Vector<Node*>* b ):
    Node(CHUNK,sp,ep),
    body(b)
  {}
};

struct Function : public Node {
  size_t func_pos;                 // Function keyword position
  Variable* name;                  // If function has a name
  zone::Vector<Variable*>* proto;  // Prototype of the function argument list
  Chunk* body;                     // Body of the function

  Function( size_t sp, size_t ep , size_t fp , Variable* n ,
      zone::Vector<Variable*>* p , Chunk* b ):
    Node(FUNCTION,sp,ep),
    func_pos(0),
    name(n),
    proto(p),
    body(b)
  {}
};

struct Root : public Node {
  Chunk* body;
  Root( size_t sp , size_t ep , Chunk* b ):
    Node(ROOT,sp,ep),
    body(b)
  {}
};

// Dump the ast into text representation for debugging purpose/ or other purpose
void DumpAst( const Node& , std::ostream& );

/** Inline definition of all Node::AsXXX functions */
#define __(A,B,C)                                                                    \
  inline B* Node::As##B()                                                            \
  { lava_assert( type == A , "expect type " C ); return static_cast<B*>(this); }     \
  inline const B* Node::As##B() const                                                \
  { lava_assert( type == A , "expect type" C ); return static_cast<const B*>(this); }

LAVA_AST_LIST(__)

#undef __ // __


/**
 * Visitor for AST. I personally don't like to write a visitor for AST due to the fact
 * that it is not very easy and intuitive to generate a recursive descent visiting into
 * a template class.
 *
 * For this class, it will help to organize the boilerplate code to *push down* the visiting.
 * The class will *not* tries to do the visit automatically but it leaves to user's choice.
 * The class exposes a lots of DoVisitXXX functions which expect user to call it in certain
 * VisitXXX classes. Inside of the DoVisitXXX functions, it will call VisitXXX function based
 * on its name and type.
 */
template< typename T >
class AstVisitor {
 protected:
  void VisitNode( const Node& );

 private:
  T* impl() { return static_cast<T*>(this); }
};

template< typename T >
void AstVisitor<T>::VisitNode( const Node& node ) {
  switch(node.type) {
    case LITERAL: impl()->Visit(static_cast<const Literal&>(node)); break;
    case VARIABLE:impl()->Visit(static_cast<const Variable&>(node)); break;
    case PREFIX:  impl()->Visit(static_cast<const Prefix&>(node)); break;
    case BINARY:  impl()->Visit(static_cast<const Binary&>(node)); break;
    case UNARY:   impl()->Visit(static_cast<const Unary&>(node)); break;
    case TERNARY: impl()->Visit(static_cast<const Ternary&>(node)); break;
    case LIST:    impl()->Visit(static_cast<const List&>(node)); break;
    case OBJECT:  impl()->Visit(static_cast<const Object&>(node)); break;
    case VAR:     impl()->Visit(static_cast<const Var&>(node)); break;
    case ASSIGN:  impl()->Visit(static_cast<const Assign&>(node)); break;
    case IF:      impl()->Visit(static_cast<const If&>(node)); break;
    case FOR:     impl()->Visit(static_cast<const For&>(node)); break;
    case FOREACH: impl()->Visit(static_cast<const ForEach&>(node)); break;
    case BREAK:   impl()->Visit(static_cast<const Break&>(node)); break;
    case CONTINUE:impl()->Visit(static_cast<const Continue&>(node)); break;
    case RETURN:  impl()->Visit(static_cast<const Return&>(node)); break;
    case REQUIRE: impl()->Visit(static_cast<const Require&>(node)); break;
    case CHUNK:   impl()->Visit(static_cast<const Chunk&>(node)); break;
    case FUNCTION:impl()->Visit(static_cast<const Function&>(node)); break;
    case ROOT :   impl()->Visit(static_cast<const Root&>(node)); break;
    default:      lava_unreach("unknown node type"); break;
  }
}


} // namespace ast
} // namespace parser
} // namespaace lavascript

#endif // PARSER_AST_AST_H_
