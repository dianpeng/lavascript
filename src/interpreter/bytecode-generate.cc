#include "bytecode-generate.h"
#include "bytecode.h"

#include <src/util.h>
#include <src/script.h>
#include <src/trace.h>
#include <src/parser/ast/ast.h>

#include <vector>

namespace lavascript {
namespace interpreter {
namespace {

using namespace lavascript::parser;
class Generator;
class RegisterAllocator;

class Register {
 public:
  static const Register kAccReg;
  static const std::size_t kAccIndex = 255;

  Register( std::uint8_t index ): index_(index) {}

 public:
  bool IsAcc() const { return index_ == kAccIndex; }
  std::uint8_t index() const { return index_; }
  operator int () const { return index_; }
  bool operator == ( const Register& reg ) const{
    return index_ == reg.index_;
  }
 private:
  std::uint8_t index_;
};

class RegisterAllocator {
 public:
  RegisterAllocator();

  inline bool Grab( Register* );
  inline void Drop( const Register& );
  inline bool IsAvailable( const Register& );
  inline bool IsUsed     ( const Register& );

  bool IsEmpty() const { return free_register_ == NULL; }
  std::size_t size() const { return size_; }
 private:
  static void* const kNotUsed = reinterpret_cast<void*>(0x1);

  struct Node {
    Node* next;
    Register reg;
    Node( std::uint8_t index ):next(NULL),reg(index){}
  };
  inline Node* RegisterToSlot( const Register& );

  Node* free_register_;
  std::size_t size_;
  std::uint8_t reg_buffer_[kAllocatableBytecodeRegisterSize*sizeof(Node)];
};

RegisterAllocator::RegisterAllocator():
  free_register_(NULL),
  size_(kAllocatableBytecodeRegisterSize),
  reg_buffer_()
{
  Node* n = reinterpret_cast<Node*>(reg_buffer_);

  for( std::size_t i = 0 ; i < kAllocatableBytecodeRegisterSize-1; ++i ) {
    ConstructFromBuffer<Node>(n,i);
    n->next = n+1;
    ++n;
  }
  ConstructFromBuffer<Node>(n,kAllocatableBytecodeRegisterSize-1);
  n->next = NULL;
}

inline bool RegisterAllocator::Grab( Register* reg ) {
  if(free_register_) {
    Node* ret = free_register_;
    free_register_ = free_register_->next;
    *reg = ret->reg;
    ret->next = static_cast<Node*>(kNotUsed);
    --size_;
    return true;
  }
  return false;
}

inline void RegisterAllocator::Drop( const Register& reg ) {
  if(!reg.IsAcc()) {
    Node* n = RegisterToSlot(reg);
#ifdef LAVASCRIPT_CHECK_OBJECTS
    lava_verify(n->next == static_cast<Node*>(kNotUsed));
#endif // LAVASCRIPT_CHECK_OBJECTS
    n->next = free_register_;
    free_register_ = n;
    ++size_;
  }
}

inline bool RegisterAllocator::IsAvailable( const Register& reg ) {
  Node* n = RegisterToSlot(reg);
  return n->next != static_cast<Node*>(kNotUsed);
}

inline bool RegisterAllocator::IsUsed( const Register& reg ) {
  Node* n = RegisterToSlot(reg);
  return n->next == static_cast<Node*>(kNotUsed);
}

inline RegisterAllocator::Node* RegisterAllocator::RegisterToSlot( const Register& reg ) {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(!reg.IsAcc());
#endif // LAVASCRIPT_CHECK_OBJECTS
  Node* n = reinterpret_cast<Node*>(reg_buffer_) + reg.index();
  return n;
}

/**
 *
 * Scope is an object that is used to track all the information generated
 * during the conversion phase. Basically we have 2 types of scopes:
 * 1) LexicalScope which is generated whenever we enter a lexical scope
 * 2) FunctionScope which is generated whenever a new function is encounterred
 */

class FunctionScope;
class LexicalScope;

class Scope {
 public:
  // Get its parental scope , if returns NULL , means it is the top most scope
  Scope* parent_scope() const;

  // Get its closest function scope
  Scope* function_scope() const;

  virtual bool IsLexicalScope() const = 0;
  virtual bool IsFunctionScope()const = 0;
  virtual FunctionScope* AsFunctionScope() = 0;
  virtual LexicalScope*  AsLexicalScope () = 0;
 public:
  // Define a *local variable* and pin it to a register
  // If we can define then a positive number is returned indicate the Register that
  // holds the variable , otherwise an error is recoreded and Acc register is returned
  bool DefineVar( const zone::String& , Register* );

  // Get a local variable from current lexical scope
  bool GetLocalVar( const zone::String& , Register* );

  // Get a variable as a upvalue starting from this lexical scope
  bool GetUpValue( const zone::String& , UpValueIndex* );

  // Size of variables defined in *this* scope
  std::size_t var_size() const;

  // Get the generator
  Generator* generator() const { return generator_; }

 private:
  Generator* generator_;
};

class LexicalScope {
};

class FunctionScope {
 public:
  BytecodeBuilder* bc_builder(); { return &bc_builder_; }
 private:
  BytecodeBuilder bc_builder_;
};

enum GenResultKind {
  KERROR,             // Yeah, failed for compilation
  KREG,               // Okay, the result is been held by an register
  KACC,               // It is in ACC register and it is not reserved cross expression
  KINT,               // It is a integer literal
  KREAL,              // It is a real literal
  KSTR,               // It is a string literal
  KTRUE,              // It is a true
  KFALSE,             // It is a false
  KNULL               // It is a null literal
};

struct GenResult {
  GenResultKind kind;
  std::int32_t ref;           // Reference if it is a int/real/string type
  Register reg;               // Register

  bool IsOk() const { return kind != KERROR; }
  operator bool () const { return !IsOk(); }
  void release() { release_ = true; }

  GenResult( Generator* gen ):
    kind(KERROR),
    ref(0),
    reg(0),
    generator_(gen),
    release_(false)
  {}

  GenResult( const GenResult& that ):
    kind(that.kind),
    ref (that.ref),
    reg (that.ref),
    generator_(that.generator_),
    release_(that.release_)
  {}

  ~GenResult() { if(!release_ && kind == EREG) generator_->Drop(reg); }
  private:
  Generator* generator_;
  bool release_;
};

#define emit func_scope()->bc_builder()->

class Generator {
 public:
  /* ------------------------------------------------
   * Generate expression
   * -----------------------------------------------*/

 public:
  Generator( Context* , const ast::Root& , Script* , std::string* );
  FunctionScope* func_scope() const { return func_scope_; }

  bool Generate();

 private:

  bool Visit( const ast::Literal& lit , GenResult* );
  bool Visit( const ast::Variable& var, GenResult* );
  bool Visit( const ast::FuncCall& fc , const Register& func , GenResult* );
  bool Visit( const ast::Prefix& pref , GenResult* );
  bool Visit( const ast::Unary&  , GenResult* );
  bool Visit( const ast::Binary& , GenResult* );
  bool Visit( const ast::Ternary&, GenResult* );

 private:
  FunctionScope* func_scope_;
  Scope* cur_scope_;
  RegisterAllocator ra_;
};

bool Generator::Visit( const ast::Literal& lit , GenResult* result ) {
  switch(lit.literal_type) {
    case ast::Literal::LIT_INTEGER:
      result->kind = KINT;
      result->ref = func_scope()->bc_builder()->Add( result->int_value );
      if(result->ref<0) {
        ErrorTooComplicatedFunc(result);
        return false;
      }
      return true;
    case ast::Literal::LIT_REAL:
      result->kind = KREAL;
      result->ref  = func_scope()->bc_builder()->Add( result->real_value );
      if(result->ref<0) {
        ErrorTooComplicatedFunc(result);
        return false;
      }
      return true;
    case ast::Literal::LIT_BOOLEAN:
      result->kind = lit.bool_value ? KTRUE : KFALSE;
      return true;
    case ast::Literal::LIT_STRING:
      result->kind = KSTR;
      result->ref = func_scope()->bc_builder()->Add( *result->str_value , context_ );
      if(result->ret <0) {
        ErrorTooComplicatedFunc(result);
        return false;
      }
      return true;
    default:
      result->kind = KNULL;
      return true;
  }
}

bool Generator::Visit( const ast::Variable& var , GenResult* result ) {
  // 1. Try to establish it as local variable
  if(cur_scope_->GetLocalVar(*var.name,&(result->reg))) {
    result->ekind = EREG;
    return true;
  } else if(cur_scope_->GetUpValue()) {
  } else {
    // It is a global variable so we need to emit global variable stuff
    std::int32_t ref = func_scope_->bc_builder()->Add(*var.name,context_);
    if(ref<0) {
      ErrorTooComplicatedFunc(result);
      return false;
    }
    // Reserve a register for holding this temporary value
    if(!ra_.Grab(&(result->reg))) {
      ErrorTooComplicatedFunc(result);
      return false;
    }
    emit gset(var.sci(),ref,result->reg.index());
    return true;
  }
}

bool Generator::Visit( const ast::FuncCall& fc , const Register& func ,
                                                 GenResult* result ) {
  std::vector<GenResult> result_set;
  for( std::size_t i = 0 ; i < fc.args->size() ; ++i ) {
  }
}


} // namespace interpreter
} // namespace interpreter
