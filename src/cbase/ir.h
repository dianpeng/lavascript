#ifndef CBASE_IR_H_
#define CBASE_IR_H_
#include "src/config.h"
#include "src/util.h"
#include "src/cbase/bytecode-analyze.h"
#include "src/zone/zone.h"
#include "src/zone/vector.h"
#include "src/zone/list.h"
#include "src/zone/string.h"

#include <vector>

/**
 * CBase compiler IR
 *
 * The CBase compiler is a method JIT and it uses sea of nodes as its IR
 *
 * The IR node sits on top the zone::Zone allocator and should be thread safe
 * since IR manipulation will happen at backgruond thread
 */

namespace lavasript {
namespace cbase {
namespace ir {
using namespace ::lavascript;

class Graph;
class GraphBuilder;

/**
 * This is a separate information maintained for each IR node. It contains
 * information needed for (1)GC (2)OSRExit. This information is always kept
 * and carry on to code generation.
 *
 * Some IR may have exactly same IRInfo object since one bytecode can be mapped
 * to multiple IR node in some cases.
 */

class IRInfo {
 public:
  IRInfo( const Handle<Closure>& closure , const std::uint32_t* bc ,
                                           const InterpreterRegisterSet& lcvar ,
                                           std::uint32_t base ):
    closure_(closure),
    bc_     (bc),
    loc_var_(lcvar),
    base_   (base)
  {}

 public:
  const Handle<Closure>& closure() const { return closure_l }
  const std::uint32_t* bc() const { return bc_; }
  const InterpreterRegisterSet& loc_var() const { return loc_var_; }
  const std::uint32_t  base() const { return base_; }

 private:
  Handle<Closure> closure_;
  const std::uint32_t* bc_;
  InterpreterRegisterSet loc_var_;  // Up to this IR, which local variable's register
                                    // has been used/modified. This is used by GC to
                                    // decide GC root
  const std::uint32_t base_;        // base offset for this IR, since we do have inline
};

#define CBASE_IR_EXPRESSION(__)                 \
  /* base for all none control flow */          \
  __(Expr,EXPR,"expr")                          \
  /* argument node */                           \
  __(ARG,Arg,"arg")                             \
  /* ariethmetic/comparison node */             \
  __(Binary,BINARY,"binary")                    \
  __(Unary,UNARY ,"unary" )                     \
  /* logic node */                              \
  __(And,AND,"and")                             \
  __(Or ,OR , "or")                             \
  /* ternary node */                            \
  __(Ternary,TERNARY,"ternary")                 \
  /* upvalue */                                 \
  __(UGet,UGET  ,"uget"  )                      \
  __(USet,USET  ,"uset"  )                      \
  /* property/idx */                            \
  __(PGet,PGET  ,"pget"  )                      \
  __(PSet,PSET  ,"pset"  )                      \
  /* gget */                                    \
  __(GGet,GGET  , "gget" )                      \
  __(GSet,GSET  , "gset" )                      \
  /* iterator */                                \
  __(ItrNew ,ITRNEW ,"itrnew" )                 \
  __(ItrNext,ITRNEXT,"itrnext")                 \
  __(ItrDref,ITRDREF,"itrdref")                 \
  /* call     */                                \
  __(Call,CALL   ,"call"   )                    \
  /* const    */                                \
  __(Int32,INT32  ,"int32"  )                   \
  __(Int64,INT64  ,"int64"  )                   \
  __(Float64,FLOAT64,"float64")                 \
  __(LongString,LONG_STRING   ,"lstr"   )       \
  __(SSO, SSO    ,"sso"    )                    \
  __(Boolean,BOOLEAN,"boolean")                 \
  __(Null,NULL   ,"null"   )                    \
  /* compound */                                \
  __(IRList,LIST,   "list"   )                  \
  __(IRObject,OBJECT, "object" )                \
  __(IRClosure,CLOSURE,"closure")               \
  __(IRExtension,EXTENSION,"extension")

#define CBASE_IR_CONTROL_FLOW(__)               \
  __(ControlFlow,CONTROL_FLOW,"control_flow")   \
  __(Start,START,"start")                       \
  __(Loop,Loop,LOOP ,"loop" )                   \
  __(LoopExit,LOOP_EXIT,"loop_exit")            \
  __(If,IF,"if")                                \
  __(Jump,JUMP,"jump")                          \
  __(Region,REGION,"region")                    \
  __(Ret,RET  , "ret" )                         \
  __(End,END  , "end" )

#define CBASE_IR_MISC(__)                       \
  __(Phi,PHI  , "phi" )

#define CBASE_IR_OSR(__)                        \
  __(OSREntry,OSR_ENTRY,"osr_entry")            \
  __(OSRExit ,OSR_EXIT ,"osr_exit" )            \
  __(OSRLoadS,OSR_LOADS,"osr_loads")            \
  __(OSRLoadU,OSR_LOADU,"osr_loadu")            \
  __(OSRLoadG,OSR_LOADG,"osr_loadg")            \
  __(OSRStoreS,OSR_STORES,"osr_stores")         \
  __(OSRStoreU,OSR_STOREU,"osr_storeu")         \
  __(OSRStoreG,OSR_STOREG,"osr_storeg")


#define CBASE_IR_LIST(__)                       \
  CBASE_IR_EXPRESSION(__)                       \
  CBASE_IR_CONTROL_FLOW(__)                     \
  CBASE_IR_MISC(__)                             \
  CBASE_IR_OSR (__)

enum IRType {
#define __(A,B,...) IRTYPE_##B,
  CBASE_IR_LIST(__)

  SIZE_OF_IRTYPE
#undef __ // __
};

const char* IRTypeGetName( IRType );

// Forward declaration of all the IR
#define __(A,...) class A;
CBASE_IR_LIST(__)
#undef __ // __

// Mother of all IR node , most of the important information should be stored via ID
// as out of line storage
class Node : public zone::ZoneObject {
 public:
  // type of the node
  IRType type() const { return type_; }

  // name/string of the type
  const char* type_name() const { return IRTypeGetName(type()); }

  // a unique id for this node , it can be used to indexed into secondary storage
  std::uint32_t id() const { return id_; }

  // get the belonged graph object
  Graph* graph() const { return graph_; }

 public: // Cast

#define __(A,...) A* As##A() {               \
    lava_verify(type_ == IRTYPE_##B);        \
    return static_cast<A*>(this);            \
  }

  CBASE_IR_LIST(__)

#undef __ // __

#define __(A,...) const A* As##A() const {   \
    lava_verify(type_ == IRTYPE_##B);        \
    return static_cast<const A*>(this);      \
  }

  CBASE_IR_LIST(__)

#undef __ // __

 protected:
  Node( IRType type , std::uint32_t id , Graph* graph ):
    type_    (type),
    id_      (id)  ,
    graph_   (graph)
 {}

 private:
  IRType        type_;
  std::uint32_t id_;
  Graph*        graph_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(Node)
};

// ================================================================
// Expr
//
//   This node is the mother all other expression node and its solo
//   goal is to expose def-use and use-def chain into different types
// ================================================================

class Expr : public Node {
 public:
  /**
   * Def-Use chain and Use-Def chain. We rename these field to
   * make it more easy to understand since I personally don't
   * think Def-Use and Use-Def chain to be a easy way to express
   * what it does mean
   *
   * 1) operand list , represent what expression is used/depend on
   *    by this expression
   *
   * 2) ref list     , represent list of expression that use *this*
   *    expression
   *
   */
  typedef zone::List<Expr*>        OperandList;
  typedef OperandList::Iterator    OperandIterator;

  struct Ref {
    OperandIterator id;  // iterator used for fast deletion of this Ref it is
                         // modified
    Expr* node;
    Ref( const OperandIterator& iter , Expr* n ): id(iter),node(n) {}
    Ref(): id(), node(NULL) {}
  };

  typedef zone::List<Ref>          RefList;


  // Operand list
  //
  // This list returns a list of operands used by this Expr/IR node. Most
  // of time operand_list will return at most 3 operands except for call
  // function
  OperandList* operand_list() { return &operand_list_; }
  const OperandList* operand_list() const { return &operand_list_; }
  inline void AddOperand( Expr* );


  // Reference list
  //
  // This list returns a list of Ref object which allow user to
  //   1) get a list of expression that uses this expression, ie who uses me
  //   2) get the corresponding iterator where *me* is inserted into the list
  //      so we can fast modify/remove us from its list
  const RefList* ref_list() const { return &ref_list_; }
  RefList* ref_list() { return &ref_list_; }
  inline void AddRef( Expr* who_uses_me , const OperandIterator& iter );

 public: // effect bits to mark the effect of this expression

  // during GVN, we won't enimilate expression that has side effect
  // or have propogate_effect
  bool side_effect () const { return side_effect_; }
  bool propogate_effect() const { return propogate_effect_; }

 public: // GVN hash value and hash function 
  virtual bool SupportGVN() const { return false; }

  virtual std::uint64_t GVNHash()   = 0;
  virtual bool Equal( const Expr* ) = 0;

  IRInfo* bytecode_info() const {
    return bytecode_info_;
  }
 protected:
  Expr( Graph* graph , IRType type , std::uint32_t id , bool side_effect,
                                                        bool propogate_effect ):
    Node             (type,id,graph),
    operand_list_    (),
    ref_list_        (),
    side_effect_     (side_effect),
    propogate_effect_(propogate_effect)
  {}

 private:
  OperandList operand_list_;
  RefList     ref_list_;
  bool side_effect_;
  bool propogate_effect_;
};

class Arg : public Expr {
 public:
  static Arg* New( Graph* , std::uint32_t , IRInfo* );
};

// ================================================================
// Const
// ================================================================
class Int32 : public Expr {
 public:
  inline static Int32* New( Graph* , std::int32_t , IRInfo* );
  std::int32_t value() const { return value_; }

 private:
  std::int32_t value_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Int32)
};

class Int64: public Expr {
 public:
  inline static Int64* New( Graph* , std::int64_t , IRInfo* );
  std::int64_t value() const { return value_; }

 private:
  std::int64_t value_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Int64)
};

class Float64 : public Expr {
 public:
  inline static Float64* New( Graph* , double , IRInfo* );
  double value() const { return value_; }

 private:
  double value_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Float64)
};

class Boolean : public Expr {
 public:
  inline static Boolean* New( Graph* , bool , IRInfo* );
  bool value() const { return value_; }

 private:
  bool value_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(Boolean)
};

class LongString : public Expr {
 public:
  inline static LongString* New( Graph* , String** , IRInfo* );
  const zone::String* value() const { return value_; }

 private:
  const zone::String* value_;

  LAVA_DSIALLOW_COPY_AND_ASSIGN(LongString)
};

class SSO : public Expr {
 public:
  inline static SSO* New( Graph* , SSO* , IRInfo* );
  const zone::String* value() const { return value_; }

 private:
  const zone::String* value_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(SSO)
};

class Null : public Expr {
 public:
  inline static Null* New( Graph* , IRInfo* );

 private:
  LAVA_DSIALLOW_COPY_AND_ASSIGN(Null)
};

class IRList : public Expr {
 public:
  inline static IRList* New( Graph* , IRInfo* );

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(IRList)
};

class IRObject : public Expr {
 public:
  inline static IRObject* New( Graph* , IRInfo* );

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(IRObject)
};

class IRClosure : public Expr {
 public:
  inline static IRClosure* New( Graph* , IRInfo* );
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(IRClosure)
};

class IRExtension : public Expr {
 public:
  inline static IRExtension* New( Graph* , IRInfo* );
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(IRExtension)
};

// ==============================================================
// Arithmetic Node
// ==============================================================
class Binary : public Expr {
 public:
  enum Operator {
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    POW,
    LT ,
    LE ,
    GT ,
    GE ,
    EQ ,
    NE
  };

  // Create a binary node
  inline static Binary* New( Graph* , Node* , Node* , Operator , IRInfo* );
  inline static Operator BytecodeToOperator( interpreter::Bytecode );

 public:
  Node*   lhs() const { return lhs_; }
  Node*   rhs() const { return rhs_; }
  Operator op() const { return op_;  }

  inline const char* op_name() const;

 private:
  Node* lhs_;
  Node* rhs_;
  Operator op_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(Binary)
};

class Unary : public Expr {
 public:
  enum Operator {
    MINUS,
    NOT
  };

  inline static Unary* New( Graph* , Node* , Operator , IRInfo* );

  static Operator BytecodeToOperator( interpreter::Bytecode bc ) {
    if(bc == interpreter::BC_NEGATE)
      return MINUS;
    else
      return NOT;
  }

 public:
  Node* operand() const { return operand_; }
  Operator op  () const { return op_;      }

 private:
  Node* operand_;
  Operator   op_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(Unary)
};

class And : public Expr {
 public:
  inline static And* New( Graph* , Expr* , Expr* , IRInfo* );
};

class Or  : public Expr {
 public:
  inline static Or* New ( Graph* , Expr* , Expr* , IRInfo* );
};

class Ternary: public Expr {
 public:
  inline static Ternary* New( Graph* , Expr* , Expr* , Expr* , IRInfo* );
};

// ==============================================================
// Control Flow
// ==============================================================
class ControlFlow : public Node {
 public: // backward edge and forward edge
  const zone::Vector<Node*>& backward_edge() const {
    return backward_edge_;
  }

  zone::Vector<Node*>& backedge_edge() {
    return backward_edge_;
  }

  void AddBackwardEdge( ControlFlow* edge ) {
    backward_edge_.Add(zone(),edge);
  }

  // ------------------------------------------------------------------------------------
  // Bounded expression/statement
  // Due to the natural way of sea-of-nodes, we may lose some statement though
  // they have side effect.
  // Example as this:
  //   foo();
  //   var bar = 3;
  //
  // During the construction of the graph, the call "foo()" will not be used as
  // input by any expression , since it has no reference its side effect , return
  // value will sit inside the stack and no one uses it. However since this node
  // has *side effect*, we cannot discard it automatically. We need to make it bound
  // inside of the certain node. This where bounded expression take into play.
  //
  // We lazily add this kind of statement into bounded list for the current control
  // flow list only when we find out this node is not added into any expression basically
  // its use chain is 0.
  // ------------------------------------------------------------------------------------
  const zone::Vector<Expr*>& effect_expr() const {
    return effect_expr_;
  }

  zone::Vector<Expr*>& effect_expr() {
    return effect_expr_;
  }

 private:
  zone::Vector<ControlFlow*> backward_edge_;
  zone::Vector<ir:Expr* > effect_expr_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(ControlFlow)
};

class LoopExit : public ControlFlow {
 public:
  // add a *jump/continue* node as its output
  void AddContinueEdge( Jump* );
 private:
  zone::Vector<Jump*> continue_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(LoopExit)
};

// Special node of the graph
class Start : public ControlFlow {
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Start)
};

class End   : public ControlFlow {
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(End)
};

class Region: public ControlFlow {
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Merge)
};

class If : public ControlFlow {
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(If)
};

// This unconditional JUMP it is needed to mark bytecode that is doing
// continue/break.
class Jump : public ControlFlow {
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Jump)
};

class Phi : public Node {
 public:
  void set_lhs( Expr* lhs ) { lhs_ = lhs; }
  void set_rhs( Expr* rhs ) { rhs_ = rhs; }

  Expr* lhs () const { return lhs_; }
  Expr* rhs () const { return rhs_; }

 private:
  Expr* lhs_;
  Expr* rhs_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(Phi)
};

class Graph {
 public:
  Graph();
 public: // getter and setter
  void set_start( Start* start ) { start_ = start; }
  void set_end  ( End*   end   ) { end_   = end;   }

  Start* start() const { return start_; }
  End*   end  () const { return end_;   }
  zone::Zone* zone()   { return &zone_; }

  const zone::Vector<IRInfo*>& ir_info() const { return ir_info_; }

  // add ir info into the ir_info array held by the Graph object
  void AddIRInfo( std::uint32_t index , IRInfo* info ) {
    if((index+1) > ir_info_.size()) {
      ir_info_.Resize(zone(),index+1);
    }
    ir_info_[index] = info;
  }

  std::uint32_t id() const { return id_; }
  std::uint32_t AssignID() { return id_++; }

 private:
  zone::Zone                zone_;
  zone::Vector<IRInfo*>     ir_info_;
  Start*                    start_;
  End*                      end_;
  Handle<Closure>           closure_;
  std::uint32_t             id_;

  friend class GraphBuilder;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Graph)
};

} // namespace ir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_IR_H_
