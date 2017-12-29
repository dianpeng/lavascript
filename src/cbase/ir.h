#ifndef CBASE_IR_H_
#define CBASE_IR_H_
#include "src/config.h"
#include "src/util.h"
#include "src/cbase/bytecode-analyze.h"
#include "src/zone/zone.h"
#include "src/zone/vector.h"
#include "src/zone/list.h"
#include "src/zone/string.h"

#include "ool-array.h"

#include <vector>

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
  IRInfo( std::uint32_t method , const interpreter::BytecodeLocation& bc ):
    bc_     (bc),
    method_ (method)
  {}
 public:
  std::uint32_t method() const { return method_; }
  const interpreter::BytecodeLocation& bc() const { return bc_; }
 private:
  interpreter::BytecodeLocation bc_;  // Encoded bytecode
  std::uint32_t method_;              // Index for method information
};

/**
 * Used to record each IR's corresponding prototype information
 */
struct PrototypeInfo {
  std::uint32_t base;
  Handle<Closure> closure;
  PrototypeInfo( std::uint32_t b , const Handle<Closure>& cls ):
    base(b),
    closure(cls)
  {}
};

#define CBASE_IR_EXPRESSION(__)                 \
  /* base for all none control flow */          \
  __(Expr,EXPR,"expr")                          \
  /* const    */                                \
  __(Int32,INT32  ,"int32"  )                   \
  __(Int64,INT64  ,"int64"  )                   \
  __(Float64,FLOAT64,"float64")                 \
  __(LString,LONG_STRING,"lstring"   )          \
  __(SString,SMALL_STRING,"small_string")       \
  __(Boolean,BOOLEAN,"boolean")                 \
  __(Nil,NIL,"null"   )                         \
  /* compound */                                \
  __(IRList,LIST,   "list"   )                  \
  __(IRObject,OBJECT, "object" )                \
  /* closure */                                 \
  __(LoadCls,LOAD_CLS,"load_cls")               \
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
  __(IGet,IGET  ,"iget"  )                      \
  __(ISet,ISET  ,"iset"  )                      \
  /* gget */                                    \
  __(GGet,GGET  , "gget" )                      \
  __(GSet,GSET  , "gset" )                      \
  /* iterator */                                \
  __(ItrNew ,ITR_NEW ,"itr_new" )               \
  __(ItrNext,ITR_NEXT,"itr_next")               \
  __(ItrDeref,ITR_DEREF,"itr_deref")            \
  /* call     */                                \
  __(Call,CALL   ,"call"   )                    \
  /* phi */                                     \
  __(PHI,Phi,"phi")

#define CBASE_IR_CONTROL_FLOW(__)               \
  __(ControlFlow,CONTROL_FLOW,"control_flow")   \
  __(Start,START,"start")                       \
  __(LoopHeader,LOOP_HEADER,"loop_header")      \
  __(Loop,Loop,LOOP ,"loop" )                   \
  __(LoopExit,LOOP_EXIT,"loop_exit")            \
  __(If,IF,"if")                                \
  __(IfTrue,IF_TRUE,"if_true")                  \
  __(IfFalse,IF_FALSE,"if_false")               \
  __(Jump,JUMP,"jump")                          \
  __(Region,REGION,"region")                    \
  __(End,END  , "end" )

#define CBASE_IR_MISC(__)                       \
  __(InitCls,INIT_CLS,"init_cls")               \
  __(Projection,PROJECTION,"projection")        \

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

// Forward class declaration
#define __(A,...) class A;
CASE_IR_LIST(__)
#undef __ // __

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

  // get the belonged zone object from graph
  inline zone::Zone* zone() const;

 public: // type check and cast

#define __(A,B,...) bool Is##A() const { return type() == B; }
  CBASE_IR_LIST(__)
#undef __ // __

#define __(A,B,..) inline A* As##A(); inline const A* As##A() const;
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
 public: // GVN hash value and hash function 

  // If GVNHash returns 0 means this expression doesn't support GVN
  virtual std::uint64_t GVNHash()   { return 0; }
  virtual bool Equal( const Expr* ) { return false; }

 public: // patching function helps to mutate any def-use and use-def

  // Replace all expression that uses *this* expression node with all
  // the input node
  std::size_t Replace( Expr* );
 protected:
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

  // This function will add the input node into this node's operand list and
  // it will take care of the input node's ref list as well
  inline void AddOperand( Expr* node );

  // Reference list
  //
  // This list returns a list of Ref object which allow user to
  //   1) get a list of expression that uses this expression, ie who uses me
  //   2) get the corresponding iterator where *me* is inserted into the list
  //      so we can fast modify/remove us from its list
  const RefList* ref_list() const { return &ref_list_; }
  RefList* ref_list() { return &ref_list_; }

  // Add the referece into the reference list
  inline void AddRef( Expr* who_uses_me , const OperandIterator& iter );

 protected:
  Expr( Graph* graph , IRType type , std::uint32_t id , IRInfo* info ):
    Node             (type,id,graph),
    operand_list_    (),
    ref_list_        (),
    ir_info_         (info)
  {}

 private:
  OperandList operand_list_;
  RefList     ref_list_;
  IRInfo*     ir_info_;
};


class Arg : public Expr {
 public:
  inline static Arg* New( Graph* , std::uint32_t , IRInfo* );
  std::uint32_t index() const { return index_; }

 private:
  Arg( Graph* graph , std::uint32_t id , std::uint32_t index , IRInfo* info ):
    Expr  (ARG,id,graph,info),
    index_(index)
  {}

  std::uint32_t index_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Arg)
};

class Int32 : public Expr {
 public:
  inline static Int32* New( Graph* , std::int32_t , IRInfo* );
  std::int32_t value() const { return value_; }

 private:
  Int32( Graph* graph , std::uint32_t id , std::int32_t value , IRInfo* info ):
    Expr  (INT32,id,graph,info),
    value_(value)
  {}

  std::int32_t value_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Int32)
};

class Int64: public Expr {
 public:
  inline static Int64* New( Graph* , std::int64_t , IRInfo* );
  std::int64_t value() const { return value_; }

 private:
  Int64( Graph* graph , std::uint32_t id , std::int64_t value , IRInfo* info ):
    Expr  (INT64,id,graph,info),
    value_(value)
  {}

  std::int64_t value_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Int64)
};

class Float64 : public Expr {
 public:
  inline static Float64* New( Graph* , double , IRInfo* );
  double value() const { return value_; }

 private:
  Float64( Graph* graph , std::uint32_t id , double value , IRInfo* info ):
    Expr  (FLOAT64,id,graph,info),
    value_(value)
  {}

  double value_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Float64)
};

class Boolean : public Expr {
 public:
  inline static Boolean* New( Graph* , bool , IRInfo* );
  bool value() const { return value_; }

 private:
  Boolean( Graph* graph , std::uint32_t id , bool value , IRInfo* info ):
    Expr  (BOOLEAN,id,graph,info),
    value_(value)
  {}

  bool value_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Boolean)
};

class LString : public Expr {
 public:
  inline static LString* New( Graph* , const LString& , IRInfo* );
  const zone::String* value() const { return value_; }

 private:
  LString( Graph* graph , std::uint32_t id , const zone::String* value ,
                                             IRInfo* info ):
    Expr  (LONG_STRING,id,graph,info),
    value_(value)
  {}

  const zone::String* value_;
  LAVA_DSIALLOW_COPY_AND_ASSIGN(LString)
};

class SString : public Expr {
 public:
  inline static SString* New( Graph* , const SSO& , IRInfo* );
  const zone::String* value() const { return value_; }

 private:
  SString( Graph* graph , std::uint32_t id , const zone::String* value ,
                                             IRInfo* info ):
    Expr (SMALL_STRING,id,graph,info),
    value_(value)
  {}

  const zone::String* value_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(SSO)
};

class Nil : public Expr {
 public:
  inline static Nil* New( Graph* , IRInfo* );

 private:
  Nil( Graph* graph , std::uint32_t id , IRInfo* info ):
    Expr(NIL,id,graph,info)
  {}

  LAVA_DSIALLOW_COPY_AND_ASSIGN(Nil)
};

class IRList : public Expr {
 public:
  inline static IRList* New( Graph* , std::size_t size , IRInfo* );

  const zone::Vector<Expr*>& array() const { return array_; }
  zone::Vector<Expr*>& array() { return array_; }

  void Add( Expr* node ) {
    array_.Add(zone(),node);
  }

 private:
  IRList( Graph* graph , std::uint32_t id , std::size_t size , IRInfo* info ):
    Expr  (IRLIST,id,graph,info),
    array_()
  {
    array_.Reserve(zone(),size);
  }

  zone::Vector<Expr*> array_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(IRList)
};

class IRObject : public Expr {
 public:
  inline static IRObject* New( Graph* , std::size_t size , IRInfo* );

  struct Pair {
    Expr* key;
    Expr* val;
    Pair( Expr* k , Expr* v ): key(k), val(v) {}
    Pair() : key(NULL), val(NULL) {}
  };

  const zone::Vector<Pair>& array() const { return array_; }
  zone::Vector<Pair>& array() { return array_; }

  void Add( Expr* key , Expr* val ) {
    array_.Add(zone(),Pair(key,val));
  }

 private:
  IRObject( Graph* graph , std::uint32_t id , std::size_t size , IRInfo* info ):
    Expr  (IROBJECT,id,graph,info),
    array_()
  {
    array_.Reserve(zone(),size);
  }

  zone::Vector<Pair> array_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(IRObject)
};

// ----------------------------------------------------------------
// Closure
// ----------------------------------------------------------------
class LoadCls : public Expr {
 public:
  static inline LoadCls* New( Graph* , std::uint32_t ref , IRInfo* info );
  std::uint32_t ref() const { return ref_; }

 private:
  LoadCls( Graph* graph , std::uint32_t id , std::uint32_t ref , IRInfo* info ):
    Expr (LOAD_CLS,id,graph,info),
    ref_ (ref)
  {}

  std::uint32_t ref_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(LoadCls);
};

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
    NE ,
    AND,
    OR
  };
  inline static Operator BytecodeToOperator( interpreter::Bytecode );
  inline static const char* GetOperatorName( Operator );

  // Create a binary node
  inline static Binary* New( Graph* , Expr* , Expr* , Operator , IRInfo* );

 public:
  Node*   lhs() const { return operand_list()->First(); }
  Node*   rhs() const { return operand_list()->Last (); }
  Operator op() const { return op_;  }
  inline const char* op_name() const;

 private:

  Binary( Graph* graph , std::uint32_t id , Expr* lhs , Expr* rhs , Operator op ,
                                                                    IRInfo* info ):
    Expr  (BINARY,id,graph,info),
    op_   (op)
  {
    operand_list()->PushBack(zone(),lhs);
    operand_list()->PushBack(zone(),rhs);
  }

  Operator op_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Binary)
};

class Unary : public Expr {
 public:
  enum Operator { MINUS, NOT };

  inline static Unary* New( Graph* , Node* , Operator , IRInfo* );

  static Operator BytecodeToOperator( interpreter::Bytecode bc ) {
    if(bc == interpreter::interpreter::BC_NEGATE)
      return MINUS;
    else
      return NOT;
  }

 public:
  Node* operand() const { return operand_list()->First(); }
  Operator op  () const { return op_;      }

 private:
  Unary( Graph* graph , std::uint32_t id , Expr* opr , Operator op ,
                                                       IRInfo* info ):
    Expr  (UNARY,id,graph,info),
    op_   (op)
  {
    operand_list()->PushBack(zone(),opr);
  }

  Operator   op_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Unary)
};

class Ternary: public Expr {
 public:
  inline static Ternary* New( Graph* , Expr* , Expr* , Expr* , IRInfo* );

 private:
  Ternary( Graph* graph , std::uint32_t id , Expr* cond , Expr* lhs ,
                                                          Expr* rhs ,
                                                          IRInfo* info ):
    Expr  (TERNARY,id,graph,info)
  {
    operand_list()->PushBack(zone(),cond);
    operand_list()->PushBack(zone(),lhs);
    operand_list()->PushBack(zone(),rhs);
  }

  LAVA_DISALLOW_COPY_AND_ASSIGN(Ternary)
};

// -------------------------------------------------------------------------
// upvalue get/set
// -------------------------------------------------------------------------
class UGet : public Expr {
 public:
  inline static UGet* New( Graph* , std::uint8_t , IRInfo* );
  std::uint8_t index () const { return index_ ; }

 private:
  UGet( Graph* graph , std::uint32_t id , std::uint8_t imm ,
                                          IRInfo* info ):
    Expr   (UGET,id,graph,info),
    index_ (imm)
  {}

  std::uint8_t index_ ;
  LAVA_DISALLOW_COPY_AND_ASSIGN(UGet)
};

class USet : public Expr {
 public:
  inline static USet* New( Graph* , std::uint8_t , Expr* opr , IRInfo* );
  Expr* operand() const { return operand_list()->First(); }
  std::uint8_t index() const { return index_; }

 private:
  USet( Graph* graph , std::uint32_t id , std::uint8_t index , Expr* opr ,
                                                               IRInfo* info ):
    Expr  (USET,id,graph,info),
    index_(index)
  {
    operand_list()->PushBack(zone(),opr);
  }

  std::uint8_t index_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(USet)
};

// -------------------------------------------------------------------------
// property set/get
// -------------------------------------------------------------------------
class PGet : public Expr {
 public:
  inline static PGet* New( Graph* , Expr* , Expr* , IRInfo* );
  Expr* object() const { return operand_list()->First(); }
  Expr* key   () const { return operand_list()->Last (); }

 private:
  PGet( Graph* graph , std::uint32_t id , Expr* object , Expr* index ,
                                                         IRInfo* info ):
    Expr  (PGET,id,graph,info)
  {
    operand_list()->PushBack(zone(),object);
    operand_list()->PushBack(zone(),index );
  }

  LAVA_DISALLOW_COPY_AND_ASSIGN(PGet)
};

class PSet : public Expr {
 public:
  inline static PSet* New( Graph* , Expr* , Expr* , Expr* , IRInfo* );
  Expr* object() const { return operand_list()->First(); }
  Expr* key   () const { return operand_list()->Index(1);}
  Expr* value () const { return operand_list()->Last (); }

 private:
  PGet( Graph* graph , std::uint32_t id , Expr* object , Expr* index ,
                                                         Expr* value ,
                                                         IRInfo* info ):
    Expr  (PGET,id,graph,info)
  {
    operand_list()->PushBack(zone(),object);
    operand_list()->PushBack(zone(),index );
    operand_list()->PushBack(zone(),value );
  }

  LAVA_DISALLOW_COPY_AND_ASSIGN(PGet)
};

class IGet : public Expr {
 public:
  inline static IGet* New( Graph* , Expr* , Expr* , IRInfo* );
  Expr* object() const { return operand_list()->First(); }
  Expr* index () const { return operand_list()->Last (); }

 private:
  IGet( Graph* graph , std::uint32_t id , Expr* object , Expr* index ,
                                                         IRInfo* info ):
    Expr  (IGET,id,graph,info)
  {
    operand_list()->PushBack(zone(),object);
    operand_list()->PushBack(zone(),index );
  }

  LAVA_DISALLOW_COPY_AND_ASSIGN(IGet)
};

class ISet : public Expr {
 public:
  inline static ISet* New( Graph* , Expr* , Expr* , Expr* , IRInfo* );

  Expr* object() const { return operand_list()->First(); }
  Expr* index () const { return operand_list()->Index(1);}
  Expr* value () const { return operand_list()->Last (); }

 private:
  ISet( Graph* graph , std::uint32_t id , Expr* object , Expr* index ,
                                                         Expr* value ,
                                                         IRInof* info ):
    Expr(ISET,id,graph,info)
  {
    operand_list()->PushBack(zone(),object);
    operand_list()->PushBack(zone(),index );
    operand_list()->PushBack(zone(),value );
  }

  LAVA_DISALLOW_COPY_AND_ASSIGN(ISet)
};

// -------------------------------------------------------------------------
// global set/get
// -------------------------------------------------------------------------
class GGet : public Expr {
 public:
  inline static PGet* New( Graph* , Expr* , IRInfo* );
  Expr* name() const { return operand_list()->First(); }

 private:
  GGet( Graph* graph , std::uint32_t id , Expr* name , IRInfo* info ):
    Expr  (GGET,id,graph,info)
  {
    operand_list()->PushBack(zone(),name);
  }

  LAVA_DISALLOW_COPY_AND_ASSIGN(PGet)
};

class GSet : public Expr {
 public:
  inline static PSet* New( Graph* , Expr* key , Expr* value , IRInfo* );
  Expr* key () const { return operand_list()->First(); }
  Expr* value()const { return operand_list()->Last() ; }

 private:
  GSet( Graph* graph , std::uint32_t id , Expr* key , Expr* value ,
                                                      IRInfo* info ):
    Expr  (GSET,id,graph,info)
  {
    operand_list()->PushBack(zone(),key);
    operand_list()->PushBack(zone(),value);
  }

  LAVA_DISALLOW_COPY_AND_ASSIGN(PSet)
};

// -------------------------------------------------------------------------
// Iterator node
// -------------------------------------------------------------------------
class ItrNew : public Expr {
 public:
  inline static ItrNew* New( Graph* , Expr* , IRInfo* );
  Expr* operand() const { return operand_list()->First(); }

 private:
  ItrNew( Graph* graph , std::uint32_t id , Expr* operand , IRInfo* info ):
    Expr  (ITR_NEW,id,graph,info)
  {
    operand_list()->PushBack(zone(),operand);
  }

  LAVA_DISALLOW_COPY_AND_ASSIGN(ItrNew)
};

class ItrNext : public Expr {
 public:
  inline static ItrNext* New( Graph* , Expr* , IRInfo* );
  Expr* operand() const { return operand_list()->First(); }

 private:
  ItrNew( Graph* graph , std::uint32_t id , Expr* operand , IRInfo* info ):
    Expr  (ITR_NEXT,id,graph,info)
  {
    operand_list()->PushBack(zone(),operand);
  }

  LAVA_DISALLOW_COPY_AND_ASSIGN(ItrNext)
};

class ItrDeref : public Expr {
 public:
  enum {
    PROJECTION_KEY = 0,
    PROJECTION_VAL
  };

  inline static ItrDeref* New( Graph* , Expr* , IRInfo* );
  Expr* operand() const { return operand_list()->First(); }

 private:
  ItrDeref( Graph* graph , std::uint32_t id , Expr* operand , IRInfo* info ):
    Expr   (ITER_DEREF,id,graph,info)
  {
    operand_list()->PushBack(zone(),operand);
  }

  LAVA_DISALLOW_COPY_AND_ASSIGN(ItrDeref)
};

// -------------------------------------------------------------------------
// Phi node
//
// A Phi node is a control flow related merged node. It can accept *at most*
// 2 input nodes.
// -------------------------------------------------------------------------
class Phi : public Expr {
 public:
  inline static Phi* New( Graph* , ControlFlow* , IRInfo* );
  inline static Phi* New( Graph* , Expr* , Expr* , ControlFlow* , IRInfo* );

  // Bounded control flow region node.
  // Each phi node is bounded to a control flow regional node
  // and by this we can easily decide which region contributs
  // to a certain input node of Phi node
  ControlFlow* region() const { return region_; }

 private:

  Phi( Graph* graph , std::uint32_t id , ControlFlow* region , IRInfo* info ):
    Expr           (PHI,id,graph,info),
    region_        (region)
  {}

  ControlFlow* region_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Phi)
};

class Projection : public Expr {
 public:
  inline static Projection* New( Graph* , Expr* , std::uint32_t index , IRInfo* );
  Expr* operand() const { return operand_; }

  // a specific value to indicate which part of the input operand
  // needs to be projected
  std::uint32_t index() const { return index_; }

 private:
  Projection( Graph* graph , std::uint32_t id , Expr* operand , std::uint32_t index ,
                                                                IRInfo* info ):
    Expr  (PROJECTION,id,graph,info),
    index_(index)
  {
    operand_list()->PushBack(zone(),operand);
  }

  std::uint32_t index_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Projection)
};

// -------------------------------------------------------------------------
// Control Flow
// 
//  The control flow node needs to support one additional important
//  feature , mutation/modification/deletion of existed control flow
//  graph.
//
// -------------------------------------------------------------------------
class ControlFlow : public Node {
 public:
  const zone::Vector<Node*>* backward_edge() const {
    return &backward_edge_;
  }

  void AddBackwardEdge( ControlFlow* edge ) {
    backward_edge_.Add(zone(),edge);
  }

  // --------------------------------------------------------------------------
  // Effective expression doesn't belong to certain expression chain
  //
  // Like free function invocation, they are not part certain expression chain
  // but they have visiable effects.
  //
  // All these types of expressions are stored inside of the effect_expr list
  // to be used later on for code generation
  // --------------------------------------------------------------------------
  const zone::Vector<Expr*>* effect_expr() const {
    return &effect_expr_;
  }

  void AddEffectExpr( Expr* node ) {
    effect_expr_.Add(zone(),node);
  }

 protected:
  zone::Vector<Node*>* backedge_edge() {
    return &backward_edge_;
  }

  zone::Vector<Expr*>* effect_expr() {
    return &effect_expr_;
  }

 private:
  ControlFlow( IRType type , std::uint32_t id , Graph* graph , ControlFlow* parent = NULL ):
    Node(type,id,graph),
    backward_edge_   (),
    effect_expr_     ()
  {
    if(parent) backward_edge_.Add(zone(),parent);
  }

  zone::Vector<ControlFlow*> backward_edge_;
  zone::Vector<ir:Expr*>     effect_expr_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(ControlFlow)
};


class Region: public ControlFlow {
 public:
  inline static Region* New( Graph* );
  inline static Region* New( Graph* , ControlFlow* parent );

 private:
  Region( Graph* graph , std::uint32_t id ):
    ControlFlow(REGION,id,graph)
  {}

  LAVA_DISALLOW_COPY_AND_ASSIGN(Merge)
};

// --------------------------------------------------------------------------
//
// Loop related blocks
//
// --------------------------------------------------------------------------
class LoopHeader : public ControlFlow {
 public:
  inline static LoopHeader* New( Graph* , Expr* , ControlFlow* );
  Expr* condition() const { return condition_; }

 private:
  LoopHeader( Graph* graph , Expr* cond , ControlFlow* region ):
    ControlFlow(LOOP_HEADER,graph->AssignID(),graph,region),
    condition_ (cond)
  {}

  Expr* condition_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(LoopHeader);
};

class Loop : public ControlFlow {
 public:
  inline static Loop* New( Graph* );

 private:
  Loop( Graph* graph , ControlFlow* region ):
    ControlFlow(LOOP,graph->AssignID(),graph,region)
  {}

  LAVA_DISALLOW_COPY_AND_ASSIGN(Loop)
};

class LoopExit : public ControlFlow {
 public:
  inline static LoopExit* New( Graph* , Expr* );
  Expr* condition() const { return condition_; }

 private:
  LoopExit( Graph* graph , std::uint32_t id , Expr* cond , ControlFlow* region ):
    ControlFlow(LOOP_EXIT,id,graph,region),
    condition_ (cond)
  {}

  Expr* condition_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(LoopExit)
};

// -----------------------------------------------------------------------
//
// Branch
//
// -----------------------------------------------------------------------
class If : public ControlFlow {
 public:
  inline static If* New( Graph* , Expr* , ControlFlow* );
  Expr* condition() const { return condition_; }

 private:
  If( Grpah* graph , std::uint32_t id , Expr* cond , ControlFlow* region ):
    ControlFlow(IF,id,graph,region),
    condition_ (cond)
  {}

  Expr* condition_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(If)
};

class IfTrue : public ControlFlow {
 public:
  inline static IfTrue* New( Graph* , ControlFlow* );
  inline static IfTrue* New( Graph* );
 private:
  IfTrue( Graph* graph , std::uint32_t id , ControlFlow* region ):
    ControlFlow(IF_TRUE,id,graph,region)
  {}

  LAVA_DISALLOW_COPY_AND_ASSIGN(IfTrue)
};

class IfFalse: public ControlFlow {
 public:
  inline static IfFalse * New( Graph* , ControlFlow* );
  inline static IfFalse * New( Graph* );
 private:
  IfFalse( Graph* graph , std::uint32_t id , ControlFlow* region ):
    ControlFlow(IF_FALSE,id,graph,region)
  {}

  LAVA_DISALLOW_COPY_AND_ASSIGN(IfFalse)
};

class Jump : public ControlFlow {
 public:
  inline static Jump* New( Graph* , ControlFlow* );

  // which target this jump jumps to
  ControlFlow* target() const { return target_; }

  bool TrySetTarget( const std::uint32_t* bytecode_pc , ControlFlow* target ) {
    if(bytecode_pc_ == bytecode_pc) {
      target_ = target;
      return true;
    }
    return false;
  }

 private:
  Jump( Graph* graph , std::uint32_t id , ControlFlow* region ,
                                          const std::uint32_t* bytecode_bc ):
    ControlFlow(JUMP,id,graph,region),
    target_(NULL),
    bytecode_pc_(bytecode_bc)
  {}

  ControlFlow* target_; // where this Jump node jumps to
  const std::uint32_t* bytecode_pc_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Jump)
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

class Graph {
 public:
  Graph();
 public: // getter and setter
  void set_start( Start* start ) { start_ = start; }
  void set_end  ( End*   end   ) { end_   = end;   }

  Start* start() const { return start_; }
  End*   end  () const { return end_;   }
  zone::Zone* zone()   { return &zone_; }

  std::uint32_t id() const { return id_; }
  std::uint32_t AssignID() { return id_++; }

  std::uint32_t AddPrototypeInfo( const Handle<Closure>& cls , std::uint32_t base ) {
    prototype_info_.Add(zone(),PrototypeInfo(base,cls));
    return static_cast<std::uint32_t>(prototype_info_.size()-1);
  }

  const PrototypeInfo& GetProrotypeInfo( std::uint32_t index ) const {
    return prototype_info_[index];
  }

 private:
  zone::Zone                  zone_;
  Start*                      start_;
  End*                        end_;
  zone::Vector<PrototypeInfo> prototype_info_;
  std::uint32_t               id_;

  friend class GraphBuilder;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Graph)
};

// =========================================================================
//
// Inline Functions
//
// =========================================================================

#define __(A,B,...)                           \
  inline A* Node::As##A() {                   \
    lava_debug(NORMAL,lava_verify(Is##A());); \
    return static_cast<A*>(this);             \
  }                                           \
  inline const A* Node::As##A() const {       \
    lava_debug(NORMAL,lava_verify(Is##A());); \
    return static_cast<const A*>(this);       \
  }

CBASE_IR_LIST(__)

#undef __ // __

inline Arg* Arg::New( Graph* graph , std::uint32_t index , IRInfo* info ) {
  return graph->zone()->New<Arg>(graph,graph->AssignID(),index,info);
}

inline Int64* Int64::New( Graph* graph , std::int64_t value , IRInfo* info ) {
  return graph->zone()->New<Int64>(graph,graph->AssignID(),value,info);
}

inline Float64* Float64::New( Graph* graph , double value , IRInfo* info ) {
  return graph->zone()->New<Float64>(graph,graph->AssignID(),value,info);
}

inline Boolean* Boolean::New( Graph* graph , bool value , IRInfo* info ) {
  return graph->zone()->New<Boolean>(graph,value,info);
}

inline LString* LString::New( Graph* graph , const LongString& str , IRInfo* info ) {
  return graph->zone()->New<LString>(graph,graph->AssignID(),
      zone::String::New(graph->zone(),str.data(),str.size),info);
}

inline SString* SString::New( Graph* graph , const SSO& str , IRInfo* info ) {
  return graph->zone()->New<SString>(graph,graph->AssignID(),
      zone::String::New(graph->zone(),str.data(),str.size()),info);
}

inline Nil* Nil::New( Graph* graph , IRInfo* info ) {
  return graph->zone()->New<Nil>(graph,graph->AssignID(),info);
}

inline IRList* IRList::New( Graph* graph , std::size_t size , IRInfo* info ) {
  return graph->zone()->New<IRList>(graph,graph->AssignID(),size,info);
}

inline IRObject* IRObject::New( Graph* graph , std::size_t size , IRInfo* info ) {
  return graph->zone()->New<IRObject>(graph,graph->AssignID(),size,info);
}

inline Binary* Binary::New( Graph* graph , Expr* lhs , Expr* rhs, Operator op ,
                                                                  IRInfo* info ) {
  return graph->zone()->New<Binary>(graph,graph->AssignID(),lhs,rhs,op,info);
}

inline Binary::Operator Binary::BytecodeToOperator( interpreter::Bytecode op ) {
  switch(op) {
    case interpreter::BC_ADDRV: case interpreter::BC_ADDVR: case interpreter::BC_ADDVV: return ADD;
    case interpreter::BC_SUBRV: case interpreter::BC_SUBVR: case interpreter::BC_SUBVV: return SUB;
    case interpreter::BC_MULRV: case interpreter::BC_MULVR: case interpreter::BC_MULVV: return MUL;
    case interpreter::BC_DIVRV: case interpreter::BC_DIVVR: case interpreter::BC_DIVVV: return DIV;
    case interpreter::BC_MODRV: case interpreter::BC_MODVR: case interpreter::BC_MODVV: return MOD;
    case interpreter::BC_POWRV: case interpreter::BC_POWVR: case interpreter::BC_POWVV: return POW;
    case interpreter::BC_LTRV : case interpreter::BC_LTVR : case interpreter::BC_LTVV : return LT ;
    case interpreter::BC_LERV : case interpreter::BC_LEVR : case interpreter::BC_LEVV : return LE ;
    case interpreter::BC_GTRV : case interpreter::BC_GTVR : case interpreter::BC_GTVV : return GT ;
    case interpreter::BC_GERV : case interpreter::BC_GEVR : case interpreter::BC_GEVV : return GE ;

    case interpreter::BC_EQRV : case interpreter::BC_EQVR : case interpreter::BC_EQSV :
    case interpreter::BC_EQVS: case interpreter::BC_EQVV:
      return EQ;

    case interpreter::BC_NERV : case interpreter::BC_NEVR : case interpreter::BC_NESV :
    case interpreter::BC_NEVS: case interpreter::BC_NEVV:
      return NE;

    case interpreter::BC_AND: return AND;
    case interpreter::BC_OR : return OR;
    default:
      lava_unreachF("unknown bytecode %s",interpreter::GetBytecodeName(op));
      break;
  }
}

inline const char* Binary::GetOperatorName( Operator op ) {
  switch(op) {
    case ADD : return "add";
    case SUB : return "sub";
    case MUL : return "mul";
    case DIV : return "div";
    case MOD : return "mod";
    case POW : return "pow";
    case LT  : return "lt" ;
    case LE  : return "le" ;
    case GT  : return "gt" ;
    case GE  : return "ge" ;
    case EQ  : return "eq" ;
    case NE  : return "ne" ;
    case AND : return "and";
    case OR  : return "or";
    default: lava_die();
  }
}

inline Unary* Unary::New( Graph* graph , Expr* opr , Operator op , IRInfo* info ) {
  return graph->zone()->New<Unary>(graph,graph->AssignID(),opr,op,info);
}

inline Ternary* Ternary::New( Graph* graph , Expr* cond , Expr* lhs , Expr* rhs ,
                                                                      IRInfo* info ) {
  return graph->zone()->New<Ternary>(graph,graph->AssignID(),cond,lhs,rhs,info);
}

inline UGet* UGet::New( Graph* graph , std::uint8_t index , IRInfo* info ) {
  return graph->zone()->New<UGet>(graph,graph->AssignID(),index,info);
}

inline USet* USet::New( Graph* graph , Expr* opr , IRInfo* info ) {
  return graph->zone()->New<USet>(graph,graph->AssignID(),opr,info);
}

inline PGet* PGet::New( Graph* graph , Expr* obj , Expr* key , IRInfo* info ) {
  return graph->zone()->New<PGet>(graph,graph->AssignID(),obj,key,info);
}

inline PSet* PSet::New( Graph* graph , Expr* obk , Expr* key , Expr* value ,
                                                               IRInfo* info ) {
  return graph->zone()->New<PSet>(graph,graph->AssignID(),obj,key,value,info);
}

inline GGet* GGet::New( Graph* graph , Expr* key , IRInfo* info ) {
  return graph->zone()->New<GGet>(graph,graph->AssignID(),key,info);
}

inline GSet* GSet::New( Graph* graph , Expr* key, Expr* value , IRInfo* info ) {
  return graph->zone()->New<GSet>(graph,graph->AssignID(),key,value,info);
}

inline ItrNew* ItrNew::New( Graph* graph , Expr* operand , IRInfo* info ) {
  return graph->zone()->New<ItrNew>(graph,graph->AssignID(),operand,info);
}

inline ItrNext* ItrNext::New( Graph* graph , Expr* operand , IRInfo* info ) {
  return graph->zone()->New<ItrNext>(graph,graph->AssignID(),operand,info);
}

inline ItrDeref* ItrDeref::New( Graph* graph , Expr* operand , IRInfo* info ) {
  return graph->zone()->New<ItrDeref>(graph,graph->AssignID(),operand,info);
}

inline Phi* Phi::New( Graph* graph , Expr* lhs , Expr* rhs , IRInfo* info ) {
  return graph->zone()->New<Phi>(graph,graph->AssignID(),lhs,rhs,info);
}

inline Projection* Projection::New( Graph* graph , Expr* operand , std::uint32_t index ,
                                                                   IRInfo* info ) {
  return graph->zone()->New<Projection>(graph,graph->AssignID(),operand,index,info);
}

} // namespace ir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_IR_H_
