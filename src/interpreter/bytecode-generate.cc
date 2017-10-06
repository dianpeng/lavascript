#include "bytecode-generate.h"
#include "bytecode-builder.h"

#include <src/error-report.h>
#include <src/context.h>
#include <src/objects.h>
#include <src/common.h>
#include <src/util.h>
#include <src/script-builder.h>
#include <src/trace.h>
#include <src/parser/ast/ast.h>

#include <vector>
#include <memory>

namespace lavascript {
namespace interpreter {
namespace {

using namespace lavascript::parser;

class RegisterAllocator;
class Register;
class ScopedRegister;
class Generator;
class FunctionScope;
class LexicalScope;

/**
 * Simple template class to represents existance of certain type/object.
 */
template< typename T >
class Optional {
 public:
  Optional():value_(),has_(false) {}
  Optional( const T& value ):value_(),has_(false) { Set(value); }
  Optional( const Optional& opt ):
    value_(),
    has_  (opt.has_) {
    if(has_) Copy(opt.Get());
  }
  Optional& operator = ( const Optional& that ) {
    if(this != &that) {
      Clear();
      if(that.Has()) Set(that.Get());
    }
    return *this;
  }
  ~Optional() { Clear(); }
 public:
  void Set( const T& value ) {
    Clear();
    Copy(value);
    has_ = true;
  }
  void Clear() {
    if(has_) {
      Destruct( reinterpret_cast<T*>(value_) );
      has_ = false;
    }
  }
  T& Get() {
    lava_verify(has_);
    return *reinterpret_cast<T*>(value_);
  }
  const T& Get() const {
    lava_verify(has_);
    return *reinterpret_cast<const T*>(value_);
  }
  bool Has() const { return has_; }
  operator bool () const { return has_; }
 private:
  void Copy( const T& value ) {
    ConstructFromBuffer<T>(reinterpret_cast<T*>(value_),value);
  }

  std::uint8_t value_[sizeof(T)];
  bool has_;
};

/**
 * Represent a bytecode register
 */
class Register {
 public:
  static const Register kAccReg;
  static const std::size_t kAccIndex = 255;

  Register( std::uint8_t index ): index_(index) {}
  Register():index_(kAccIndex) {}

 public:
  bool IsAcc() const { return index_ == kAccIndex; }
  void SetAcc() { index_ = kAccIndex; }
  std::uint8_t index() const { return index_; }
  operator int () const { return index_; }
  bool operator == ( const Register& reg ) const {
    return index_ == reg.index_;
  }
  bool operator != ( const Register& reg ) const {
    return index_ != reg.index_;
  }
 private:
  std::uint8_t index_;
};

const Register Register::kAccReg;

/**
 * Represent a lexical scoped bounded register. Mainly used to
 * help reclaim registers. This class should be preferred to using
 * the Register object directly.
 */
class ScopedRegister {
 public:
  ScopedRegister( Generator* generator , const Optional<Register>& reg ):
    generator_(generator),
    reg_(),
    empty_(!reg.Has())
  { if(reg.Has()) { reg_ = reg.Get(); } }

  ScopedRegister( Generator* generator , const Register& reg ):
    generator_(generator),
    reg_(reg)
  {}

  ScopedRegister( Generator* generator ):
    generator_(generator),
    reg_()
  {}

  inline ~ScopedRegister();
 public:
  bool IsEmpty() const { return empty_; }
  operator bool() const { return !IsEmpty(); }
  const Register& Get() const { lava_verify(!IsEmpty()); return reg_; }
  inline Register Release();
  inline void Reset( const Register& reg );
  inline void Reset();
  inline bool Reset( const Optional<Register>& reg );
 private:
  Generator* generator_;
  Register reg_;
  bool empty_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(ScopedRegister);
};

/**
 * A class to track all used and available registers. Each function will
 * have a register allocator.
 *
 * The allocator put all the available register into 2 groups. The 1st group
 * are *reserved* for local variables. Since we know how many local variables
 * are there in each scope , marked by the parser . We reserve those slots for
 * each variables at very first. The 2nd group is on demand register used to
 * hold temporary/intermediate value. All the register will be allocated *in
 * order* since these registers are mapped back to the stack slots.
 *
 * The 1st group's API will only worked with 1st group's register and 2nd group's
 * API will only worked with 2nd group's API. This design allow easily handle
 * concept between local variable's register and temporary registers.
 */
class RegisterAllocator {
 public:
  RegisterAllocator();

 public:
  // The following API are used for temporary/intermediate registers
  inline Optional<Register> Grab();
  inline void Drop( const Register& );
  inline bool IsAvailable( const Register& );
  inline bool IsUsed     ( const Register& );
  bool IsEmpty() const { return free_register_ == NULL; }
  std::size_t size() const { return size_; }
  inline std::uint8_t base() const;
 public:
  // The following API are used for local variable reservation
  inline bool EnterScope( std::size_t , std::uint8_t* base );
  inline void LeaveScope();

  // This API is just an alias of EnterScope and user will not call
  // the corresponding leave_scope due to the function is terminated
  inline void ReserveFuncArg( std::size_t len ) {
    std::uint8_t base;
    lava_verify(EnterScope(len,&base));
    lava_verify(base == 0);
  }

  bool IsReserved( const Register& reg ) const {
    if(scope_base_.empty()) {
      return false;
    } else {
      return reg.index() < scope_base_.back();
    }
  }
 private:
  static void* kRegUsed;

  // Free registers for temporary/intermeidate usage
  struct Node {
    Node* next;
    Register reg;
    Node( std::uint8_t index ):next(NULL),reg(index){}
  };
  inline Node* RegisterToSlot( const Register& );
  inline Node* RegisterToSlot( std::uint8_t );
  Node* free_register_;
  std::size_t size_;
  std::uint8_t reg_buffer_[kAllocatableBytecodeRegisterSize*sizeof(Node)];

  // Reserved registers for local variables
  std::vector<std::uint8_t> scope_base_;
};

void* RegisterAllocator::kRegUsed = reinterpret_cast<void*>(0x1);

/**
 * Scope is an object that is used to track all the information generated
 * during the conversion phase. Basically we have 2 types of scopes:
 * 1) LexicalScope which is generated whenever we enter a lexical scope
 * 2) FunctionScope which is generated whenever a new function is encounterred
 */
class Scope {
 public:
  // Get its parental scope , if returns NULL , means it is the top most scope
  Scope* parent() const { return parent_; }

  // Get the generator
  Generator* generator() const { return generator_; }

  virtual bool IsLexicalScope() const = 0;
  virtual bool IsFunctionScope()const = 0;
  inline FunctionScope* AsFunctionScope();
  inline LexicalScope*  AsLexicalScope ();

 protected:
  // Get the nearest enclosed function scope
  static FunctionScope* GetEnclosedFunctionScope( Scope* );
  inline Scope( Generator* , Scope* p );

 private:
  Generator* generator_;      // generator object
  Scope* parent_;             // Parental scope

  LAVA_DISALLOW_COPY_AND_ASSIGN(Scope);
};

class LexicalScope : public Scope {
 public:
  virtual bool IsLexicalScope() const { return true; }
  virtual bool IsFunctionScope() const { return false; }
 public:
  LexicalScope( Generator* , bool loop );
  ~LexicalScope();

  // We need to call this function right after the LexicalScope is
  // setup since inside of this function it reserves the registers
  // slot at very first part of the stack for local variables.
  bool Init( const ast::Chunk& );
  void Init( const ast::Function& );

  FunctionScope* func_scope() const { return func_scope_; }
 public:
  /* ------------------------------------
   * local variables                    |
   * -----------------------------------*/

  // Define a local variable with the given Register
  inline bool DefineLocalVar( const zone::String& , const Register& );

  // Get a local variable from current lexical scope
  inline Optional<Register> GetLocalVar( const zone::String& );

  // Get a loop variable in current scope and it *must* be in this loop.
  Register GetIterator() const {
    lava_debug(NORMAL,lava_verify(iterator_.Has()););
    return iterator_.Get();
  }

  // Size of variables defined in *this* scope
  std::size_t var_size() const { return local_vars_.size(); }

 public:
  /**----------------------------------------
   * Loop related APIs                      |
   * ---------------------------------------*/
  bool IsLoop() const { return is_loop_; }
  bool IsInLoop() const { return is_in_loop_; }
  bool IsLoopScope() const { return IsLoop() || IsInLoop(); }

  // Find its nearest enclosed loop scope can return *this*
  LexicalScope* GetNearestLoopScope();

  // Helpers for Break and Continue's jump
  inline bool AddBreak( const ast::Break& );
  inline bool AddContinue( const ast::Continue& );

  void PatchBreak( std::uint16_t );
  void PatchContinue( std::uint16_t );

 private:
  // Local variables related to this lexical scope
  struct LocalVar {
    const zone::String* name;
    Register reg;
    LocalVar():name(NULL),reg(){}
    LocalVar( const zone::String* n, const Register& r ): name(n), reg(r) {}

    bool operator == ( const zone::String& n ) const {
      return (*name) == n;
    }
  };
  std::vector<LocalVar> local_vars_;

  // Whether this lexical scope is a direct body of a loop
  bool is_loop_;

  // Whether this lexical scope is a scope that has been enclosed
  // by a loop body scope
  bool is_in_loop_;

  // Break label
  std::vector<BytecodeBuilder::Label> break_list_;

  // Continue label
  std::vector<BytecodeBuilder::Label> continue_list_;

  // Function scope that enclose this lexical scope
  FunctionScope* func_scope_;

  // Iterator that is used for the loops right inside of this loops
  Optional<Register> iterator_;


  LAVA_DISALLOW_COPY_AND_ASSIGN(LexicalScope);
};

class FunctionScope : public Scope {
 public:
  inline FunctionScope( Generator* , const ast::Function& node );
  inline FunctionScope( Generator* , const ast::Chunk& node );
  inline ~FunctionScope();

  virtual bool IsLexicalScope() const { return false; }
  virtual bool IsFunctionScope()const { return true; }

 public:
  // bytecode builder
  BytecodeBuilder* bb() { return &bb_; }

  // register allocator
  RegisterAllocator* ra() { return &ra_; }

  // body node
  const ast::Chunk& body() const { return *body_; }
 public:
  /* --------------------------------
   * full lexical scope local var   |
   * -------------------------------*/

  // Get local variable that is in this function scope .
  // This function will search all *enclosed* scopes
  Optional<Register> GetLocalVar( const zone::String& );

 public:
  /* -------------------------------
   * upvalue management            |
   * ------------------------------*/

  enum {
    UV_FAILED,
    UV_NOT_EXISTED,
    UV_SUCCESS
  };

  // Try to treat a variable *name* as an upvalue . If we can resolve
  // it as an upvalue , then we return true and index ; otherwise
  // returns false
  int GetUpValue( const zone::String& , std::uint16_t* ) ;

 private:
  bool FindUpValue( const zone::String& , std::uint16_t* );
  void AddUpValue ( const zone::String& , std::uint16_t  );

 private:
  // Bytecode builder for this Function
  BytecodeBuilder bb_;

  // Register allocator for this Function
  RegisterAllocator ra_;

  // UpValue table for this Function
  struct UpValue {
    const zone::String* name;
    std::uint16_t index;
    UpValue( const zone::String* n , std::uint16_t i ):name(n),index(i) {}
    bool operator == ( const zone::String& n ) const {
      return *name == n;
    }
  };
  std::vector<UpValue> upvalue_;

  // All enclosed lexical scope at this time
  std::vector<LexicalScope*> lexical_scope_list_;

  // function node
  const ast::Chunk* body_;

  friend class LexicalScope;

  LAVA_DISALLOW_COPY_AND_ASSIGN(FunctionScope);
};


/* =======================================
 * Expression Intermediate Represetation |
 * ======================================*/

enum ExprResultKind {
  KREG,               // Okay, the result is been held by an register
  KINT,               // It is a integer literal
  KREAL,              // It is a real literal
  KSTR,               // It is a string literal
  KTRUE,              // It is a true
  KFALSE,             // It is a false
  KNULL               // It is a null literal
};

class ExprResult {
 public:
  ExprResult(): kind_(KNULL), ref_(0), reg_(0) {}
  ExprResult( const ExprResult& that ):
    kind_(that.kind_), ref_(that.ref_) , reg_(that.reg_)
  {}
  ExprResult& operator = ( const ExprResult& that ) {
    if(this != &that) {
      kind_ = that.kind_;
      ref_  = that.ref_ ;
      reg_  = that.reg_ ;
    }
    return *this;
  }

 public:
  /* --------------------
   * getters            |
   * -------------------*/
  std::int32_t ref() const { lava_verify(IsRefType()); return ref_; }
  const Register& reg() const { lava_verify(IsReg()); return reg_; }
  ExprResultKind kind() const { return kind_; }

  /* --------------------
   * testers            |
   * -------------------*/
  bool IsLiteral() const { return !IsReg(); }
  bool IsRefType() const { return kind_ == KINT || kind_ == KREAL || kind_ == KSTR; }
  bool IsInteger() const { return kind_ == KINT; }
  bool IsReal()const { return kind_ == KREAL;}
  bool IsString() const { return kind_ == KSTR; }
  bool IsReg() const { return kind_ == KREG; }
  bool IsAcc() const { return kind_ == KREG && reg_.IsAcc(); }
  bool IsTrue() const { return kind_ == KTRUE; }
  bool IsFalse() const { return kind_ == KFALSE; }
  bool IsNull() const { return kind_ == KNULL; }

  /* --------------------
   * setters            |
   * -------------------*/
  void SetIRef( std::int32_t iref ) {
    ref_ = iref;
    kind_ = KINT;
  }

  void SetRRef( std::int32_t rref ) {
    ref_ = rref;
    kind_ = KREAL;
  }

  void SetSRef( std::int32_t sref ) {
    ref_ = sref;
    kind_ = KSTR;
  }

  void SetTrue() { kind_ = KTRUE; }
  void SetFalse(){ kind_ = KFALSE;}
  void SetNull() { kind_ = KNULL; }

  void SetRegister( const Register& reg ) {
    kind_ = KREG;
    reg_  = reg;
  }
  void SetAcc() { kind_ = KREG; reg_ = Register::kAccReg; }

 private:
  ExprResultKind kind_;
  std::int32_t ref_;
  Register reg_;
};

/* ==================================
 * Code emit macro:
 * EEMIT is for expression level
 * SEMIT is for statement  level
 * =================================*/

#define EEMIT(XX)                                            \
  do {                                                       \
    auto _ret = func_scope()->bb()->XX;                      \
    if(!_ret) {                                              \
      Error(ERR_FUNCTION_TOO_LONG,func_scope()->body());     \
      return false;                                          \
    }                                                        \
  } while(false)

#define SEMIT(XX)                                            \
  do {                                                       \
    auto _ret = func_scope()->bb()->XX;                      \
    if(!_ret) {                                              \
      Error(ERR_FUNCTION_TOO_LONG,func_scope()->body());     \
      return false;                                          \
    }                                                        \
  } while(false)


/**
 * General rules for handling the registers in bytecode.
 *
 * 1. Bytecode allocator *DOESN'T* manage Acc register.
 * 2. Any function can potentially use Acc register and it won't save it for you.
 *    Acc is always caller saved. So if a part of sub-expression are held in Acc,
 *    then before you call out any other function , you should Save it by calling
 *    SpillFromAcc to move your result from Acc to another register that owns by you.
 * 3. Any function's result *CAN* be held in Acc register. So calling some internal
 *    Visit function can result in the value held in Acc register.
 */

class Generator {
 public:
  /* ------------------------------------------------
   * Generate expression                            |
   * -----------------------------------------------*/
  Generator( Context* , const ast::Root& , ScriptBuilder* , std::string* );
  FunctionScope* func_scope() const { return func_scope_; }
  LexicalScope*  lexical_scope() const { return lexical_scope_; }

 public:
  bool Generate();

 private:
  /* --------------------------------------------
   * Helper for specialized binary instruction  |
   * -------------------------------------------*/
  enum BinOperandType{ TINT = 0 , TREAL , TSTR };

  bool CanBeSpecializedLiteral( ast::Literal& lit ) const {
    return lit.IsInteger() || lit.IsReal() || lit.IsString();
  }
  bool CanBeSpecializedLiteral( const ExprResult& expr ) const {
    return expr.IsInteger() || expr.IsReal() || expr.IsString();
  }
  bool SpecializedLiteralToExprResult( const ast::Literal& lit ,
                                       ExprResult* result ) {
    return Visit(lit,result);
  }

  inline BinOperandType GetBinOperandType( const ast::Literal& ) const;
  inline const char* GetBinOperandTypeName( BinOperandType t ) const;
  inline bool GetBinBytecode( const SourceCodeInfo& , const Token& tk ,
                                                      BinOperandType type ,
                                                      bool lhs ,
                                                      bool rhs ,
                                                      Bytecode* ) const;
 private:
  /* --------------------------------------------
   * Expression Code Generation                 |
   * -------------------------------------------*/
  bool Visit( const ast::Literal& lit , ExprResult* );
  bool Visit( const ast::Variable& var, ExprResult* );
  bool Visit( const ast::Prefix& pref , ExprResult* );
  bool Visit( const ast::Unary&  , ExprResult* );
  bool Visit( const ast::Binary& , ExprResult* );
  bool VisitLogic( const ast::Binary& , ExprResult* );
  bool Visit( const ast::Ternary&, ExprResult* );
  bool Visit( const ast::List&   , const Register& , ExprResult* );
  bool Visit( const ast::Object& , const Register& , ExprResult* );
  bool Visit( const ast::List&  node , ExprResult* result ) {
    return Visit(node,Register::kAccReg,result);
  }
  bool Visit( const ast::Object& node , ExprResult* result ) {
    return Visit(node,Register::kAccReg,result);
  }

  bool VisitExpression( const ast::Node&   , ExprResult* );
  bool VisitExpression( const ast::Node& expr , Register* );
  bool VisitExpression( const ast::Node& expr , ScopedRegister* );

  // Visit prefix like ast until end is met
  bool VisitPrefix( const ast::Prefix& pref , std::size_t end ,
                                              bool tcall ,
                                              Register* );

  bool VisitPrefix( const ast::Prefix& pref , std::size_t end ,
                                              bool tcall ,
                                              ScopedRegister* );

  /* -------------------------------------------
   * Statement Code Generation                 |
   * ------------------------------------------*/
  bool Visit( const ast::Var& , Register* holder = NULL );
  bool Visit( const ast::Assign& );
  bool VisitSimpleAssign( const ast::Assign& );
  bool VisitPrefixAssign( const ast::Assign& );
  bool Visit( const ast::Call& );
  bool Visit( const ast::If& );
  bool VisitForCondition( const ast::For& , const Register& var );
  bool Visit( const ast::For& );
  bool Visit( const ast::ForEach& );
  bool Visit( const ast::Break& );
  bool Visit( const ast::Continue& );
  bool CanBeTailCallOptimized( const ast::Node& node ) const;
  bool Visit( const ast::Return& );

  bool VisitStatment( const ast::Node& );

  bool VisitChunkNoLexicalScope( const ast::Chunk& );
  bool VisitChunk( const ast::Chunk& , bool );

  /* -------------------------------------------
   * Function                                 |
   * -----------------------------------------*/
  Handle<Prototype> VisitFunction( const ast::Function& );

  bool VisitNamedFunction( const ast::Function& );
  bool VisitAnonymousFunction( const ast::Function& );

 private: // Misc helpers --------------------------------------
  // Spill the Acc register to another register
  Optional<Register> SpillFromAcc( const SourceCodeInfo& );
  bool SpillToAcc( const SourceCodeInfo& , ScopedRegister* );

  // Allocate literal with in certain registers
  bool AllocateLiteral( const ast::Literal& , const Register& );

  // Convert ExprResult to register, it may allocate new register
  // to hold it if it is literal value
  Optional<Register> ExprResultToRegister( const SourceCodeInfo& sci , const ExprResult& );

 private: // Errors ---------------------------------------------
  void Error( const SourceCodeInfo& , const char* fmt , ... ) const;

  // Predefined error category for helping manage consistent error reporting
#define BYTECODE_COMPILER_ERROR_LIST(__) \
  __(REGISTER_OVERFLOW,"too many intermeidate value and local variables")      \
  __(TOO_MANY_LITERALS,"too many integer/real/string literals")                \
  __(TOO_MANY_PROTOTYPES,"too many function defined in one file")              \
  __(FUNCTION_TOO_LONG,"function is too long and too complex")                 \
  __(FUNCTION_NAME_REDEFINE,"function is defined before")                      \
  __(LOCAL_VARIABLE_NOT_EXISTED,"local variable is not existed")

  enum ErrorCategory {
#define __(A,B) ERR_##A,
    BYTECODE_COMPILER_ERROR_LIST(__)
    SIZE_OF_ERROR_CATEGORY
#undef __ // __
  };

  const char* GetErrorCategoryDescription( ErrorCategory ) const;

  // The following ErrorXXX function is common or frequently used error report
  // function which captures certain common cases
  void Error( ErrorCategory , const ast::Node& , const char* fmt , ... ) const;
  void Error( ErrorCategory , const ast::Node& ) const;
  void Error( ErrorCategory , const SourceCodeInfo& sci ) const;

 private:
  FunctionScope* func_scope_;
  LexicalScope* lexical_scope_;
  ScriptBuilder* script_builder_;
  Context* context_;
  const ast::Root* root_;
  std::string* error_;

  friend class Scope;
  friend class LexicalScope;
  friend class FunctionScope;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Generator);
};

/* =====================================================================
 *
 * Definition
 *
 * ===================================================================*/

RegisterAllocator::RegisterAllocator():
  free_register_(NULL),
  size_(kAllocatableBytecodeRegisterSize),
  reg_buffer_()
{
  free_register_ = reinterpret_cast<Node*>(reg_buffer_);
  Node* n = reinterpret_cast<Node*>(reg_buffer_);

  for( std::size_t i = 0 ; i < kAllocatableBytecodeRegisterSize-1; ++i ) {
    ConstructFromBuffer<Node>(n,i);
    n->next = n+1;
    ++n;
  }
  ConstructFromBuffer<Node>(n,kAllocatableBytecodeRegisterSize-1);
  n->next = NULL;
}

inline Optional<Register> RegisterAllocator::Grab() {
  if(free_register_) {
    Node* ret = free_register_;
    free_register_ = free_register_->next;
    ret->next = static_cast<Node*>(kRegUsed);
    --size_;
    return Optional<Register>(ret->reg);
  }
  return Optional<Register>();
}

inline void RegisterAllocator::Drop( const Register& reg ) {
  if(!reg.IsAcc() && !IsReserved(reg)) {
    Node* n = RegisterToSlot(reg);

    lava_debug(NORMAL,
        lava_verify(n->next == static_cast<Node*>(kRegUsed));
        );

    /**
     * Ensure the register is put in order and then Grab function
     * will always return the least indexed registers */
    if(free_register_) {
      if(free_register_->reg.index() > reg.index()) {
        n->next = free_register_;
        free_register_ = n;
      } else {
        for( Node* c = free_register_ ; c ; c = c->next ) {

          lava_debug(NORMAL,
              lava_verify(c->reg.index()<reg.index());
              );

          Node* next = c->next;
          if(next) {
            if(next->reg.index() > reg.index()) {
              c->next = n;
              n->next = next;
              break;
            }
          } else {
            c->next = n;
            break;
          }
        }
      }
    } else {
      n->next = NULL;
      free_register_ = n;
    }
    ++size_;
  }

  lava_debug(NORMAL,
    for( Node* c = free_register_ ; c ; c = c->next ) {
      lava_verify( c->next && c->reg.index() < c->next->reg.index() );
    }
  );
}

inline bool RegisterAllocator::IsAvailable( const Register& reg ) {
  Node* n = RegisterToSlot(reg);
  return n->next != static_cast<Node*>(kRegUsed);
}

inline bool RegisterAllocator::IsUsed( const Register& reg ) {
  Node* n = RegisterToSlot(reg);
  return n->next == static_cast<Node*>(kRegUsed);
}

inline RegisterAllocator::Node*
RegisterAllocator::RegisterToSlot( const Register& reg ) {
  lava_debug(NORMAL,!reg.IsAcc(););
  Node* n = reinterpret_cast<Node*>(reg_buffer_) + reg.index();
  return n;
}

inline RegisterAllocator::Node*
RegisterAllocator::RegisterToSlot( std::uint8_t index ) {
  return RegisterToSlot(Register(index));
}

inline std::uint8_t RegisterAllocator::base() const {
  return free_register_ ? free_register_->reg.index() : Register::kAccIndex;
}

bool RegisterAllocator::EnterScope( std::size_t size , std::uint8_t* b ) {
  lava_debug(NORMAL,
      if(free_register_)
        lava_verify(free_register->reg.index() == base()+1);
      );

  if((base() + size) > kAllocatableBytecodeRegisterSize) {
    return false; // Too many registers so we cannot handle it
  } else {
    std::size_t start = base();
    Node* next;
    Node* cur;

    for( cur = free_register_ ; cur ; cur = next ) {
      lava_debug(NORMAL,lava_verify(cur->reg.index() == start););
      next = cur->next;
      cur->next = static_cast<Node*>(kRegUsed);
      ++start;
      if(start == base()+size) break;
    }

    size_ -= size;
    free_register_ = next;

    *b = base();
    scope_base_.push_back(base()+size);

    return true;
  }
}

void RegisterAllocator::LeaveScope() {
  lava_debug(NORMAL,
      lava_verify(!scope_base_.empty());
      lava_verify(!free_register_ ||
                  (free_register_->reg.index() == scope_base_.back()));
      );
  std::uint8_t end = scope_base_.back();
  scope_base_.pop_back();
  std::uint8_t start = scope_base_.empty() ? 0 : scope_base_.back();

  if(end == kAllocatableBytecodeRegisterSize) {
    RegisterToSlot(end)->next = NULL;
  }

  for( --end ; end > start ; --end ) {
    Node* to = RegisterToSlot(Register(end));
    Node* from = RegisterToSlot(end-1);
    from->next = to;
  }

  free_register_ = RegisterToSlot(start);
  size_ += (end-start);
}

inline Register ScopedRegister::Release() {
  lava_debug( NORMAL , lava_verify(!empty_); );
  Register reg(reg_);
  empty_ = true;
  return reg;
}

inline void ScopedRegister::Reset( const Register& reg ) {
  if(!empty_) {
    generator_->func_scope()->ra()->Drop(reg_);
  }
  reg_ = reg;
}

inline void ScopedRegister::Reset() {
  if(!empty_) {
    generator_->func_scope()->ra()->Drop(reg_);
    empty_ = true;
  }
}

inline bool ScopedRegister::Reset( const Optional<Register>& reg ) {
  if(!empty_) {
    generator_->func_scope()->ra()->Drop(reg_);
  }
  if(reg.Has()) {
    reg_ = reg.Get();
    return true;
  } else {
    empty_ = true;
    return false;
  }
}

inline ScopedRegister::~ScopedRegister() {
  if(!empty_) generator_->func_scope()->ra()->Drop(reg_);
}

inline Scope::Scope( Generator* gen , Scope* p ):
  generator_(gen),
  parent_(p)
{}

inline FunctionScope* Scope::AsFunctionScope() {
  lava_debug(NORMAL,lava_verify(IsFunctionScope()););
  return static_cast<FunctionScope*>(this);
}
inline LexicalScope*  Scope::AsLexicalScope () {
  lava_debug(NORMAL,lava_verify(IsLexicalScope()););
  return static_cast<LexicalScope*>(this);
}

FunctionScope* Scope::GetEnclosedFunctionScope( Scope* scope ) {
  if(scope) {
    scope = scope->parent();
    while(scope && !scope->IsFunctionScope()) {
      scope = scope->parent();
    }
    return scope ? scope->AsFunctionScope() : NULL;
  }
  return NULL;
}

bool LexicalScope::Init( const ast::Chunk& node ) {
  std::uint8_t base;
  const std::size_t len = node.local_vars->size() + node.has_iterator;
  if(!func_scope()->ra()->EnterScope(len,&base))
    return false;

  for( std::size_t i = 0; i < node.local_vars->size(); ++i ) {
    local_vars_.push_back(
        LocalVar(node.local_vars->Index(i)->name,base + i) );
  }

  if(node.has_iterator) {
    lava_debug(NORMAL,lava_verify(is_loop_););
    const std::uint8_t idx =
      base + static_cast<std::uint8_t>(node.local_vars->size());
    iterator_.Set(Register(idx));
  }

  return true;
}

void LexicalScope::Init( const ast::Function& node ) {
  if(!node.proto->empty()) {
    const std::size_t len = node.proto->size();
    func_scope()->ra()->ReserveFuncArg(len);
    for( std::size_t i = 0 ; i < len ; ++i ) {
      local_vars_.push_back(LocalVar((node.proto->Index(i)->name),
            static_cast<std::uint8_t>(i)));
    }
  }
}

inline bool LexicalScope::DefineLocalVar( const zone::String& name ,
                                          const Register& reg ) {
  lava_debug( NORMAL , lava_verify(GetLocalVar(name).Has()); );
  local_vars_.push_back( LocalVar( &name , reg ) );
  return true;
}

inline Optional<Register> LexicalScope::GetLocalVar( const zone::String& name ) {
  std::vector<LocalVar>::iterator itr =
    std::find( local_vars_.begin() , local_vars_.end() , name );
  return itr  == local_vars_.end() ? Optional<Register>() :
                                     Optional<Register>( itr->reg );
}

inline bool LexicalScope::AddBreak( const ast::Break& node ) {
  BytecodeBuilder::Label l( func_scope()->bb()->brk(node.sci()) );
  if(!l) return false;
  if(is_loop_)
    break_list_.push_back(l);
  else
    GetNearestLoopScope()->break_list_.push_back(l);
  return true;
}

inline bool LexicalScope::AddContinue( const ast::Continue& node ) {
  BytecodeBuilder::Label l( func_scope()->bb()->cont(node.sci()) );
  if(!l) return false;
  if(is_loop_)
    continue_list_.push_back(l);
  else
    GetNearestLoopScope()->continue_list_.push_back(l);
  return true;
}

void LexicalScope::PatchBreak( std::uint16_t pos ) {
  for( auto &e : break_list_ ) {
    e.Patch(pos);
  }
}

void LexicalScope::PatchContinue( std::uint16_t pos ) {
  for( auto &e : continue_list_ ) {
    e.Patch(pos);
  }
}

LexicalScope::LexicalScope( Generator* gen , bool loop ):
  Scope      (gen,gen->lexical_scope_ ? static_cast<Scope*>(gen->lexical_scope_):
                                        static_cast<Scope*>(gen->func_scope_)),
  local_vars_(),
  is_loop_   (loop),
  is_in_loop_(gen->lexical_scope_ ?
              (gen->lexical_scope_->IsLoop() || gen->lexical_scope_->IsInLoop()) : false),
  break_list_(),
  continue_list_(),
  func_scope_(gen->func_scope()),
  iterator_  () {

  lava_debug(NORMAL,
      if(!func_scope()->lexical_scope_list_.empty()) {
        LelxicalScope* back = func_scope()->lexical_scope_list_.back();
        lava_verify(back == parent());
      }
    );

  func_scope()->lexical_scope_list_.push_back(this);
  gen->lexical_scope_ = this;
}

LexicalScope::~LexicalScope() {
  lava_debug(NORMAL,
      lava_verify(func_scope()->lexical_scope_list.back() == this);
      );

  if(!local_vars_.empty())
    func_scope()->ra()->LeaveScope();

  func_scope()->lexical_scope_list_.pop_back();
  generator()->lexical_scope_ = parent()->IsFunctionScope() ? NULL :
                                                              parent()->AsLexicalScope();
}

LexicalScope* LexicalScope::GetNearestLoopScope() {
  Scope* scope = this;
  do {
    if(scope->IsLexicalScope() && scope->AsLexicalScope()->IsLoop())
      return scope->AsLexicalScope();
  } while((scope = scope->parent()));
  return NULL;
}

int FunctionScope::GetUpValue( const zone::String& name ,
                               std::uint16_t* index ) {
  lava_debug(NORMAL,
      lava_verify(!GetLocalVar(name).Has());
      );

  if(FindUpValue(name,index)) {
    return UV_SUCCESS;
  } else {
    FunctionScope* scope = GetEnclosedFunctionScope(this);
    std::vector<FunctionScope*> scopes;
    scopes.push_back(this);

    while(scope) {
      // find the name inside of upvalue slot
      if(scope->FindUpValue(name,index)) {
        // find the name/symbol as upvalue in the |scope|
        for( std::vector<FunctionScope*>::reverse_iterator itr =
            scopes.rbegin() ; itr != scopes.rend() ; ++itr ) {
          std::uint16_t idx;
          if(!scope->bb()->AddUpValue(UV_DETACH,*index,&idx))
            return UV_FAILED;
          scope->AddUpValue(name,idx);
          *index = idx;
        }
        return UV_SUCCESS;
      }

      Optional<Register> reg(scope->GetLocalVar(name));
      if(reg) {
        {
          FunctionScope* last = scopes.back();
          if(!last->bb()->AddUpValue(UV_EMBED,reg.Get().index(),index))
            return UV_FAILED;
          last->AddUpValue(name,*index);
        }
        std::vector<FunctionScope*>::reverse_iterator itr = ++scopes.rbegin();
        for( ; itr != scopes.rend() ; ++itr ) {
          std::uint16_t idx;
          if(!scope->bb()->AddUpValue(UV_DETACH,*index,&idx))
            return UV_FAILED;
          scope->AddUpValue(name,idx);
          *index = idx;
        }
        return UV_SUCCESS;
      }

      // record the scopes inside of the scopes array
      scopes.push_back(scope);

      // move to previous function scope
      scope = GetEnclosedFunctionScope(scope);
    }

    return UV_NOT_EXISTED;
  }
}

bool FunctionScope::FindUpValue( const zone::String& name ,
                                 std::uint16_t* index ) {
  std::vector<UpValue>::iterator
    ret = std::find( upvalue_.begin() , upvalue_.end() , name );
  if(ret == upvalue_.end()) return false;
  *index = ret->index;
  return true;
}

void FunctionScope::AddUpValue ( const zone::String& name ,
                                 std::uint16_t index ) {
  upvalue_.push_back(UpValue(&name,index));
}

Optional<Register> FunctionScope::GetLocalVar( const zone::String& name ) {
  for( auto &e : lexical_scope_list_ ) {
    Optional<Register> r(e->GetLocalVar(name));
    if(r) return r;
  }
  return Optional<Register>();
}

inline FunctionScope::FunctionScope( Generator* gen , const ast::Function& node ):
  Scope(gen,gen->lexical_scope_),
  bb_  (),
  ra_  (),
  upvalue_ (),
  lexical_scope_list_(),
  body_(node.body)
{
  gen->func_scope_ = this;
  gen->lexical_scope_ = NULL;
}

inline FunctionScope::FunctionScope( Generator* gen , const ast::Chunk& node ):
  Scope(gen,gen->lexical_scope_),
  bb_  (),
  ra_  (),
  upvalue_ (),
  lexical_scope_list_(),
  body_(&node)
{
  gen->func_scope_ = this;
  gen->lexical_scope_ = NULL;
}

inline FunctionScope::~FunctionScope() {
  lava_debug(NORMAL,
      lava_verify( lexical_scope_list.empty() );
      lava_verify( parent() ? parent()->IsLexicalScope() : true );
   );
  generator()->lexical_scope_ = parent() ? parent()->AsLexicalScope() : NULL;
  if(parent()) {
    lava_verify(parent()->IsLexicalScope());
    generator()->func_scope_ = parent()->AsLexicalScope()->func_scope();
  } else {
    generator()->func_scope_ = NULL;
  }
}

std::uint8_t kBinSpecialOpLookupTable [][3][3] = {
  /* arithmetic operator */
  {
    {BC_HLT,BC_ADDIV,BC_ADDVI},
    {BC_HLT,BC_ADDRV,BC_ADDVR}
  },
  {
    {BC_HLT,BC_SUBIV,BC_SUBVI},
    {BC_HLT,BC_SUBRV,BC_SUBVR}
  },
  {
    {BC_HLT,BC_MULIV,BC_MULVI},
    {BC_HLT,BC_MULRV,BC_MULVR}
  },
  {
    {BC_HLT,BC_DIVIV,BC_DIVVI},
    {BC_HLT,BC_DIVRV,BC_DIVVR}
  },
  {
    {BC_HLT,BC_MODIV,BC_MODVI},
    {BC_HLT,BC_HLT,BC_HLT}
  },
  {
    {BC_HLT,BC_POWIV,BC_POWVI},
    {BC_HLT,BC_POWRV,BC_POWVR}
  },
  /* comparison operator */
  {
    {BC_HLT,BC_LTIV,BC_LTVI},
    {BC_HLT,BC_LTRV,BC_LTVR},
    {BC_HLT,BC_LTSV,BC_LTVS}
  },
  {
    {BC_HLT,BC_LEIV,BC_LEVI},
    {BC_HLT,BC_LERV,BC_LEVR},
    {BC_HLT,BC_LESV,BC_LEVS}
  },
  {
    {BC_HLT,BC_GTIV,BC_GTVI},
    {BC_HLT,BC_GTRV,BC_GTVR},
    {BC_HLT,BC_GTSV,BC_GTVS}
  },
  {
    {BC_HLT,BC_GEIV,BC_GEVI},
    {BC_HLT,BC_GERV,BC_GEVR},
    {BC_HLT,BC_GESV,BC_GEVS}
  },
  {
    {BC_HLT,BC_EQIV,BC_EQVI},
    {BC_HLT,BC_EQRV,BC_EQVR},
    {BC_HLT,BC_EQSV,BC_EQVS},
  },
  {
    {BC_HLT,BC_NEIV,BC_NEVI},
    {BC_HLT,BC_NERV,BC_NEVR},
    {BC_HLT,BC_NESV,BC_NEVS}
  }
};

static int kBinGeneralOpLookupTable [] = {
  BC_ADDVV,
  BC_SUBVV,
  BC_MULVV,
  BC_DIVVV,
  BC_MODVV,
  BC_POWVV,
  BC_LTVV,
  BC_LEVV,
  BC_GTVV,
  BC_GEVV,
  BC_EQVV,
  BC_NEVV
};

inline Generator::Generator( Context* context , const ast::Root& root ,
                                                ScriptBuilder* sb,
                                                std::string* error ):
  func_scope_(NULL),
  lexical_scope_(NULL),
  script_builder_(sb),
  context_(context),
  root_(&root),
  error_(error)
{}

inline Generator::BinOperandType
Generator::GetBinOperandType( const ast::Literal& node ) const {
  switch(node.literal_type) {
    case ast::Literal::LIT_INTEGER: return TINT;
    case ast::Literal::LIT_REAL: return TREAL;
    case ast::Literal::LIT_STRING: return TSTR;
    default: lava_unreach(""); return TINT;
  }
}

inline const char* Generator::GetBinOperandTypeName( BinOperandType t ) const {
  switch(t) {
    case TINT : return "int";
    case TREAL: return "real";
    default:    return "string";
  }
}

inline bool Generator::GetBinBytecode( const SourceCodeInfo& sci ,
                                       const Token& tk ,
                                       BinOperandType type ,
                                       bool lhs ,
                                       bool rhs ,
                                       Bytecode* output ) const {
  lava_debug(NORMAL, lava_verify(!(rhs && lhs)); );

  int index = static_cast<int>(rhs) << 1 | static_cast<int>(rhs);
  int opindex = static_cast<int>(tk.token());
  Bytecode bc = static_cast<Bytecode>(
      kBinSpecialOpLookupTable[opindex][static_cast<int>(type)][index]);
  if(bc == BC_HLT) {
    Error(sci,"binary operator %s cannot between type %s",
          tk.token_name(),GetBinOperandTypeName(type));

    return false;
  }

  lava_debug(NORMAL, lava_verify(bc >= 0 && bc <= BC_NEVV); );

  *output = bc;
  return true;
}


Optional<Register> Generator::SpillFromAcc( const SourceCodeInfo& sci ) {
  Optional<Register> reg(func_scope()->ra()->Grab());
  if(!reg) {
    Error(ERR_REGISTER_OVERFLOW,sci);
    return reg;
  }
  if(!func_scope()->bb()->move(sci,reg.Get().index(),Register::kAccIndex)) {
    Error(ERR_FUNCTION_TOO_LONG,sci);
    return Optional<Register>();
  }
  return reg;
}

bool Generator::SpillToAcc( const SourceCodeInfo& sci , ScopedRegister* reg ) {
  lava_debug(NORMAL,lava_verify(*reg););
  EEMIT(move(sci,Register::kAccIndex,reg->Get().index()));
  reg->Reset();
  return true;
}

bool Generator::AllocateLiteral( const ast::Literal& lit , const Register& reg ) {
  switch(lit.literal_type) {
    case ast::Literal::LIT_INTEGER:
      if(lit.int_value == 0) {
        EEMIT(load0(lit.sci(),reg.index()));
      } else if(lit.int_value == 1) {
        EEMIT(load1(lit.sci(),reg.index()));
      } else if(lit.int_value == -1) {
        EEMIT(loadn1(lit.sci(),reg.index()));
      } else {
        std::int32_t iref = func_scope()->bb()->Add(lit.int_value);
        if(iref<0) {
          Error(ERR_TOO_MANY_LITERALS,lit.sci());
          return false;
        }
        EEMIT(loadi(lit.sci(),reg.index(),static_cast<std::uint16_t>(iref)));
      }
      break;
    case ast::Literal::LIT_REAL:
      {
        std::int32_t rref = func_scope()->bb()->Add(lit.real_value);
        if(rref<0) {
          Error(ERR_TOO_MANY_LITERALS,lit.sci());
          return false;
        }
        EEMIT(loadr(lit.sci(),reg.index(),static_cast<std::uint16_t>(rref)));
      }
      break;
    case ast::Literal::LIT_BOOLEAN:
      if(lit.bool_value) {
        EEMIT(loadtrue(lit.sci(),reg.index()));
      } else {
        EEMIT(loadfalse(lit.sci(),reg.index()));
      }
      break;
    case ast::Literal::LIT_STRING:
      {
        std::int32_t sref = func_scope()->bb()->Add(*lit.str_value,context_->gc());
        if(sref<0) {
          Error(ERR_TOO_MANY_LITERALS,lit.sci());
          return false;
        }
        EEMIT(loadstr(lit.sci(),reg.index(),static_cast<std::uint16_t>(sref)));
      }
      break;
    default:
      EEMIT(loadnull(lit.sci(),reg.index()));
      break;
  }
  return true;
}

Optional<Register> Generator::ExprResultToRegister( const SourceCodeInfo& sci ,
                                                    const ExprResult& expr ) {
  if(expr.IsReg())
    return Optional<Register>(expr.reg());
  else {
    Optional<Register> r(func_scope()->ra()->Grab());
    if(!r) {
      Error(ERR_REGISTER_OVERFLOW,sci);
      return r;
    }
    switch(expr.kind()) {
      case KINT:
        if(!func_scope()->bb()->loadi(
              sci,r.Get().index(),static_cast<std::uint16_t>(expr.ref()))) {
          Error(ERR_FUNCTION_TOO_LONG,sci);
          return Optional<Register>();
        }
        break;
      case KREAL:
        if(!func_scope()->bb()->loadr(
              sci,r.Get().index(),static_cast<std::uint16_t>(expr.ref()))) {
          Error(ERR_FUNCTION_TOO_LONG,sci);
          return Optional<Register>();
        }
        break;
      case KSTR:
        if(!func_scope()->bb()->loadstr(
              sci,r.Get().index(),static_cast<std::uint16_t>(expr.ref()))) {
          Error(ERR_FUNCTION_TOO_LONG,sci);
          return Optional<Register>();
        }
        break;
      case KTRUE:
        if(!func_scope()->bb()->loadtrue(sci,r.Get().index())) {
          Error(ERR_FUNCTION_TOO_LONG,sci);
          return Optional<Register>();
        }
        break;
      case KFALSE:
        if(!func_scope()->bb()->loadfalse(sci,r.Get().index())) {
          Error(ERR_FUNCTION_TOO_LONG,sci);
          return Optional<Register>();
        }
        break;
      default:
        if(!func_scope()->bb()->loadnull(sci,r.Get().index())) {
          Error(ERR_FUNCTION_TOO_LONG,sci);
          return Optional<Register>();
        }
        break;
    }
    return Optional<Register>(r.Get());
  }
}

const char* Generator::GetErrorCategoryDescription( ErrorCategory ec ) const {
#define __(A,B) B,
  static const char* kDescription[] = {
    BYTECODE_COMPILER_ERROR_LIST(__)
    NULL
  };
#undef __ // __
  return kDescription[ec];
}

void Generator::Error( const SourceCodeInfo& sci , const char* fmt , ... ) const {
  va_list vl;
  va_start(vl,fmt);
  ReportErrorV(error_,"[bytecode-compiler]",script_builder_->source().c_str(),
                                            sci.start,
                                            sci.end,
                                            fmt,
                                            vl);
}

void Generator::Error( ErrorCategory ec , const ast::Node& node ,
                                          const char* fmt ,
                                          ... ) const {
  va_list vl;
  va_start(vl,fmt);
  ReportError(error_,"[bytecode-compiler]",script_builder_->source().c_str(),
                                           node.sci().start,
                                           node.sci().end,
                                           "%s:%s",
                                           GetErrorCategoryDescription(ec),
                                           FormatV(fmt,vl).c_str());
}

void Generator::Error( ErrorCategory ec , const ast::Node& node ) const {
  ReportError(error_,"[bytecode-compiler]",script_builder_->source().c_str(),
                                           node.sci().start,
                                           node.sci().end,
                                           "%s",
                                           GetErrorCategoryDescription(ec));
}

void Generator::Error( ErrorCategory ec , const SourceCodeInfo& sci ) const {
  ReportError(error_,"[bytecode-compiler]",script_builder_->source().c_str(),
                                           sci.start,
                                           sci.end,
                                           "%s",
                                           GetErrorCategoryDescription(ec));
}

/* ----------------------------------------
 * Expression                             |
 * ---------------------------------------*/
bool Generator::Visit( const ast::Literal& lit , ExprResult* result ) {
  std::int32_t ref;
  switch(lit.literal_type) {
    case ast::Literal::LIT_INTEGER:
      if((ref=func_scope()->bb()->Add(lit.int_value))<0) {
        Error(ERR_REGISTER_OVERFLOW,lit);
        return false;
      }
      result->SetIRef(ref);
      return true;
    case ast::Literal::LIT_REAL:
      if((ref=func_scope()->bb()->Add(lit.real_value))<0) {
        Error(ERR_REGISTER_OVERFLOW,lit);
        return false;
      }
      result->SetRRef(ref);
      return true;
    case ast::Literal::LIT_BOOLEAN:
      if(lit.bool_value)
        result->SetTrue();
      else
        result->SetFalse();
      return true;
    case ast::Literal::LIT_STRING:
      if((ref = func_scope()->bb()->Add(*lit.str_value,context_->gc()))<0) {
        Error(ERR_REGISTER_OVERFLOW,lit);
        return false;
      }
      result->SetSRef(ref);
      return true;
    default:
      result->SetNull();
      return true;
  }
}

bool Generator::Visit( const ast::Variable& var , ExprResult* result ) {
  // 1. Try to establish it as local variable
  Optional<Register> reg;
  std::uint16_t upindex = 0;

  if((reg=lexical_scope_->GetLocalVar(*var.name))) {
    result->SetRegister( reg.Get() );
  } else if(func_scope_->GetUpValue(*var.name,&upindex)) {
    EEMIT(uvget(var.sci(),Register::kAccIndex,upindex));
    result->SetAcc();
  } else {
    // It is a global variable so we need to EEMIT global variable stuff
    std::int32_t ref = func_scope()->bb()->Add(*var.name,context_->gc());
    if(ref<0) {
      Error(ERR_REGISTER_OVERFLOW,var);
      return false;
    }

    // Hold the global value inside of Acc register since we can
    EEMIT(gget(var.sci(),Register::kAccIndex,ref));
    result->SetAcc();
  }
  return true;
}

bool Generator::VisitPrefix( const ast::Prefix& node , std::size_t end ,
                                                       bool tcall ,
                                                       Register* result ) {
  // Allocate register for the *var* field in node
  Register var_reg;
  if(!VisitExpression(*node.var,&var_reg)) return false;

  // Handle the component part
  const std::size_t len = node.list->size();
  lava_verify(end <= len);

  for( std::size_t i = 0 ; i < end ; ++i ) {
    const ast::Prefix::Component& c = node.list->Index(i);
    switch(c.t) {
      case ast::Prefix::Component::DOT:
        {
          // Get the string reference
          std::int32_t ref = func_scope()->bb()->Add(
              *c.var->name , context_ ->gc() );
          if(ref<0) {
            Error(ERR_REGISTER_OVERFLOW,*c.var);
            return false;
          }
          // Use PROPGET instruction
          EEMIT(propget(c.var->sci(),var_reg.index(),ref));

          // Since PROPGET will put its value into the Acc so afterwards
          // the value will be held in scratch registers until we meet a
          // function call since function call will just mess up the
          // scratch register. We don't have spill in register
          if(!var_reg.IsAcc()) {
            EEMIT(move(c.var->sci(),Register::kAccIndex,var_reg.index()));
            func_scope()->ra()->Drop(var_reg);
            var_reg.SetAcc();
          }
        }
        break;
      case ast::Prefix::Component::INDEX:
        // Specialize the integer literal if we can do so
        if(c.expr->IsLiteral() && c.expr->AsLiteral()->IsInteger()) {
          // Okay it is a index looks like this : a[100]
          // so we gonna specialize this one with 100 be a direct ref
          // no register shuffling here
          std::int32_t ref = func_scope()->bb()->Add(
              c.expr->AsLiteral()->int_value);
          if(ref<0) {
            Error(ERR_FUNCTION_TOO_LONG,*c.expr);
            return false;
          }

          // Use idxgeti bytecode to get the stuff and by default value
          // are stored in ACC register
          EEMIT(idxgeti(c.expr->sci(),var_reg.index(),ref));

          // Drop register if it is not Acc
          if(!var_reg.IsAcc()) { func_scope()->ra()->Drop(var_reg); var_reg.SetAcc(); }
        } else {
          // Register used to hold the expression
          ScopedRegister expr_reg(this);

          // We need to spill ACC register if our current value are held inside
          // of ACC register due to *this* expression evaluating may use Acc.
          if(var_reg.IsAcc()) {
            Optional<Register> new_reg(SpillFromAcc(c.expr->sci()));
            if(!new_reg) return false;
            var_reg = new_reg.Get();
          }

          // Get the register for the expression
          if(!VisitExpression(*c.expr,&expr_reg)) return false;

          // EEMIT the code for getting the index and the value is stored
          // inside of the ACC register
          EEMIT(idxget(c.expr->sci(),var_reg.index(),expr_reg.Get().index()));

          // Drop register if it is not in Acc
          if(!var_reg.IsAcc()) {
            EEMIT(move(c.var->sci(),Register::kAccIndex,var_reg.index()));
            func_scope()->ra()->Drop(var_reg);
            var_reg.SetAcc();
          }
        }
        break;
      default:
        {
          // now for function call here
          // 1. Move the acc register to antoher temporary register due to the
          //    fact that acc will be *scratched* when evaluating the argument
          //    expression
          if(var_reg.IsAcc()) {
            Optional<Register> reg = SpillFromAcc(c.fc->sci());
            if(!reg) return false;
            // hold the new register
            var_reg = reg.Get();
          }

          // 2. Visit each argument and get all its related registers
          const std::size_t len = c.fc->args->size();
          std::uint8_t base = 255;

          for( std::size_t i = 0; i < len; ++i ) {
            Register reg;
            if(!VisitExpression(*c.fc->args->Index(i),&reg)) return false;
            if(reg.IsAcc()) {
              Optional<Register> r(SpillFromAcc(c.fc->sci()));
              if(!r) return false;
              reg = r.Get();
            }

            lava_debug(NORMAL,
                if(base != 255) {
                lava_verify(prev_index+1 == reg.index());
                }
              );

            if(base == 255) base = reg.index();
          }

          // 3. Generate call instruction based on the argument size for
          //    sepcialization , this save us some decoding time when function
          //    call number is small ( <= 2 ).

          // Check whether we can use tcall or not. The tcall instruction
          // *must be* for the last component , since then we know we will
          // forsure generate a ret instruction afterwards
          const bool tc = tcall && (i == (len-1));
          if(tc) {
            EEMIT(tcall(c.fc->sci(),
                        static_cast<std::uint8_t>(c.fc->args->size()),
                        var_reg.index(),
                        base));
          } else {
            EEMIT(call(c.fc->sci(),
                       static_cast<std::uint8_t>(c.fc->args->size()),
                       var_reg.index(),
                       base));
          }

          // 4. the result will be stored inside of Acc
          if(!var_reg.IsAcc()) {
            EEMIT(move(c.var->sci(),Register::kAccIndex,var_reg.index()));
            func_scope()->ra()->Drop(var_reg);
            var_reg.SetAcc();
          }

          // 5. free all temporary register used by the func-call
          for( std::size_t i = 0 ; i < len ; ++i ) {
            func_scope()->ra()->Drop(Register(static_cast<std::uint8_t>(base+i)));
          }
        }
        break;
    }
  }

  *result = var_reg;
  return true;
}

bool Generator::VisitPrefix( const ast::Prefix& node , std::size_t end ,
                                                       bool tcall ,
                                                       ScopedRegister* result ) {
  Register reg;
  if(!VisitPrefix(node,end,tcall,&reg)) return false;
  result->Reset(reg);
  return true;
}

bool Generator::Visit( const ast::Prefix& node , ExprResult* result ) {
  Register r;
  if(!VisitPrefix(node,node.list->size(),false,&r)) return false;
  result->SetRegister(r);
  return true;
}

bool Generator::Visit( const ast::List& node , const Register& reg ,
                                               ExprResult* result ) {
  const std::size_t entry_size = node.entry->size();

  if(entry_size == 0) {
    EEMIT(loadlist0(node.sci(),reg.index()));
    result->SetRegister(reg);
  } else if(entry_size == 1) {
    ScopedRegister r1(this);
    if(!VisitExpression(*node.entry->Index(0),&r1)) return false;
    EEMIT(loadlist1(node.sci(),reg.index(),r1.Get().index()));
    result->SetRegister(reg);
  } else if(entry_size == 2) {
    ScopedRegister r1(this) , r2(this);
    if(!VisitExpression(*node.entry->Index(0),&r1)) return false;
    if(r1.Get().IsAcc()) {
      if(!r1.Reset(SpillFromAcc(node.sci()))) return false;
    }
    if(!VisitExpression(*node.entry->Index(1),&r2)) return false;
    EEMIT(loadlist2(node.sci(),reg.index(),r1.Get().index(),
                                           r2.Get().index()));
    result->SetRegister(reg);
  } else {
    Register r;
    if(reg.IsAcc()) {
      // We are trying to use ACC register here for long version
      // of list . This is not very efficient , so we spill the
      // register out of ACC.
      Optional<Register> new_reg(func_scope_->ra()->Grab());
      if(!new_reg) {
        Error(ERR_REGISTER_OVERFLOW,node);
        return false;
      }
      r = new_reg.Get();
    } else
      r = reg;

    /**
     * When list has more than 2 entries, the situation becomes a little
     * bit complicated. The way we generate the list literal cannot relay
     * on the xarg since xarg requires the entries to be held inside of the
     * registers which has at most 256 registers. This number is fine for
     * function argument but too small for list entries. Therefore the bytecode
     * allows us to use two set of instructions to perform this job. This is
     * not ideal but this allow better flexibility
     */
    EEMIT(newlist(node.sci(),r.index(),static_cast<std::uint16_t>(entry_size)));

    // Go through each list entry/element
    for( std::size_t i = 0 ; i < entry_size ; ++i ) {
      ScopedRegister r1(this);
      const ast::Node& e = *node.entry->Index(i);
      if(!VisitExpression(e,&r1)) return false;
      EEMIT(addlist(e.sci(),r.index(),r1.Get().index()));
    }
    result->SetRegister(r);
  }
  return true;
}

bool Generator::Visit( const ast::Object& node , const Register& reg ,
                                                 ExprResult* result ) {
  const std::size_t entry_size = node.entry->size();
  if(entry_size == 0) {
    EEMIT(loadobj0(node.sci(),reg));
    result->SetRegister(reg);
  } else if(entry_size == 1) {
    ScopedRegister k(this);
    ScopedRegister v(this);
    const ast::Object::Entry& e = node.entry->Index(0);
    if(!VisitExpression(*e.key,&k)) return false;
    if(k.Get().IsAcc()) {
      if(!k.Reset(SpillFromAcc(node.sci()))) return false;
    }
    if(!VisitExpression(*e.val,&v)) return false;
    EEMIT(loadobj1(node.sci(),reg.index(),k.Get().index(),v.Get().index()));
    result->SetRegister(reg);
  } else {
    Register r;
    if(reg.IsAcc()) {
      Optional<Register> new_reg( func_scope()->ra()->Grab() );
      if(!new_reg) {
        Error(ERR_REGISTER_OVERFLOW,node);
        return false;
      }
      r = new_reg.Get();
    } else
      r = reg;

    EEMIT(newobj(node.sci(),reg.index(),
          static_cast<std::uint16_t>(entry_size)));

    for( std::size_t i = 0 ; i < entry_size ; ++i ) {
      const ast::Object::Entry& e = node.entry->Index(i);
      ScopedRegister k(this);
      ScopedRegister v(this);
      if(!VisitExpression(*e.key,&k)) return false;
      if(k.Get().IsAcc()) {
        if(!k.Reset(SpillFromAcc(e.key->sci()))) return false;
      }
      if(!VisitExpression(*e.val,&v)) return false;
      EEMIT(addobj(e.key->sci(),r.index(),k.Get().index(),
                                          v.Get().index()));
    }

    result->SetRegister(reg);
  }
  return true;
}

bool Generator::Visit( const ast::Unary& node , ExprResult* result ) {
  Register reg;
  if(!VisitExpression(*node.opr,&reg)) return false;
  if(node.op == Token::kSub) {
    EEMIT(negate(node.sci(),reg.index()));
  } else {
    EEMIT(not_(node.sci(),reg.index()));
  }
  result->SetRegister(reg);
  return true;
}

/**
 * The logic kinds of have some register allocating complexity due to the
 * logic value's two component doesn't evaluate at the same time which means
 * the result can be in different register.
 * However we need to agree on a certain register to hold the value. The easist
 * way to do this is carried out the value through Acc register.
 * So any logic expression's result will be in Acc register
 */
bool Generator::VisitLogic( const ast::Binary& node , ExprResult* result ) {
  ScopedRegister lhs(this);
  // Left hand side
  if(!VisitExpression(*node.lhs,&lhs)) return false;
  if(!lhs.Get().IsAcc())
    if(!SpillToAcc(node.sci(),&lhs))
      return false;

  // And or Or instruction
  BytecodeBuilder::Label label;

  if(node.op == Token::kAnd) {
    label = func_scope()->bb()->and_(node.lhs->sci());
  } else {
    label = func_scope()->bb()->or_(node.lhs->sci());
  }
  if(!label) {
    Error(ERR_FUNCTION_TOO_LONG,func_scope()->body());
    return false;
  }

  // Right hand side expression evaluation
  ScopedRegister rhs(this);
  if(!VisitExpression(*node.rhs,&rhs)) return false;
  if(!rhs.Get().IsAcc())
    if(!SpillToAcc(node.rhs->sci(),&rhs))
      return false;

  // Patch the branch label
  label.Patch( func_scope()->bb()->CodePosition() );

  result->SetAcc();
  return true;
}

bool Generator::Visit( const ast::Binary& node , ExprResult* result ) {
  if(node.op.IsArithmetic() || node.op.IsComparison()) {
    if((node.lhs->IsLiteral() && CanBeSpecializedLiteral(*node.lhs->AsLiteral())) ||
       (node.rhs->IsLiteral() && CanBeSpecializedLiteral(*node.rhs->AsLiteral())) ) {

      lava_debug(NORMAL,
          lava_verify(!(node.lhs->IsLiteral() && node.rhs->IsLiteral())); );

      BinOperandType t = node.lhs->IsLiteral() ? GetBinOperandType(*node.lhs->AsLiteral()) :
                                                 GetBinOperandType(*node.rhs->AsLiteral());
      Bytecode bc;

      // Get the bytecode for this expression
      if(!GetBinBytecode(node.sci(),node.op,t,node.lhs->IsLiteral(),
                                              node.rhs->IsLiteral(),
                                              &bc))
        return false;

      // Evaluate each operand and its literal value
      if(node.lhs->IsLiteral()) {
        ScopedRegister rhs_reg(this);
        if(!VisitExpression(*node.rhs,&rhs_reg)) return false;

        // Get the reference for the literal
        ExprResult lhs_expr;
        if(!SpecializedLiteralToExprResult(*node.lhs->AsLiteral(),&lhs_expr))
          return false;

        if(!func_scope()->bb()->EmitC(
              node.sci(),bc,lhs_expr.ref(),rhs_reg.Get().index())) {
          Error(ERR_FUNCTION_TOO_LONG,func_scope()->body());
          return false;
        }

      } else {
        ScopedRegister lhs_reg(this);
        if(!VisitExpression(*node.lhs,&lhs_reg)) return false;

        ExprResult rhs_expr;
        if(!SpecializedLiteralToExprResult(*node.rhs->AsLiteral(),&rhs_expr))
          return false;

        if(!func_scope()->bb()->EmitB(
              node.sci(),bc,lhs_reg.Get().index(),rhs_expr.ref())) {
          Error(ERR_FUNCTION_TOO_LONG,node);
          return false;
        }
      }
    } else {
      // Well will have to use VV type instruction which is a slow path.
      // This can be anything from using boolean and null for certain
      // arithmetic and comparsion ( which is not allowed ) to using
      // variable as operand for above operands.
      ScopedRegister lhs(this);
      ScopedRegister rhs(this);

      // Due to the fact our parser generate right hand side recursive AST.
      // We visit right hand side first since this will save us on the fly
      // register and avoid needless register overflow in certain cases
      if(!VisitExpression(*node.rhs,&rhs)) return false;
      if(rhs.Get().IsAcc()) {
        if(!rhs.Reset(SpillFromAcc(node.rhs->sci()))) return false;
      }

      if(!VisitExpression(*node.lhs,&lhs)) return false;

      if(!func_scope()->bb()->EmitE(
          node.sci(),
          static_cast<Bytecode>( kBinGeneralOpLookupTable[static_cast<int>(node.op.token())] ),
          lhs.Get().index(),
          rhs.Get().index())) {
        Error(ERR_FUNCTION_TOO_LONG,node);
        return false;
      }
    }

    return true;
  } else {
    lava_debug(NORMAL, lava_verify(node.op.IsLogic()); );
    return VisitLogic(node,result);
  }
}

bool Generator::Visit( const ast::Ternary& node , ExprResult* result ) {
  // Generate code for condition , don't have to be in Acc register
  ScopedRegister reg(this);
  if(!VisitExpression(*node._1st,&reg)) return false;

  // Based on the current condition to branch
  BytecodeBuilder::Label cond_label =
    func_scope()->bb()->jmpf(node.sci(),reg.Get().index());
  if(!cond_label) {
    Error(ERR_FUNCTION_TOO_LONG,node);
    return false;
  }

  // Now 2nd expression generated here , which is natural fallthrough
  ScopedRegister reg_2nd(this);
  if(!VisitExpression(*node._2nd,&reg_2nd)) return false;
  if(!reg_2nd.Get().IsAcc())
    if(!SpillToAcc(node._2nd->sci(),&reg_2nd))
      return false;

  // After the 2nd expression generated it needs to jump/skip the 3rd expression
  BytecodeBuilder::Label label_2nd = func_scope()->bb()->jmp(node._2nd->sci());
  if(!label_2nd) {
    Error(ERR_FUNCTION_TOO_LONG,node);
    return false;
  }

  // Now false / 3nd expression generated here , which is not a natural fallthrough
  // The conditional failed branch should jump here which is the false branch value
  cond_label.Patch(func_scope()->bb()->CodePosition());
  ScopedRegister reg_3rd(this);
  if(!VisitExpression(*node._3rd,&reg_3rd)) return false;
  if(!reg_3rd.Get().IsAcc())
    if(!SpillToAcc(node._3rd->sci(),&reg_3rd))
      return false;

  // Now merge 2nd expression jump label
  label_2nd.Patch(func_scope()->bb()->CodePosition());

  result->SetAcc();
  return true;
}

bool Generator::VisitExpression( const ast::Node& node , ExprResult* result ) {
  switch(node.type) {
    case ast::LITERAL:  return Visit(*node.AsLiteral(),result);
    case ast::VARIABLE: return Visit(*node.AsVariable(),result);
    case ast::PREFIX:   return Visit(*node.AsPrefix(),result);
    case ast::UNARY:    return Visit(*node.AsUnary(),result);
    case ast::BINARY:   return Visit(*node.AsBinary(),result);
    case ast::TERNARY:  return Visit(*node.AsTernary(),result);
    case ast::LIST:     return Visit(*node.AsList(),result);
    case ast::OBJECT:   return Visit(*node.AsObject(),result);
    case ast::FUNCTION:
      if(VisitAnonymousFunction(*node.AsFunction())) {
        result->SetAcc();
        return true;
      }
      return false;
    default:
      lava_unreachF("Disallowed expression with node type %s",node.node_name());
      return false;
  }
}

bool Generator::VisitExpression( const ast::Node& node , Register* result ) {
  ExprResult r;
  if(!VisitExpression(node,&r)) return false;
  ScopedRegister reg(this,ExprResultToRegister(node.sci(),r));
  if(!reg) return false;
  *result = reg.Get();
  return true;
}

bool Generator::VisitExpression( const ast::Node& node , ScopedRegister* result ) {
  Register reg;
  if(!VisitExpression(node,&reg)) return false;
  result->Reset(reg);
  return true;
}

/* ---------------------------------
 * Statement                       |
 * --------------------------------*/
bool Generator::Visit( const ast::Var& node , Register* holder ) {
  /**
   * The local variable will be defined during we setup the lexical scope
   * so here we just need to get the local variable and it *MUST* be existed
   */
  Optional<Register> lhs(lexical_scope()->GetLocalVar(*node.var->name));
  lava_debug(NORMAL, lava_verify(lhs.Has()); );

  if(node.expr) {
    if(node.expr->IsLiteral()) {
      if(!AllocateLiteral(*node.expr->AsLiteral(),lhs.Get())) return false;
    } else if(node.expr->IsList()) {
      ExprResult res;
      if(!Visit(*node.expr->AsList(),lhs.Get(),&res)) return false;
      lava_debug(NORMAL,lava_verify(res.IsReg() && (res.reg() == lhs.Get())););
    } else if(node.expr->IsObject()) {
      ExprResult res;
      if(!Visit(*node.expr->AsObject(),lhs.Get(),&res)) return false;
      lava_debug(NORMAL,lava_verify(res.IsReg() && (res.reg() == lhs.Get())););
    } else {
      ScopedRegister rhs(this);
      if(!VisitExpression(*node.expr,&rhs)) return false;
      SEMIT(move(node.sci(),lhs.Get().index(),rhs.Get().index()));
    }
  } else {
    // put a null to that value as default value
    SEMIT(loadnull(node.sci(),lhs.Get().index()));
  }
  return true;
}

bool Generator::VisitSimpleAssign( const ast::Assign& node ) {
  Optional<Register> r(lexical_scope()->GetLocalVar(*node.lhs_var->name));
  if(!r) {
    Error(ERR_LOCAL_VARIABLE_NOT_EXISTED,node,
          "variable name: %s",node.lhs_var->name->data());
    return false;
  }
  /**
   * Optimize for common case since our expression generator allocate
   * register on demand , this may requires an extra move to move the
   * intermediate register used to hold rhs to lhs's register
   */
  if(node.IsLiteral()) {
    if(!AllocateLiteral(*node.AsLiteral(),r.Get())) return false;
  } else if(node.IsList()) {
    ExprResult res;
    if(!Visit(*node.AsList(),r.Get(),&res)) return false;
    lava_debug(NORMAL,lava_verify(res.IsReg() && (res.reg() == r.Get())););
  } else if(node.IsObject()) {
    ExprResult res;
    if(!Visit(*node.AsObject(),r.Get(),&res)) return false;
    lava_debug(NORMAL,lava_verify(res.IsReg() && (res.reg() == r.Get())););
  } else {
    ScopedRegister rhs(this);
    if(!VisitExpression(*node.lhs_var,&rhs)) return false;
    SEMIT(move(node.sci(),r.Get().index(),rhs.Get().index()));
  }
  return true;
}

bool Generator::VisitPrefixAssign( const ast::Assign& node ) {
  ScopedRegister lhs(this);
  ScopedRegister rhs(this);
  if(!VisitExpression(*node.rhs,&rhs)) return false;
  if(!VisitPrefix(*node.lhs_pref,node.lhs_pref->list->size()-1,false,&lhs)) return false;
  const ast::Prefix::Component& last_comp = node.lhs_pref->list->Last();
  switch(last_comp.t) {
    case ast::Prefix::Component::DOT:
      {
        std::int32_t ref = func_scope()->bb()->Add(*last_comp.var->name,context_->gc());
        if(ref<0) {
          Error(ERR_TOO_MANY_LITERALS,node);
          return false;
        }
        if(!rhs.Get().IsAcc()) {
          SEMIT(move(node.sci(),Register::kAccIndex,rhs.Get().index()));
        }
        SEMIT(propset(node.sci(),lhs.Get().index(),ref));
      }
      break;
    case ast::Prefix::Component::INDEX:
      {
        /**
         * Since we need to evaluate the component expression inside of
         * the IDXSET instruction to feed it. And the evaluation can
         * result in Acc to be scratched. So we need to move the result
         * from the rhs to be in a temporary register and then evaluate
         * the stuff
         */
        if(rhs.Get().IsAcc()) {
          if(rhs.Reset(SpillFromAcc(last_comp.expr->sci()))) return false;
        }

        ScopedRegister expr_reg(this);
        if(!VisitExpression(*last_comp.expr,&expr_reg)) return false;

        // idxset REG REG REG
        SEMIT(idxset(node.sci(),lhs.Get().index(),expr_reg.Get().index(),
                                                  rhs.Get().index()));
      }
      break;
    default:
      lava_unreach("Cannot be in this case ending with a function call");
      return false;
  }
  return true;
}

bool Generator::Visit( const ast::Assign& node ) {
  if(node.lhs_type() == ast::Assign::LHS_VAR) {
    return VisitSimpleAssign(node);
  } else {
    return VisitPrefixAssign(node);
  }
}

bool Generator::Visit( const ast::Call& node ) {
  Register reg;
  if(!VisitPrefix(*node.call,node.call->list->size(),false,&reg)) return false;
  // Must be inside of the Acc since the Prefix is ending with a CALL component
  lava_verify(reg.IsAcc());
  return true;
}

bool Generator::Visit( const ast::If& node ) {
  std::vector<BytecodeBuilder::Label> label_vec;
  BytecodeBuilder::Label prev_jmp;
  const std::size_t len = node.br_list->size();
  for( std::size_t i = 0 ; i < len ; ++i ) {
    const ast::If::Branch& br = node.br_list->Index(i);
    // If we have a previous branch, then its condition should jump
    // to this place
    if(prev_jmp) {
      prev_jmp.Patch(func_scope()->bb()->CodePosition());
    }

    // Generate condition code if we need to
    if(br.cond) {
      ScopedRegister cond(this);
      if(!VisitExpression(*br.cond,&cond)) return false;
      prev_jmp = func_scope()->bb()->jmpf(br.cond->sci(),cond.Get().index());
      if(!prev_jmp) {
        Error(ERR_FUNCTION_TOO_LONG,*br.cond);
        return false;
      }
    } else {
      lava_debug( NORMAL , lava_verify( i == len - 1 ); );
    }

    // Generate code body
    if(!VisitChunk(*br.body,true)) return false;

    // Generate the jump
    if(br.cond)
      label_vec.push_back(func_scope()->bb()->jmp(br.cond->sci()));
  }

  for(auto &e : label_vec)
    e.Patch(func_scope()->bb()->CodePosition());
  return true;
}

bool Generator::VisitForCondition( const ast::For& node ,
                                   const Register& var  ) {
  ExprResult cond;
  if(!VisitExpression(*node._2nd,&cond)) return false;
  switch(cond.kind()) {
    case KINT:
      SEMIT(ltvi(node._2nd->sci(),var.index(),cond.ref()));
      break;
    case KREAL:
      SEMIT(ltvr(node._2nd->sci(),var.index(),cond.ref()));
      break;
    case KSTR:
      SEMIT(ltvs(node._2nd->sci(),var.index(),cond.ref()));
    default:
      {
        ScopedRegister r(this,ExprResultToRegister(node.sci(),cond));
        if(!r) return false;
        SEMIT(ltvv(node._2nd->sci(),var.index(),r.Get().index()));
      }
      break;
  }
  return true;
}

bool Generator::Visit( const ast::For& node ) {
  BytecodeBuilder::Label forward;
  Register induct_reg;

  if(node._2nd) {
    lava_verify(node._1st);
    if(!Visit(*node._1st,&induct_reg)) return false;
    if(!VisitForCondition(node,induct_reg)) return false;
    forward = func_scope()->bb()->fstart( node.sci() , induct_reg.index() );
  } else {
    if(node._1st) {
      if(!Visit(*node._1st,&induct_reg)) return false;
    }
    SEMIT(fevrstart(node.sci())); // Mark for JIT
  }

  /* ------------------------------------------
   * Loop body                                |
   * -----------------------------------------*/
  {
    LexicalScope scope(this,true);
    if(!scope.Init(*node.body)) {
      Error(ERR_REGISTER_OVERFLOW,node);
      return false;
    }

    std::uint16_t header = static_cast<std::uint16_t>(
        func_scope()->bb()->CodePosition());

    if(!VisitChunk(*node.body,false)) return false;

    // Patch all contiune to jump here
    scope.PatchContinue(
        static_cast<std::uint16_t>(func_scope()->bb()->CodePosition()));

    /**
     * Now generate loop header at the bottom of the loop. Basically
     * we have a simple loop inversion for the for loop
     */
    if(node._2nd) {
      if(!VisitForCondition(node,induct_reg)) return false;

      // Jump back to the loop header
      SEMIT(fend(node.sci(),header));
    } else {
      SEMIT(fevrend(node.sci(),header));
    }

    // Patch all break to jump here , basically jumps out of
    // the scope
    scope.PatchBreak(
        static_cast<std::uint16_t>(func_scope()->bb()->CodePosition()));
  }
  if(forward) {
    forward.Patch(
        static_cast<std::uint16_t>(func_scope()->bb()->CodePosition()) );
  }

  return true;
}

bool Generator::Visit( const ast::ForEach& node ) {
  // Get the iterator register
  Register itr_reg(lexical_scope()->GetIterator());

  // Evaluate the interator initial value
  ScopedRegister init_reg(this);
  if(!VisitExpression(*node.iter,&init_reg)) return false;

  SEMIT(move(node.iter->sci(),itr_reg.index(),init_reg.Get().index()));

  // Generate the festart
  BytecodeBuilder::Label forward =
    func_scope()->bb()->festart(node.sci(),itr_reg.index());

  {
    LexicalScope scope(this,true);
    if(!scope.Init(*node.body)) {
      Error(ERR_REGISTER_OVERFLOW,node);
      return false;
    }

    std::uint16_t header = static_cast<std::uint16_t>(
        func_scope()->bb()->CodePosition());

    Optional<Register> v(func_scope()->GetLocalVar(*node.var->name));
    lava_debug(NORMAL,lava_verify(v.Has()););

    // Deref the key from iterator register into the target register
    SEMIT(idref(node.var->sci(),v.Get().index(),itr_reg.index()));

    // Visit the chunk
    if(!VisitChunk(*node.body,false)) return false;

    scope.PatchContinue(
        static_cast<std::uint16_t>(func_scope()->bb()->CodePosition()));

    SEMIT(feend(node.sci(),itr_reg.index(),header));


    scope.PatchBreak(
        static_cast<std::uint16_t>(func_scope()->bb()->CodePosition()));
  }

  forward.Patch(
      static_cast<std::uint16_t>(func_scope()->bb()->CodePosition()));

  return true;
}

bool Generator::Visit( const ast::Continue& node ) {
  if(!lexical_scope()->AddContinue(node)) {
    Error(ERR_FUNCTION_TOO_LONG,node);
    return false;
  }
  return true;
}

bool Generator::Visit( const ast::Break& node ) {
  if(!lexical_scope()->AddBreak(node)) {
    Error(ERR_FUNCTION_TOO_LONG,node);
    return false;
  }
  return true;
}

/*
 * As long as the return value is a function call , then we can
 * tail call optimized it
 */
bool Generator::CanBeTailCallOptimized( const ast::Node& node ) const {
  if(node.IsPrefix()) {
    const ast::Prefix& p = *node.AsPrefix();
    if(p.list->Last().IsCall()) {
      return true;
    }
  }
  return false;
}

bool Generator::Visit( const ast::Return& node ) {
  if(!node.has_return_value()) {
    SEMIT(ret0(node.sci()));
  } else {
    // Look for return function-call() style code and then perform
    // tail call optimization on this case. Basically, as long as
    // the return is returning a function call , then we can perform
    // tail call optimization since we don't need to return to the
    // previous call frame at all.
    if(CanBeTailCallOptimized(*node.expr)) {
      lava_debug(NORMAL, lava_verify(node.expr->IsPrefix()); );

      ScopedRegister ret(this);
      if(!VisitPrefix(*node.expr->AsPrefix(),
                      node.expr->AsPrefix()->list->size(),
                      true, // allow the tail call optimization
                      &ret))
        return false;
      if(!ret.Get().IsAcc())
        if(!SpillToAcc(node.expr->sci(),&ret))
          return false;
    } else {
      ScopedRegister ret(this);
      if(!VisitExpression(*node.expr,&ret)) return false;
      if(!ret.Get().IsAcc())
        if(!SpillToAcc(node.expr->sci(),&ret))
          return false;
    }
    SEMIT(ret(node.sci()));
  }
  return true;
}

bool Generator::VisitStatment( const ast::Node& node ) {
  switch(node.type) {
    case ast::VAR: return Visit(*node.AsVar());
    case ast::ASSIGN: return Visit(*node.AsAssign());
    case ast::CALL: return Visit(*node.AsCall());
    case ast::IF: return Visit(*node.AsIf());
    case ast::FOR: return Visit(*node.AsFor());
    case ast::FOREACH: return Visit(*node.AsForEach());
    case ast::BREAK: return Visit(*node.AsBreak());
    case ast::CONTINUE: return Visit(*node.AsContinue());
    case ast::RETURN: return Visit(*node.AsReturn());
    case ast::FUNCTION: return VisitNamedFunction(*node.AsFunction());
    default: lava_unreachF("Unexpected statement node %s",node.node_name());
  }
  return false;
}

bool Generator::VisitChunkNoLexicalScope( const ast::Chunk& node ) {
  const std::size_t len = node.body->size();
  for( std::size_t i = 0 ; i < len ; ++i ) {
    if(!VisitStatment(*node.body->Index(i))) return false;
  }
  return true;
}

bool Generator::VisitChunk( const ast::Chunk& node , bool scope ) {
  if(scope) {
    LexicalScope scope(this,false);
    if(!scope.Init(node)) {
      Error(ERR_FUNCTION_TOO_LONG,node);
      return false;
    }
    return VisitChunkNoLexicalScope(node);
  }
  return VisitChunkNoLexicalScope(node);
}

Handle<Prototype> Generator::VisitFunction( const ast::Function& node ) {
  FunctionScope scope(this,node);
  {
    LexicalScope body_scope(this,false);
    body_scope.Init(node); // For argument
    if(!body_scope.Init(*node.body)) {
      Error(ERR_REGISTER_OVERFLOW,node);
      return Handle<Prototype>();
    }
    if(!VisitChunk(*node.body,false))
      return Handle<Prototype>();
  }
  return BytecodeBuilder::New(context_->gc(),*scope.bb(),node);
}

bool Generator::VisitNamedFunction( const ast::Function& node ) {
  lava_debug(NORMAL,lava_verify(node.name););
  Handle<Prototype> proto(VisitFunction(node));
  if(proto) {
    if(script_builder_->HasPrototype(*node.name->name)) {
      Error(ERR_FUNCTION_NAME_REDEFINE,node,"function with name %s existed",
                                            node.name->name->data());
      return false;
    }

    std::int32_t idx = script_builder_->AddPrototype(context_->gc(),
                                                     proto,
                                                     *node.name->name);
    if(idx <0) {
      Error(ERR_TOO_MANY_PROTOTYPES,node);
      return false;
    }
    return true;
  }
  return false;
}

bool Generator::VisitAnonymousFunction( const ast::Function& node ) {
  lava_debug(NORMAL,lava_verify(!node.name););
  Handle<Prototype> proto(VisitFunction(node));
  if(proto) {
    std::int32_t idx = script_builder_->AddPrototype(proto);
    if(idx <0) {
      Error(ERR_TOO_MANY_PROTOTYPES,node);
      return false;
    }
    EEMIT(loadcls(node.sci(),static_cast<std::uint16_t>(idx)));
  }
  return false;
}

bool Generator::Generate() {
  FunctionScope scope(this,*root_->body);
  if(!VisitChunk(*root_->body,true)) return false;

  Handle<Prototype> main(
      BytecodeBuilder::New(context_->gc(),*scope.bb()));
  if(!main) return false;

  script_builder_->set_main(main);
  return true;
}
} // namespace

bool GenerateBytecode( Context* context , const ::lavascript::parser::ast::Root& root ,
                                          ScriptBuilder* sb ,
                                          std::string* error ) {
  Generator gen(context,root,sb,error);
  return gen.Generate();
}

} // namespace interpreter
} // namespace lavascript
