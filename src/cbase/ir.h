#ifndef CBASE_IR_H_
#define CBASE_IR_H_
#include "src/config.h"
#include "src/util.h"
#include "src/zone/zone.h"
#include "src/zone/vector.h"
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

/**
 * OSR support. We support OSR and OSR code are generated during IR construction
 * phase. Each function in cbase will be compiled into 2 types of output.
 * 1) a function doesn't need OSR , this result in a function that only has a mian
 *    entry
 * 2) a function does have OSR , this result in a function that will be compiled
 *    into a function have 2 entry , (1) main (2) OSR entry. OSR entry does not
 *    accept any types of optimization except ra and code gen
 *
 */

#define CBASE_IR_LIST(__)                       \
  /* ariethmetic/comparison node */             \
  __(Binary,BINARY,"binary")                    \
  __(Unary,UNARY ,"unary" )                     \
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
  __(ItrTest,ITRTEST,"itrtest")                 \
  __(ItrNext,ITRNEXT,"itrnext")                 \
  __(ItrDref,ITRDREF,"itrdref")                 \
  /* call     */                                \
  __(Call,CALL   ,"call"   )                    \
  /* const    */                                \
  __(Int32,INT32  ,"int32"  )                   \
  __(Int64,INT64  ,"int64"  )                   \
  __(Float64,FLOAT64,"float64")                 \
  __(LStr,LSTR   ,"lstr"   )                    \
  __(SSO, SSO    ,"sso"    )                    \
  __(Boolean,BOOLEAN,"boolean")                 \
  __(Null,NULL   ,"null"   )                    \
  /* compound */                                \
  __(IRList,LIST,   "list"   )                  \
  __(IRObject,OBJECT, "object" )                \
  __(IRClosure,CLOSURE,"closure")               \
  __(IRExtension,EXTENSION,"extension")         \
  /* box */                                     \
  __(Box,BOX,"box")                             \
  /* control flow */                            \
  __(ControlFlow,CONTROL_FLOW,"control_flow")   \
  __(Start,START,"start")                       \
  __(Loop,Loop,LOOP ,"loop" )                   \
  __(LoopExit,LOOP_EXIT,"loop_exit")            \
  __(Merge,MERGE,"merge")                       \
  __(Region,REGION,"region")                    \
  __(Ret,RET  , "ret" )                         \
  __(End,END  , "end" )                         \
  /* misc */                                    \
  __(Phi,PHI  , "phi" )                         \
  /* osr  */                                    \
  __(OSREntry,OSR_ENTRY,"osr_entry")            \
  __(OSRExit ,OSR_EXIT ,"osr_exit" )            \
  __(OSRLoadS,OSR_LOADS,"osr_loads")            \
  __(OSRLoadU,OSR_LOADU,"osr_loadu")            \
  __(OSRLoadG,OSR_LOADG,"osr_loadg")            \
  __(OSRStoreS,OSR_STORES,"osr_stores")         \
  __(OSRStoreU,OSR_STOREU,"osr_storeu")         \
  __(OSRStoreG,OSR_STOREG,"osr_storeg")         \
 /* end */

enum IRType {
#define __(A,B,...) IRTYPE_##B,
  CBASE_IR_LIST(__)

  SIZE_OF_IRTYPE
#undef __ // __
};

const char* IRTypeGetName( IRType );

struct BytecodeInfo {
  std::int32_t saved_slot;  // Where this will be if it is interpreted
  const std::uint32_t* pc;  // Pointer points to the BC for this ir node

  BytecodeInfo():
    saved_slot(-1),
    pc(NULL)
  {}

  BytecodeInfo( std::int32_t idx , const std::uint32_t* p ):
    saved_slot(idx),
    pc         (p)
  {}

  BytecodeInfo( const std::uint32_t* p ):
    saved_slot(-1),
    pc         (p)
  {}
};

// Forward declaration of all the IR
#define __(A,...) class A;
CBASE_IR_LIST(__)
#undef __ // __

// IR Constant Pool. This object is used to dedup those constant creation.
// We don't dedup the node since different node has its own unique positional
// information. But we dedup the storage of certain primitive type number.
// This is not for saving memory since storing a pointer wastes more than
// storing a int32_t if no alignment is considered. This is mainly for GVN
// hash implementation. Since once the primitive constant number is unique,
// we could use its *address* to identify its equality which align to rest of
// the node hash.
class ConstantFactory {
 public:
  const std::int32_t* GetInt32( std::int32_t );
  const std::int64_t* GetInt64( std::int64_t );
  const double*       GetFloat64( double );
  const bool*         GetTrue();
  const bool*         GetFalse();
 private:
  zone::Zone* zone_;
  const bool* true_;
  const bool* false_;
  std::vector<std::int32_t*> i32_pool_;
  std::vector<std::int64_t*> i64_pool_;
  std::vector<double*>       f64_pool_;
  std::vector<zone::String*> str_pool_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(ConstantFactory)
};


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

  const BytecodeInfo& bytecode_info() const {
    return bytecode_info_;
  }

  // the zone used to allocate Node object
  zone::Zone* zone() const { return zone_; }

 public: // side effect
  bool side_effect() const { return side_effect_; }
  bool propogate_effect() const { return propogate_effect_; }

 public: // input/output
  const zone::Vector<Node*>& input() const  { return input_ ; }
  const zone::Vector<Node*>& output() const { return output_; }
  zone::Vector<Node*>& input() { return input_; }
  zone::Vector<Node*>& output(){ return output_;}

 public: // Cast
#define __(A,...) A* As##A() { lava_verify(type_ == IRTYPE_##B); return static_cast<A*>(this); }
  CBASE_IR_LIST(__)
#undef __ // __

#define __(A,...) const A* As##A() const { \
  lava_verify(type_ == IRTYPE_##B); \
  return static_cast<const A*>(this); \
}
  CBASE_IR_LIST(__)
#undef __ // __

 public: // GVN supports

 protected:
  Node( IRType type , std::uint32_t id , const BytecodeInfo& binfo, zone::Zone* zone ,
                                                                    bool side_effect ,
                                                                    bool propogate_effect ):
    type_    (type),
    id_      (id),
    bytecode_info_(binfo),
    propogate_effect_(propogate_effect)
    input_   () ,
    output_  (),
    zone_    (zone)
 {}

 private:
  IRType type_;
  std::uint32_t id_;
  BytecodeInfo bytecode_info_;
  bool side_effect_;
  bool propogate_effect_;
  zone::Vector<Node*> input_;
  zone::Vector<Node*> output_;
  zone::Zone* zone_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(Node)
};

// ================================================================
// Const
// ================================================================
class Int32 : public Node {
 public:
  static Int32* New( ConstantFactory* , std::int32_t , const BytecodeInfo& );

 public:
  const std::int32_t* value_label() const { return value_label_; }
  std::int32_t value() const { return *value_label_;}

 private:
  const std::int32_t* value_label_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(Int32)
};

class Int64: public Node {
 public:
  static Int64* New( ConstantFactory* , std::int64_t , const BytecodeInfo& );

 public:
  const std::int64_t* value_label() const { return value_label_; }
  std::int64_t value() const { return *value_label_;}

 private:
  const std::int64_t* value_label_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Int64)
};

class Float64 : public Node {
 public:
  static Float64* New( ConstantFactory* , double , const BytecodeInfo& );
 public:
  const double* value_label() const { return value_label_; }
  double value_label() const { return *value_label_; }
 private:
  const double* value_label_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(Float64)
};

class Boolean : public Node {
 public:
  static Boolean* New( ConstantFactory* , bool , const BytecodeInfo& );
 public:
  const bool* value_label() const { return value_label_; }
  bool value() const { return *value_label_; }

 private:
  const bool* value_label_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(Boolean)
};

class LStr : public Node {
 public:
  static LStr* New( ConstantFactory* , String** , const BytecodeInfo& );
 public:
  const zone::String* value_label() const { return value_label_; }
  const zone::String& value() const { return *value_label_; }
 private:
  const zone::String* value_label_;

  LAVA_DSIALLOW_COPY_AND_ASSIGN(LStr)
};

class SSO : public Node {
 public:
  static SSO* New( ConstantFactory* , SSO* , const BytecodeInfo& );
 public:
  const zone::String* value_label() const { return value_label_; }
  const zone::String& value() const { return *value_label_; }
 private:
  const zone::String* value_label_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(SSO)
};

class Null : public Node {
 public:
  static Null* New( ConstantFactory* , const BytecodeInfo& );
 public:
  // We use NULL/0 to represent Null's value label and we cannot
  // use this address anywhere else
  const void* value_label() const { return (const void*)(0); }

 private:
  LAVA_DSIALLOW_COPY_AND_ASSIGN(Null)
};

// ==============================================================
// Arithmetic Node
// ==============================================================
class Binary : public Node {
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
  static Binary* New( zone::Zone* zone , Node* lhs , Node* rhs , Operator op ,
                                                           const BytecodeInfo& bc );

  static Operator BytecodeToOperator( interpreter::Bytecode );

 public:
  Node*   lhs() const { return lhs_; }
  Node*   rhs() const { return rhs_; }
  Operator op() const { return op_;  }

 private:
  Node* lhs_;
  Node* rhs_;
  Operator op_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(Binary)
};

class Unary : public Node {
 public:
  enum Operator {
    MINUS,
    NOT
  };

  static Operator BytecodeToOperator( interpreter::Bytecode bc ) {
    if(bc == interpreter::BC_NEGATE)
      return MINUS;
    else
      return NOT;
  }

  static Unary* New( zone::Zone* zone , Node* operand , Operator op ,
                                                  const BytecodeInfo& bc );

 public:
  Node* operand() const { return operand_; }
  Operator op  () const { return op_;      }

 private:
  Node* operand_;
  Operator   op_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(Unary)
};

} // namespace ir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_IR_H_
