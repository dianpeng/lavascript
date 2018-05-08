#ifndef CBASE_HIR_KIT_H_
#define CBASE_HIR_KIT_H_
#include "src/config.h"
#include "src/trace.h"
#include "type-inference.h"
#include "hir.h"

#include <utility>
#include <string>
#include <vector>

namespace lavascript {
namespace cbase      {
namespace hir        {
namespace kit        {

// HIR ControlFlowKit.
//
// This kit object is a helper class to simplify building HIR graph. It forms a DSL
// inside of C++ to let me build graph in a more readable and maintainable way instead
// of manually allocating node and link the node
//
// The kit has 2 parts:
// 1) for building control flow graph , which is a stateful object
// 2) for building expression graph. User needs to use a delegate object which has its
//    operator overloaded.

class ControlFlowKit {
 public:
  ControlFlowKit( Graph* graph );
  // Create start/end osr-start/osr-end and inline-start/inline-end node.Except for
  // inline-start/inline-end node, all can only be called once.
  ControlFlowKit& DoStart      ();
  ControlFlowKit& DoEnd        ();
  ControlFlowKit& DoInlineStart();
  ControlFlowKit& DoInlineEnd  ();
 public: // return/jump_value
  ControlFlowKit& DoReturn( Expr* );
  ControlFlowKit& DoJumpValue( Expr* );
 public: // branch
  ControlFlowKit& DoIf( Expr* );
  ControlFlowKit& DoElse();
  ControlFlowKit& DoEndIf( Phi* );
 public: // region
  ControlFlowKit& DoRegion();
 private:
  // Reset state. User should never call this function since using End/OSREnd function
  // automatically reset the state of the ControlFlowKit object
  void Reset();
  // A node represents the current building context of the control flow graph via the
  // kit object. The current context can be :
  //
  // 1) BB  , normal basic block
  // 2) BR  , normal branch
  // 3) LOOP, normal loop
  enum { BB , BR , LOOP };

  struct Branch {
    ControlFlow* if_node;
    ControlFlow* if_true;
    ControlFlow* if_false;
    Branch():if_node(NULL),if_true(NULL),if_false(NULL) {}
  };
  struct Context {
    Branch br;
    ControlFlow* bb;
    int type;
    void SetBB ( ControlFlow* b ) { type = BB; bb = b; }
    bool IsBB  () const { return type == BB; }
    bool IsBR  () const { return type == BR; }
    bool IsLoop() const { return type == LOOP; }
    Context( ControlFlow* r ): br(), bb(r)   , type(BB) {}
    Context( int t          ): br(), bb(NULL), type(t) {}
  };
  struct InlineBlock {
    InlineStart*start;
    std::vector<JumpValue*> jump_value;
    InlineBlock( InlineStart* s ) : start(s), jump_value() {}
  };

  Context&          context() { lava_verify(!context_.empty());      return context_.back(); }
  Context&     prev_context() { lava_verify(context_.size() > 1);    return context_[context_.size()-2]; }
  InlineBlock& inline_block() { lava_verify(!inline_block_.empty()); return inline_block_.back(); }
  ControlFlow*      region () { lava_verify(context().IsBB());       return context().bb; }
  void          set_region ( ControlFlow* r ) { lava_verify(context().IsBB()); context().bb = r; }
 private:
  Graph*                   graph_;
  ControlFlow*             start_;
  ControlFlow*             end_  ;
  std::vector<Return*>     return_list_;
  std::vector<Context>     context_;
  std::vector<InlineBlock> inline_block_;
};

// Exprssion level kit , simplify building expression HIR graph. It has some
// constraints currently. This construction of the expression doesn't pass
// through to the folding pipeline so constent folding is not performed. User
// are not supposed to generate unfolded exprssion
class E {
 public:
  E( Graph* , double );
  E( Graph* , int    );
  E( Graph* , bool   );
  E( Graph* , const char* );
  E( Graph* , const std::string& );
  E( Graph* );
 public:
  // factory method
  static E Arg  ( Graph* , std::uint32_t );
  static E GGet ( Graph* , const char* );
  static E UGet ( Graph* , std::uint32_t , std::uint8_t );

 public: // unary operator
  static E Not    ( const E& );
  static E Negate ( const E& );

 public: // builder functions , use overloaded operator to perform construction
  // arithmetic operation
  template< typename T > E operator +  ( const T& ) const;
  template< typename T > E operator -  ( const T& ) const;
  template< typename T > E operator *  ( const T& ) const;
  template< typename T > E operator /  ( const T& ) const;
  template< typename T > E operator %  ( const T& ) const;
  // comparison operation
  template< typename T > E operator == ( const T& ) const;
  template< typename T > E operator != ( const T& ) const;
  template< typename T > E operator >  ( const T& ) const;
  template< typename T > E operator >= ( const T& ) const;
  template< typename T > E operator <  ( const T& ) const;
  template< typename T > E operator <= ( const T& ) const;
  // logical operation
  template< typename T > E operator || ( const T& ) const;
  template< typename T > E operator && ( const T& ) const;
  // type conversion operator
  operator Expr* () const { return node_; }

 private:
  E( Graph* graph , Expr* node ) : node_(node), graph_(graph) {}
  inline bool IsF64 ( const E& l , const E& r ) const;
  inline bool IsStr ( const E& l , const E& r ) const;
  inline bool IsSSO ( const E& l , const E& r ) const;
  inline bool IsBool( const E& l , const E& r ) const;

  template< typename U , typename ...ARGS >
  U* New( ARGS&& ...args ) const { return U::New(graph_,std::forward<ARGS>(args)...); }

  Expr * node_;
  Graph* graph_;
};

inline bool E::IsF64( const E& l , const E& r ) const {
  return GetTypeInference(l.node_) == TPKIND_FLOAT64 &&
         GetTypeInference(r.node_) == TPKIND_FLOAT64;
}

inline bool E::IsStr( const E& l , const E& r ) const {
  auto lt = GetTypeInference(l.node_);
  auto rt = GetTypeInference(r.node_);
  return TPKind::IsString(lt) && TPKind::IsString(rt);
}

inline bool E::IsSSO( const E& l , const E& r ) const {
  auto lt = GetTypeInference(l.node_);
  auto rt = GetTypeInference(r.node_);
  return lt == TPKIND_SMALL_STRING && rt == TPKIND_SMALL_STRING;
}

inline bool E::IsBool( const E& l , const E& r ) const {
  return GetTypeInference(l.node_) == TPKIND_BOOLEAN &&
    GetTypeInference(r.node_) == TPKIND_BOOLEAN;
}

#define _ARITH(V,OP)                                                                        \
  template< typename T >                                                                    \
  E E::operator V ( const T& v ) const {                                                    \
    E rhs(graph_,v);                                                                        \
    return IsF64(*this,rhs) ? E(graph_,New<Float64Arithmetic>(node_,rhs.node_,Binary::OP)): \
                              E(graph_,New<Arithmetic>       (node_,rhs.node_,Binary::OP)); \
  }

_ARITH(+,ADD)
_ARITH(-,SUB)
_ARITH(*,MUL)
_ARITH(/,DIV)
_ARITH(%,MOD)

#undef _ARITH // _ARITH

#define _COMP(V,OP,...)                                                          \
    template< typename T >                                                       \
    E E::operator V ( const T& v ) const {                                       \
      E rhs(graph_,v);                                                           \
      if(IsF64(*this,rhs)) {                                                     \
        return E(graph_,New<Float64Compare>(node_,rhs.node_,Binary::OP));        \
      } __VA_ARGS__                                                              \
      else if(IsStr(*this,rhs)) {                                                \
        return E(graph_,New<StringCompare>(node_,rhs.node_,Binary::OP));         \
      } else {                                                                   \
        return E(graph_,New<Compare>(node_,rhs.node_,Binary::OP));               \
      }                                                                          \
    }

_COMP(==,EQ,else if(IsSSO(*this,rhs)) { return E(graph_,New<SStringEq>(node_,rhs.node_)); })
_COMP(!=,NE,else if(IsSSO(*this,rhs)) { return E(graph_,New<SStringNe>(node_,rhs.node_)); })
_COMP(> ,GT,)
_COMP(>=,GE,)
_COMP(< ,LT,)
_COMP(<=,LE,)

#undef _COMP // _COMP

#define _LOGIC(V,OP)                                                           \
    template< typename T >                                                     \
    E E::operator V ( const T& v ) const {                                     \
      E rhs(graph_,v);                                                         \
      if(IsBool(*this,rhs)) {                                                  \
        return E(graph_,New<BooleanLogic>(node_,rhs.node_,Binary::OP));        \
      } else {                                                                 \
        return E(graph_,New<Logical>(node_,rhs.node_,Binary::OP));             \
      }                                                                        \
    }

_LOGIC(&&,AND)
_LOGIC(||,OR )

#undef _LOGIC // _LOGIC


// helper function to check whether 2 input graphs are same in terms certain nodes
// (1) Equal1 only checks the control flow nodes
// (2) Equal2 checks control flow and also the expression level.
bool Equal1( const Graph& , const Graph& );
bool Euqal2( const Graph& , const Graph& );

} // namespace kit
} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_KIT_H_
