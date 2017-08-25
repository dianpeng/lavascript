#ifndef AST_H_
#define AST_H_

#include <src/zone/zone.h>
#include <src/zone/string.h>
#include <src/zone/vector.h>

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
namespace ast {

#define LAVA_AST_LIST(__) \
  /** expression **/ \
  __( LITERAL , Literal , "literal" ) \
  __( VARIABLE, Variable, "variable") \
  __( PREFIX  , Prefix  , "prefix"  ) \
  __( BINARY  , Binary  , "binary"  ) \
  __( UNARY   , Unary   , "unary"   ) \
  __( TERNARY , Ternary , "ternary" ) \
  __( FUNCCALL, FuncCall, "funccall") \
  __( LIST    , List    , "list"    ) \
  __( OBJECT  , Object  , "object"  ) \
  /** statement **/ \
  __( VAR     , Var     , "var"     ) \
  __( ASSIGN  , Assign  , "assign"  ) \
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
  size_t start_pos;                     // Starting position of this AST in source code
  size_t end_pos;                       // End position of this AST in source code

  size_t code_length() const { return end_pos - start_pos; }

#define __(A,B,C) inline B* As##B(); inline const B* As##B() const;
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
    Node( LITEARL , sp , ep ) ,
    literal_type( kLitBoolean )
  { bool_value = bval; }

  Literal( size_t sp , size_t ep , size_t tk_len ,
           int ival ) :
    Node( LITERAL , sp , ep ) ,
    literal_type( kLitInteger )
  { int_value = ival; }

  Literal( size_t sp , size_t ep , size_t tk_len ,
           double rval ):
    Node( LITEARL , sp , ep ) ,
    literal_type( kLitReal )
  { real_value = rval; }

  Literal( size_t sp , size_t ep , size_t tk_len ,
           zone::String* str ):
    Node( LITEARL , sp , ep ) ,
    literal_type( kLitString )
  { str_value = str; }

};


struct Variable : public Node {
  zone::String* name;

  size_t token_length; // Length of this variable tokens


  Variable( size_t sp , size_t ep , size_t tk_len ,
      zone::String* n ):
    Node( VARIABLE , sp , ep ),
    name(n),
    token_length(tk_len)
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
  };
  zone::Vector<Component>* list; // List of prefix operations
  Variable* var;
  Prefix( size_t sp , size_t ep ):
    Node(PREFIX , sp , ep ),
    list(NULL),
    var(NULL)
  {}
};

struct Binary : public Node {
  size_t op_pos;         // Operator's token position in source code
  Token op;              // Operator of this binary
  Node* lhs;             // Left hand side of this binary
  Node* rhs;             // Right hand side of this binary

  Binary( size_t sp , size_t ep ):
    Node( BINARY , sp , ep ),
    op_pos(0),
    op(),
    lhs(NULL),
    rhs(NULL)
  {}

};

struct Unary : public Node {
  size_t op_pos;        // Operator's token position in source code
  Token op;             // Operator for this unary
  Node* opr;            // Operand for this unary

  Unary( size_t sp , size_t ep ):
    Node( UNARY , sp , ep ),
    op_pos(0),
    op(),
    opr(NULL)
  {}
};

struct Ternary : public Node {
  size_t quest_pos;     // Question mark position
  size_t colon_pos;     // Colon mark position
  Node* _1st;           // first operand
  Node* _2nd;           // second operand
  Node* _3rd;           // third operand

  Ternary( size_t sp , size_t ep ):
    Node( TERNARY , sp , ep ),
    quest_pos(0),
    colon_pos(0),
    _1st(NULL),
    _2nd(NULL),
    _3rd(NULL)
  {}
};

struct FuncCall : public Node {
  zone::Vector<Node*>* args;
  FuncCall( size_t sp , size_t ep ):
    Node( FUNCCALL , sp , ep ),
    args(NULL)
  {}
};

struct List : public Node {
  zone::Vector<Node*>* entry;
  List( size_t sp , size_t ep ):
    Node( LIST , sp , ep ),
    entry(NULL)
  {}
};

struct Object : public Node {
  struct Entry : zone::ZoneObject {
    Node* key;
    Node* var;
  };
  zone::Vector<Entry>* entry;
  Object( size_t sp  ,size_t ep ):
    Node( OBJECT , sp , ep ),
    entry(NULL)
  {}
};

/** Statement */
struct Var : public Node {
  Variable* var;
  Node* expr;

  Var( size_t sp , size_t ep ):
    Node( VAR , sp , ep ),
    var(NULL),
    expr(NULL)
  {}
};

struct Assign : public Node {
  enum { LHS_VAR , LHS_PREFIX };
  int lhs_t;
  Variable* lhs_var;
  Prefix* lhs_pref;
  Node* rhs;
  size_t assign_pos; // Assignment operator position
  Assign( size_t sp , size_t ep ):
    Node( ASSIGN , sp , ep ),
    lhs_t(),
    lhs_var(NULL),
    lhs_pref(NULL),
    rhs(NULL),
    assign_pos(0)
  {}
};

struct If : public Node {
  struct Branch {
    Node* cond;
    Chunk* body;
    size_t kw_pos;           // Where if/elif/else keyword position is
    Branch() : cond(NULL) , body(NULL) , kw_pos(0) {}
  };
  zone::Vector<Branch>* br_list;
  If( size_t sp , size_t ep ) : Node( IF , sp , ep ) , br_list(NULL) {}
};

// Normal for with grammar like for ( expr ; expr ; expr )
struct For : public Node {
  Node* _1st;
  Node* _2nd;
  Node* _3rd;
  Chunk* body;
  size_t for_pos;         // Where for keyword position is

  For( size_t sp  ,size_t ep ):
    Node( FOR , sp , ep ),
    _1st(NULL),
    _2nd(NULL),
    _3rd(NULL),
    body(NULL),
    for_pos(0)
  {}
};

struct ForEach : public Node {
  Variable* var;
  Node* iter;
  size_t for_pos;

  ForEach( size_t sp  ,size_t ep ):
    Node( FOREACH , sp , ep ),
    var(NULL),
    iter(NULL),
    for_pos(0)
  {}
};

struct Break : public Node {
  Break( size_t sp , size_t ep ): Node(BREAK,sp,ep) {}
};

struct Continue : public Node {
  Continue( size_t sp , size_t ep ) : Node(CONTINUE,sp,ep) {}
};

struct Return : public Node {
  size_t ret_pos;        // return keyword position
  Node* expr;            // return expression if we have it
  bool has_return_value() const { return expr != NULL; }
  Return( size_t sp , size_t ep ) : Node(RETURN,sp,ep),ret_pos(0),expr(NULL){}
};

struct Requre : public Node {
  size_t req_pos;       // Require position
  size_t as_pos;        // As position if we have as
  Node* req_expr;       // Require expression
  Variable* as_var;     // If we have an as, then as_var will be pointed to the variable
  bool has_as() const { return as_var != NULL; }

  Require( size_t sp , size_t ep ):
    Node( REQUIRE , sp , ep ),
    req_pos(0),
    as_pos (0),
    req_expr(NULL),
    as_var(NULL)
  {}
};

struct Chunk : public Node {
  Vector<Node*>* body;
  Chunk( size_t sp , size_t ep ):
    Node(CHUNK,sp,ep),
    body(NULL)
  {}
};

struct Function : public Node {
  size_t func_pos;                        // Function keyword position
  Variable* name;                         // If function has a name
  Vector<Variable*>* proto;               // Prototype of the function argument list
  Chunk* body;                            // Body of the function

  Function( size_t sp, size_t ep ):
    Node(FUNCTION,sp,ep),
    func_pos(0),
    name(NULL),
    proto(NULL),
    body(NULL)
  {}
};

struct Root : public Node {
  Chunk* body;
  Root( size_t sp , size_t ep ):
    Node(ROOT,sp,ep),
    body(NULL)
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
  template< typename R >
  R Visit( const Node& );

 private:
  T* impl() { return static_cast<T*>(this); }
};

template< typename T >
template< typename R >
R AstVisitor<T>::Visit( const Node& node ) {
  switch(node.type) {
    case LITEARL: return impl()->Visit(static_cast<const Literal&>(node));
    case VARIABLE:return impl()->Visit(static_cast<const Variable&>(node));
    case PREFIX: return impl()->Visit(static_cast<const Prefix&>(node));
    case BINARY: return impl()->Visit(static_cast<const Binary&>(node));
    case UNARY:  return impl()->Visit(static_cast<const Unary&>(node));
    case TERNARY:return impl()->Visit(static_cast<const Ternary&>(node));
    case FUNCCALL:return impl()->Visit(static_cast<const FuncCall&>(node));
    case LIST:   return impl()->Visit(static_cast<const List&>(node));
    case OBJECT: return impl()->Visit(static_cast<const Object&>(node));
    case VAR:    return impl()->Visit(static_cast<const Var&>(node));
    case ASSIGN: return impl()->Visit(static_cast<const Assign&>(node));
    case IF:     return impl()->Visit(static_cast<const If&>(node));
    case FOR:    return impl()->Visit(static_cast<const For&>(node));
    case FOREACH:return impl()->Visit(static_cast<const Foreach&>(node));
    case BREAK:  return impl()->Visit(static_cast<const Break&>(node));
    case CONTINUE: return impl()->Visit(static_cast<const Continue&>(node));
    case RETURN: return impl()->Visit(static_cast<const Return&>(node));
    case REQUIRE:return impl()->Visit(static_cast<const Require&>(node));
    case CHUNK: return impl()->Visit(static_cast<const Chunk&>(node));
    case FUNCTION: return impl()->Visit(static_cast<const Function&>(node));
    case ROOT : return impl()->Visit(static_cast<const Root&>(node));
    default: lava_unreach("unknown node type");
  }
  return R();
}


} // namespace ast
} // namespaace lavascript

#endif // AST_H_
