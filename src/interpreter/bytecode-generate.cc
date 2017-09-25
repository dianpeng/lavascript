#include "bytecode-generate.h"
#include "bytecode.h"

#include <src/common.h>
#include <src/util.h>
#include <src/script.h>
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
      if(that.has()) Set(that.Get());
    }
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
  bool has() const { return has_; }
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
  inline void Reset( const Register& reg = Register() );
  inline void Reset( const Optional<Register>& reg );
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
 public:
  // The following API are used for local variable reservation
  inline bool EnterScope( std::size_t );
  inline void LeaveScope();
  std::uint8_t base() const { return scope_base_.back(); }
  bool IsReserved( const Registe& reg ) const {
    if(scope_base_.empty()) {
      return false;
    } else {
      return reg.index() < scope_base_.back();
    }
  }
 private:
  static void* const kNotUsed = reinterpret_cast<void*>(0x1);

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

inline Optional<Register> RegisterAllocator::Grab() {
  if(free_register_) {
    Node* ret = free_register_;
    free_register_ = free_register_->next;
    ret->next = static_cast<Node*>(kNotUsed);
    --size_;
    return Optional<Register>(ret->reg);
  }
  return Optional<Register>();
}

inline void RegisterAllocator::Drop( const Register& reg ) {
  if(!reg.IsAcc() && !IsReserved(reg)) {
    Node* n = RegisterToSlot(reg);

#ifdef LAVASCRIPT_CHECK_OBJECTS
    lava_verify(n->next == static_cast<Node*>(kNotUsed));
#endif // LAVASCRIPT_CHECK_OBJECTS

    /**
     * Ensure the register is put in order and then Grab function
     * will always return the least indexed registers
     */
    if(free_register_) {
      if(free_register_->reg.index() > reg.index()) {
        n->next = free_register_;
        free_register_ = n;
      } else {
        for( Node* c = free_register_ ; c ; c = c->next ) {
#ifdef LAVASCRIPT_CHECK_OBJECTS
          lava_verify(c->reg.index() <  reg.index());
#endif // LAVASCRIPT_CHECK_OBJECTS

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

#ifdef LAVASCRIPT_CHECK_OBJECTS
  for( Node* c = free_register_ ; c ; c = c->next ) {
    lava_verify( c->next && c->reg.index() < c->next->reg.index() );
  }
#endif // LAVASCRIPT_CHECK_OBJECTS
}

inline bool RegisterAllocator::IsAvailable( const Register& reg ) {
  Node* n = RegisterToSlot(reg);
  return n->next != static_cast<Node*>(kNotUsed);
}

inline bool RegisterAllocator::IsUsed( const Register& reg ) {
  Node* n = RegisterToSlot(reg);
  return n->next == static_cast<Node*>(kNotUsed);
}

inline RegisterAllocator::Node*
RegisterAllocator::RegisterToSlot( const Register& reg ) {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(!reg.IsAcc());
#endif // LAVASCRIPT_CHECK_OBJECTS
  Node* n = reinterpret_cast<Node*>(reg_buffer_) + reg.index();
  return n;
}

bool RegisterAllocator::EnterScope( std::size_t size ) {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  if(free_register_)
    lava_verify( free_register_->reg.index() == base() + 1 );
#endif // LAVASCRIPT_CHECK_OBJECTS

  if((base() + size) > kAllocatableBytecodeRegisterSize) {
    return false; // Too many registers so we cannot handle it
  } else {
    std::size_t start = base();
    Node* next;
    Node* cur;

    for( cur = free_register_ ; cur ; cur = next ) {

#ifdef LAVASCRIPT_CHECK_OBJECTS
      lava_verify(cur->reg.index() == start);
#endif // LAVASCRIPT_CHECK_OBJECTS

      next = cur->next;
      cur->next = static_cast<Node*>(kNotUsed);
      ++start;
      if(start == base()+size) break;
    }

    size_ -= size;
    free_register_ = cur;
    scope_base_.push_back(base()+size);

    return true;
  }
}

void RegisterAllocator::LeaveScope() {
#ifdef LAVASCRIPT_CHECK_OBJECTS 
  lava_verify(!scope_base_.empty());
  lava_verify(!free_register_ ||
              (free_register_->reg.index() == scope_base_.back()));
#endif // LAVASCRIPT_CHECK_OBJECTS
  std::uint8_t end = scope_base_.back();
  scope_base_.pop();
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
  virtual FunctionScope* AsFunctionScope() = 0;
  virtual LexicalScope*  AsLexicalScope () = 0;

 protected:
  // Get the nearest enclosed function scope
  static FunctionScope* GetEnclosedFunctionScope( Scope* );

  Scope( Generator* gen , Scope* p ):
    generator_(gen),
    parent_(p)
  {}

  virtual ~Scope() {}

 private:
  Generator* generator_;      // generator object
  Scope* parent_;             // Parental scope

  LAVA_DISALLOW_COPY_AND_ASSIGN(Scope);
};

FunctionScope* Scope::GetEnclosedFunctionScope( Scope* scope ) {
  while(scope && !scope->IsFunctionScope()) {
    scope = scope->parent();
  }
  return scope ? scope->AsFunctionScope() : NULL;
}

class LexicalScope : public Scope {
 public:
  inline LexicalScope( Generator* , bool loop );
  inline ~LexicalScope();

  FunctionScope* func_scope() const { return func_scope_; }
 public:
  /* ------------------------------------
   * local variables                    |
   * -----------------------------------*/

  // Define a local variable with the given Register
  bool DefineLocalVar( const zone::String& , const Regiser& );

  // Get a local variable from current lexical scope
  Optional<Register> GetLocalVar( const zone::String& );

  // Size of variables defined in *this* scope
  std::size_t var_size() const;

 public:
  /**----------------------------------------
   * Loop related APIs                      |
   * ---------------------------------------*/
  bool IsLoop() const { return is_loop_; }
  bool IsInLoop() const { return is_in_loop_; }
  bool IsLoopScope() const { return IsLoop() || IsInLoop(); }

  // Find its nearest enclosed loop scope can return *this*
  LexicalScope* GetEnclosedLoopScope() const;

  // Helpers for Break and Continue's jump
  inline bool AddBreak( const ast::Break& );
  inline bool AddContinue( const ast::Continue& );

  inline void PatchBreak( std::uint16_t );
  inline void PatchContinue( std::uint16_t );

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
  bool is_in_loop;

  // Break label
  std::vector<BytecodeBuilder::Label> break_list_;

  // Continue label
  std::vector<BytecodeBuilder::Label> continue_list_;

  // Function scope that enclose this lexical scope
  FunctionScope* func_scope_;


  LAVA_DISALLOW_COPY_AND_ASSIGN(FunctionScope);
};

class FunctionScope {
 public:
  inline FunctionScope( Generator* );
  inline ~FunctionScope();

 public:
  // bytecode builder
  BytecodeBuilder* bb() { return &bb_; }

  // register allocator
  RegisterAllocator* ra() { return &ra_; }
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

 private:
  // Bytecode builder for this Function
  BytecodeBuilder bb_;

  // Register allocator for this Function
  RegisterAllocator ra_;

  // UpValue table for this Function
  struct UpValue {
    const zone::String* name;
    std::uint16_t index;
    UpValue( const zone::String* n , std::uint16_t* i ):name(n),index(i) {}
    bool operator == ( const zone::String& n ) const {
      return *name == n;
    }
  };
  std::vector<UpValue> upvalue_;

  // All enclosed lexical scope at this time
  std::vector<LexicalScope*> lexical_scope_list_;

  friend class LexicalScope;

  LAVA_DISALLOW_COPY_AND_ASSIGN(FunctionScope);
};

int FunctionScope::GetUpValue( const zone::String& name ,
                               std::uint16_t* index ) {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify( !GetLocalVar(name).Has() );
#endif // LAVASCRIPT_CHECK_OBJECTS

  if(FindUpValue(name,index)) {
    return UV_SUCCESS;
  } else {
    FunctionScope* scope = GetEnclosedFunctionScope(this);
    std::vector<FunctionScope*> scopes;
    scopes.push_back(this);

    while(scope) {
      // find the name inside of upvalue slot
      std::uint16_t index;
      if(scope->FindUpValue(name,&indx)) {

        // find the name/symbol as upvalue in the |scope|
        for( std::vector<Scope*>::reverse_iterator itr =
            scopes.rbegin() ; itr != scopes.rend() ; ++itr ) {
          std::uint16_t idx;
          if(!scope->bb()->AddUpValue(UV_DETACH,rindex,&idx))
            return UV_FAILED;
          index = idx;
        }
        return UV_SUCCESS;
      }

      Optional<Register> reg(scope->GetLocalVar(name));
      if(reg) {
        {
          FunctionScope* last = scopes.back();
          if(!last->AddUpValue(UV_EMBED,reg.Get().index(),&index))
            return UV_FAILED; 
        }

        if(scopes.rbegin() != scopes.rend()) {
          std::vector<Scope*>::reverse_iterator itr = ++scopes.rbegin();
          for( ; itr != scopes.rend() ; ++itr ) {
            std::uint16_t idx;
            if(!scope->bb()->AddUpValue(UV_DETACH,index,&idx))
              return UV_FAILED;
            index = idx;
          }
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
  if(ret == upvalue_.end())
    return false;
  *index = ret->index;
  return true;
}

Optional<Register> FunctionScope::GetLocalVar( const zone::String& name ) {
  for( auto &e : lexical_scope_ ) {
    Optional<Register> r(e->GetLocalVar(name));
    if(r) return r;
  }
  return Optional<Register>
}


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
  ExprResult():
    kind_(KNULL),
    ref_(0),
    reg_(0)
  {}

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

 private:
  ExprResultKind kind_;
  std::int32_t ref_;
  Register reg_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(ExprResult);
};

/* ==================================
 * Code emit macro:
 * eemit is for expression level
 * semit is for statement  level
 * =================================*/

#define eemit(RESULT,XX)                           \
  do {                                             \
    auto _ret = func_scope()->bb()->XX;    \
    if(!_ret) {                                    \
      if((RESULT)) ErrorFunctionTooLong((RESULT)); \
      return false;                                \
    }                                              \
  } while(false)

#define semit(RESULT,XX)                           \
  do {                                             \
    auto _ret = func_scope()->bb()->XX;    \
    if(!_ret) {                                    \
      return false;                                \
    }                                              \
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

 public:
  Generator( Context* , const ast::Root& , Script* , std::string* );
  FunctionScope* func_scope() const { return func_scope_; }

  bool Generate();

 private: // Binary bytecode lookup table
  enum BinOperandType{ TINT = 0 , TREAL , TSTR };

  bool CanBeSpecializedLiteral( ast::Literal& lit ) const {
    return lit.IsInteger() || lit.IsReal() || lit.IsString();
  }
  bool CanBeSpecializedLiteral( const ExprResult& expr ) const {
    return expr.IsInteger() || expr.IsReal() || expr.IsString();
  }

  inline BinOperandType GetBinOperandType( const ast::Literal& ) const;

  const char* GetBinOperandTypeName( BinOperandType t ) const {
    switch(t) {
      case TINT : return "int";
      case TREAL: return "real";
      default:    return "string";
    }
  }

  inline bool GetBinBytecode( const Token& tk , BinOperandType type , bool lhs ,
                                                                      bool rhs ,
                                                                      Bytecode* );

 private:
  /* --------------------------------------------
   * Expression Code Generation                 |
   * -------------------------------------------*/
  // Visit function's call argument and generate code for them. Put all the reference
  // register into the reg_set vector
  bool VisitFuncCall( const ast::FuncCall& fc , std::vector<std::uint8_t>* reg_set );
  bool Visit( const ast::Literal& lit , ExprResult* );
  bool Visit( const ast::Variable& var, ExprResult* );
  bool Visit( const ast::Prefix& pref , ExprResult* );
  bool Visit( const ast::Unary&  , ExprResult* );
  bool Visit( const ast::Binary& , ExprResult* );
  bool VisitLogic( const ast::Binary& , ExprResult* );
  bool Visit( const ast::Ternary&, ExprResult* );
  bool Visit( const ast::List&   , ExprResult* );
  bool Visit( const ast::Object& , ExprResult* );

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
  bool Visit( const ast::Foreach& );
  bool Visit( const ast::Break& );
  bool Visit( const ast::Continue& );
  bool CanBeTailCallOptimized( const ast::Node& node ) const;
  bool Visit( const ast::Return& );

  bool VisitStatment( const ast::Node& );

  bool VisitChunkNoLexicalScope( const ast::Chunk& );
  bool VisitChunk( const ast::Chunk& , bool );

  bool VisitNamedFunction( const ast::Function& );
  bool VisitAnonymousFunction( const ast::Function& , ExprResult* );

 private: // Misc helpers --------------------------------------
  // Spill the Acc register to another register
  Optional<Register> SpillFromAcc();
  bool SpillFromAcc(ScopedRegister*);
  void SpillToAcc( const Register& );

  // Allocate literal with in certain registers
  Optional<Register> AllocateLiteral( const ast::Literal& );
  void AllocateLiteral( const ast::Literal& , const Register& );

  // Convert ExprResult to register, it may allocate new register
  // to hold it if it is literal value
  Optional<Register> ExprResultToRegister( const ExprResult& );

 private: // Errors ---------------------------------------------
  void Error( const SourceCodeInfo& , ExprResult* , const char* fmt , ... );

  // The following ErrorXXX function is common or frequently used error report
  // function which captures certain common cases
  void ErrorTooComplicatedFunc( const SourceCodeInfo& , ExprResult* result = NULL );
  void ErrorFunctionTooLong   ( ExprResult* );
  void ErrorLocalVariable     ( const SourceCodeInfo& , const zone::String& );

 private: // RAII for managing lexical scope --------------------
  class EnterLoopScope;
  class EnterLexicalScope;
  class EnterFunctionScope;

  friend class EnterLoopScope;
  friend class EnterLexicalScope;
  friend class EnterFunctionScope;

 private:
  FunctionScope* func_scope_;
  LexicalScope* lexical_scope_;
  RegisterAllocator ra_;
  Context* context_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(Generator);
};

std::uint8_t kBinSpecialOpLookupTable [][][] = {
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
    {GC_HLT,BC_EQSV,BC_EQVS},
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

inline BinOperandType Generator::GetBinOperandType( const ast::Literal& ) const {
  switch(node.literal_type) {
    case ast::Literal::LIT_INTEGER: return TINT;
    case ast::Literal::LIT_REAL: return TREAL;
  }
}

inline bool Generator::GetBinBytecode( const Token& tk , BinOperandType type ,
                                                         bool lhs ,
                                                         bool rhs ,
                                                         Bytecode* output ) {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(!(rhs && lhs));
#endif // LAVASCRIPT_CHECK_OBJECTS

  int index = static_cast<int>(rhs) << 1 | static_cast<int>(rhs);
  int opindex = static_cast<int>(tk.token());
  Bytecode bc = static_cast<Bytecode>(
      kBinSpecialOpLookupTable[opindex][static_cast<int>(type)][index]);
  if(bc == BC_HLT) {
    Error("binary operator %s cannot between type %s", tk.token_name(),
                                                       GetTypeName(type));
    return false;
  }

#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(bc >=0 && bc <= BC_NEVV);
#endif // LAVASCRIPT_CHECK_OBJECTS

  *output = bc;
  return true;
}

/* ----------------------------------------
 * Expression                             |
 * ---------------------------------------*/
bool Generator::Visit( const ast::Literal& lit , ExprResult* result ) {
  std::int32_t ref;
  switch(lit.literal_type) {
    case ast::Literal::LIT_INTEGER:
      if((ref=func_scope()->bb()->Add(lit.int_value))<0) {
        ErrorTooComplicatedFunc(lit.sci(),result);
        return false;
      }
      result->SetIRef(ref);
      return true;
    case ast::Literal::LIT_REAL:
      if((ref=func_scope()->bb()->Add(lit.real_value))<0) {
        ErrorTooComplicatedFunc(lit.sci(),result);
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
      if((ref = func_scope()->bb()->Add(*lit.str_value,context_))<0) {
        ErrorTooComplicatedFunc(lit.sci(),result);
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
  if((reg=cur_scope_->GetLocalVar(*var.name))) {
    result->SetRegister( reg.Get() );
    return true;
  } else if(cur_scope_->GetUpValue()) {
  } else {
    // It is a global variable so we need to eemit global variable stuff
    std::int32_t ref = func_scope_->bb()->Add(*var.name,context_);
    if(ref<0) {
      ErrorTooComplicatedFunc(var.sci(),result);
      return false;
    }

    // Hold the global value inside of Acc register since we can
    eemit(result,gget(var.sci(),Register::kAccIndex,ref));
    result->SetRegister(Register::kAccReg);
    return true;
  }
}

bool Generator::VisitFuncCall( const ast::FuncCall& fc ,
                               std::vector<std::uint8_t>* reg_set ) {
  const std::size_t arg_size = fc.args->size();
  for( std::size_t i = 0 ; i < arg_size ; ++i ) {
    Register reg;
    if(!VisitExpression(*fc.args->Index(i),&reg)) return false;
    reg_set->push_back(reg.index());
  }
  return true;
}

bool Generator::VisitPrefix( const ast::Prefix& node , std::size_t end ,
                                                       bool tcall ,
                                                       Register* result ) {
  // Allocate register for the *var* field in node
  Register var_reg;
  if(!Visit(*node.var,&var_reg)) return false;

  // Handle the component part
  const std::size_t len = node.list->size();
  lava_verify(end <= len);

  for( std::size_t i = 0 ; i < end ; ++i ) {
    const ast::Prefix::Component& c = node.list->Index(i);
    switch(c.t) {
      case ast::Prefix::Component::DOT:
        {
          // Get the string reference
          std::int32_t ref = func_scope()->bb_builder()->Add(
              *c.var->name , context_ );
          if(ref<0) {
            ErrorTooComplicatedFunc(c.var->sci(),result);
            return false;
          }
          // Use PROPGET instruction
          eemit(NULL,propget(c.var->sci(),var_reg.index(),ref));

          // Since PROPGET will put its value into the Acc so afterwards
          // the value will be held in scratch registers until we meet a
          // function call since function call will just mess up the
          // scratch register. We don't have spill in register
          if(!var_reg.IsAcc()) { ra_.Drop(var_reg); var_reg.SetAcc(); }
        }
        break;
      case ast::Prefix::Component::INDEX:
        // Specialize the integer literal if we can do so
        if(c.expr->IsLiteral() && c.expr->AsLiteral()->IsInteger()) {
          // Okay it is a index looks like this : a[100]
          // so we gonna specialize this one with 100 be a direct ref
          // no register shuffling here
          std::int32_t ref = func_scope()->bb_builder()->Add(
              c.expr->AsLiteral()->int_value);
          if(ref<0) {
            ErrorTooComplicatedFunc(c.expr->sci(),result);
            return false;
          }

          // Use idxgeti bytecode to get the stuff and by default value
          // are stored in ACC register
          eemit(NULL,idxgeti(c.expr->sci(),var_reg.index(),ref));

          // Drop register if it is not Acc
          if(!var_reg.IsAcc()) { ra_.Drop(var_reg); var_reg.SetAcc(); }
        } else {
          // Register used to hold the expression
          ScopedRegister expr_reg(this);

          // We need to spill ACC register if our current value are held inside
          // of ACC register due to *this* expression evaluating may use Acc.
          if(var_reg.IsAcc()) {
            Optional<Register> new_reg(SpillFromAcc());
            if(!new_reg) return false;
            var_reg = new_reg.Get();
          }

          // Get the register for the expression
          if(!VisitExpression(*c.expr,&expr_reg)) return false;

          // eemit the code for getting the index and the value is stored
          // inside of the ACC register
          eemit(NULL,idxget(c.expr->sci(),var_reg.index(),expr_reg.index()));

          // Drop register if it is not in Acc
          if(!var_reg.IsAcc()) { ra_.Drop(var_reg); var_reg.SetAcc(); }
        }
        break;
      default:
        {
          // now for function call here
          // 1. Move the acc register to antoher temporary register due to the
          //    fact that acc will be *scratched* when evaluating the argument
          //    expression
          if(var_reg.IsAcc()) {
            Optional<Register> reg = SpillFromAcc();
            if(!reg) return false;
            // hold the new register
            var_reg = reg.Get();
          }

          // 2. Visit each argument and get all its related registers
          std::vector<std::uint8_t> areg_set;
          if(!VisitFuncCall(*c.func,&areg_set)) return false;

          // 3. Generate call instruction based on the argument size for
          //    sepcialization , this save us some decoding time when function
          //    call number is small ( <= 2 ).

          // Check whether we can use tcall or not. The tcall instruction
          // *must be* for the last component , since then we know we will
          // forsure generate a ret instruction afterwards
          const bool tc = tcall && (i == (len-1));

          if(areg_set.size() == 0) {
            if(tc)
              eemit(NULL,tcall0(c.func->sci(),var_reg.index()));
            else
              eemit(NULL,call0(c.func->sci(),var_reg.index()));
          } else if(areg_set.size() == 1) {
            if(tc)
              eemit(result,tall1(c.func->sci(),var_reg.index(),areg_set[0]));
            else
              eemit(result,call1(c.func->sci(),var_reg.index(),areg_set[0]));
          } else if(areg_set.size() == 2) {
            if(tc)
              eemit(NULL,tcall2(c.func->sci(),var_reg.index(),areg_set[0],
                                                              areg_set[1]));
            else
              eemit(NULL,call2(c.func->sci(),var_reg.index(),areg_set[0],
                                                             areg_set[1]));
          } else {
            if(tc)
              eemit(NULL,tcall( c.func->sci() ,
                    static_cast<std::uint32_t>(c.func->args->size()),
                    var_reg.index() ));
            else
              eemit(NULL,call( c.func->sci() ,
                    static_cast<std::uint32_t>(c.func->args->size()),
                    var_reg.index() ));

            eemit(NULL,xarg(areg_set));
          }

          // 4.  the result will be stored inside of Acc
          if(!var_reg.IsAcc()) { ra_.Drop(var_reg); var_reg.SetAcc(); }
        }
        break;
    }
  }

  *result = var_reg;
  return true;
}

bool Generator::Visit( const ast::Prefix& node , std::size_t end ,
                                                 bool tcall ,
                                                 ScopedRegister* result ) {
  Regiser reg;
  if(!Visit(node,end,tcall,&reg)) return false;
  result->SetRegister(ret);
  return true;
}

bool Generator::Visit( const ast::Prefix& node , ExprResult* result ) {
  Register r;
  if(!VisitPrefix(node,node.list->size(),false,&r)) return false;
  result->SetRegister(r);
  return true;
}

bool Generator::Visit( const ast::List& node , ExprResult* result ) {
  const std::size_t entry_size = node.entry->size();

  if(entry_size == 0) {
    eemit(result,loadlist0(node.sci(),Register::kAccReg));
    result->SetAcc();
  } else if(entry_size == 1) {
    ScopedRegister r1(this);
    if(!VisitExpression(*node.entry->Index(0),&r1)) return false;
    eemit(result,loadlist1(node.sci(),Register::kAccReg,r1.Get().index()));
    result->SetAcc();
  } else if(entry_size == 2) {
    ScopedRegister r1(this) , r2(this);
    if(!VisitExpression(*node.entry->Index(0),&r1)) return false;
    if(!VisitExpression(*node.entry->Index(1),&r2)) return false;
    eemit(result,loadlist2(node.sci(),Register::kAccReg,r1.Get().index(),
                                                        r2.Get().index()));
    result->SetAcc();
  } else {

    /**
     * When list has more than 2 entries, the situation becomes a little
     * bit complicated. The way we generate the list literal cannot relay
     * on the xarg since xarg requires the entries to be held inside of the
     * registers which has at most 256 registers. This number is fine for
     * function argument but too small for list entries. Therefore the bytecode
     * allows us to use two set of instructions to perform this job. This is
     * not ideal but this allow better flexibility
     */

    Optional<Register> reg;
    if(!(reg = ra_.Grab())) {
      ErrorTooComplicatedCode(node.sci(),result);
      return false;
    }

    eemit(result,newlist(node.sci(),reg.Get().index(),
          static_cast<std::uint16_t>(entry_size)));

    // Go through each list entry/element
    for( std::size_t i = 0 ; i < entry_size ; ++i ) {
      ScopedRegister r1(this);
      const ast::Node& e = *node.entry->Index(i);
      if(!VisitExpression(e,&r1)) return false;
      eemit(result,addlist(e.sci(),reg.Get().index(),r1.index()));
    }
    result->SetRegister(reg.Get());
  }
  return true;
}

bool Generator::Visit( const ast::Object& node , ExprResult* result ) {
  const std::size_t entry_size = node.entry->size();
  if(entry_size == 0) {
    eemit(result,loadobj0(node.sci(),Register::kAccReg));
    result->SetAcc();
  } else if(entry_size == 1) {
    ScopedRegister k(this);
    ScopedRegister v(this);
    const ast::Object::Entry& e = node.entry->Index(0);
    if(!VisitExpression(*e.key,&k)) return false;
    if(!VisitExpression(*e.val,&v)) return false;
    eemit(result,loadobj1(node.sci(),Register::kAccReg,k.Get().index(),
                                                       v.Get().index()));
    result->SetAcc();
  } else {
    Optional<Register> reg;
    if(!(reg = ra_.Grab())) {
      ErrorTooComplicatedCode(node.sci(),result);
      return false;
    }

    eemit(result,newobj(node.sci(),reg.Get().index(),
          static_cast<std::uint16_t>(entry_size)));

    for( std::size_t i = 0 ; i < entry_size ; ++i ) {
      const ast::Object::Entry& e = node.entry->Index(i);
      ScopedRegister k(this);
      ScopedRegister v(this);
      if(!VisitExpression(*e.key,&k)) return false;
      if(!VisitExpression(*e.val,&v)) return false;
      eemit(result,addobj(e.key->sci(),reg.Get().index(),k.Get().index(),
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
    eemit(result,negate(node.sci(),reg.index()));
  } else {
    eemit(result,not(node.sci(),reg.index()));
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
  ScopedRegister lhs;
  // Left hand side
  if(!VisitExpression(*node.lhs,&lhs)) return false;
  if(!lhs.IsAcc()) SpillToAcc(lhs);

  // And or Or instruction
  BytecodeBuilder::Label label;

  if(node.op == Token::kAnd) {
    label = func_scope()->bb()->and(node.lhs->sci());
  } else {
    label = func_scope()->bb()->or(node.lhs->sci());
  }
  if(!label) {
    ErrorFunctionTooLong(result);
    return false;
  }

  // Right hand side expression evaluation
  ScopedRegister rhs;
  if(!VisitExpression(*node.rhs,&rhs)) return false;
  if(!rhs.IsAcc()) SpillToAcc(rhs);

  // Patch the branch label
  label.Patch( func_scope()->bb()->CodePosition() );

  result->SetAcc();
  return true;
}

bool Generator::Visit( const ast::Binary& node , ExprResult* result ) {
  if(node.op.IsArithmetic() || node.op.IsComparison()) {
    if((node.lhs->IsLiteral() && CanBeSpecializedLiteral(*node.lhs->AsLiteral())) ||
       (node.rhs->IsLiteral() && CanBeSpecializedLiteral(*node.rhs->AsLiteral())) ) {
#ifdef LAVASCRIPT_CHECK_OBJECTS
      lava_verify(!(node.lhs->IsLiteral() && node.rhs->IsLiteral()));
#endif // LAVASCRIPT_CHECK_OBJECTS
      BinOperandType t = node.lhs->IsLiteral() ? GetBinOperandType(*node.lhs->AsLiteral()) :
                                                 GetBinOperandType(*node.rhs->AsLiteral());
      Bytecode bc;

      // Get the bytecode for this expression
      if(!GetBinBytecode(node.op,t,node.lhs->IsLiteral(),
                                   node.rhs->IsLiteral(),
                                   &bc))
        return false;

      // Evaluate each operand and its literal value
      if(node.lhs->IsLiteral()) {
        ScopedRegister rhs_reg;
        if(!VisitExpression(*node.rhs,&rhs_reg)) return false;
        std::int32_t ref = func_scope()->bb()->Add(
            node.lhs->AsLiteral()->int_value );
        if(ref<0) {
          ErrorTooComplicatedFunc(node.sci(),result);
          return false;
        }
        if(!func_scope()->bb()->EmitC(node.sci(),ref,rhs_reg.Get().index())) {
          ErrorFunctionTooLong(result);
          return false;
        }
      } else {
        ScopedRegister lhs_reg;
        if(!VisitExpression(*node.lhs,&lhs_reg)) return false;
        std::int32_t ref = func_scope()->bb()->Add(
            node.rhs->AsLiteral()->int_value );
        if(ref <0) {
          ErrorTooComplicatedFunc(node.sci(),result);
          return false;
        }
        if(!func_scope()->bb()->EmitB(node.sci(),lhs_reg.Get().index(),ref)) {
          ErrorFunctionTooLong(result);
          return false;
        }
      }
    } else {
      // Well will have to use VV type instruction which is a slow path. This can be anything
      // from using boolean and null for certain arithmetic and comparsion ( which is not allowed )
      // to using variable as operand for above operands.
      ScopedRegister lhs(this);
      ScopedRegister rhs(this);
      if(!VisitExpression(*node.lhs,&lhs)) return false;

      // This is a small place likely to be forgetten by people since
      // the LHS can be held in a ACC register and if it is then we
      // need to spill it since RHS can use ACC register as well
      if(lhs.IsAcc()) {
        if(!SpillFromAcc(&lhs)) return false;
      }
      if(!VisitExpression(*node.rhs,&rhs)) return false;

      if(!func_scope()->bb()->EmitE(
          node.sci(),
          static_cast<Bytecode>( kBinGeneralOpLookupTable[static_cast<int>(node.op.token())] ),
          lhs.index(),
          rhs.index())) {
        ErrorFunctionTooLong();
        return false;
      }
    }
  } else {
#ifdef LAVASCRIPT_CHECK_OBJECTS
    lava_verify(node.op.IsLogic());
#endif // LAVASCRIPT_CHECK_OBJECTS
    return VisitLogic(node,result);
  }
}

bool Generator::Visit( const ast::Ternary& node , ExprResult* result ) {
  // Generate code for condition , don't have to be in Acc register
  ScopedRegister reg;
  if(!VisitExpression(*node._1st,&reg)) return false;

  // Based on the current condition to branch
  BytecodeBuilder::Label cond_label =
    func_scope()->bb->jmpf(node.sci(),reg.Get().index());
  if(!cond_label) {
    ErrorFunctionTooLong(result);
    return false;
  }

  // Now 2nd expression generated here , which is natural fallthrough
  ScopedRegister reg_2nd;
  if(!VisitExpression(*node._2nd,&reg_2nd)) return false;
  if(!reg_2nd.IsAcc()) SpillToAcc(reg_2nd);

  // After the 2nd expression generated it needs to jump/skip the 3rd expression
  BytecodeBuilder::Label label_2nd = func_scope()->bb()->jmp();
  if(!label_2nd) {
    ErrorFunctionTooLong(result);
    return false;
  }

  // Now false / 3nd expression generated here , which is not a natural fallthrough
  // The conditional failed branch should jump here which is the false branch value
  cond_label.Patch(func_scope()->bb()->CodePosition());
  ScopedRegister reg_3rd;
  if(!VisitExpression(*node._3rd,&reg_3rd)) return false;
  if(!reg_3rd.IsAcc()) SpillToAcc(reg_3rd);

  // Now merge 2nd expression jump label
  label_2nd.Patch(func_scope()->bb()->CodePosition());

  result->SetAcc();
  return true;
}

bool Generator::VisitExpression( const ast::Node& node , ExprResult* result ) {
  switch(node.type) {
    case ast::LITERAL: return Visit(*node.AsLiteral(),result);
    case ast::VARIABLE: return Visit(*node.AsVariable(),result);
    case ast::PREFIX:  return Visit(*node.AsPrefix(),result);
    case ast::BINARY: return Visit(*node.AsBinary(),result);
    case ast::TERNARY: return Visit(*node.AsTernary(),result);
    case ast::LIST: return Visit(*node.AsList(),result);
    case ast::OBJECT: return Visit(*node.AsObject(),result);
    case ast::FUNCTION: return VisitAnonymousFunction(node,result);
    default:
      lava_unreachF("Disallowed expression with node type %s",node.node_name());
      return false;
  }
}

bool Generator::VisitExpression( const ast::Node& node , Register* result ) {
  ExprResult r;
  if(!VisitExpression(node,&r)) return false;
  ScopedRegister reg(this,ExprResultToRegister(r));
  if(!reg) {
    ErrorTooComplicatedCode();
    return false;
  }
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
   * The local variable will be defined during we setup the lexcial scope
   * so here we just need to get the local variable and it *MUST* be existed
   */
  Optional<Register> lhs(lexical_scope_->GetLocalVar(*node.var->name));

#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(lhs.Has());
#endif // LAVASCRIPT_CHECK_OBJECTS

  if(node.expr) {
    Register rhs;
    if(!VisitExpression(*node.expr,&rhs)) return false;
    semit(move(node.sci(),lhs.Get().index(),rhs.index()));
  } else {
    // put a null to that value as default value
    semit(loadnull(node.sci(),lhs.Get().index()));
  }
  return true;
}

bool Generator::Visit( const ast::Assign& node ) {
  if(node.lhs_type() == ast::Assign::LHS_VAR) {
    return VisitSimpleAssign(node);
  } else {
    return VisitComplexAssign(node);
  }
}

bool Generator::VisitSimpleAssign( const ast::Assign& node ) {
  Optional<Register> r(lexical_scope_->GetLocalVar(*node.lhs_var->name));
  if(!r) {
    ErrorLocalVariableNotExisted(node.sci(),*node.lhs_var->name);
    return false;
  }
  /**
   * Optimize for common case since our expression generator allocate
   * register on demand , this may requires an extra move to move the
   * intermediate register used to hold rhs to lhs's register
   */
  if(node.IsLiteral()) {
    AllocateLiteral(*node.AsLiteral(),r.Get());
  } else {
    ScopedRegister rhs(this);
    if(!VisitExpression(*node.lhs_var,&rhs)) return false;
    semit(move(node.sci(),r.index(),rhs.Get().index()));
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
        std::int32_t ref = func_scope_->bb()->Add(
            *last_comp.var->name,context_);
        if(ref<0) {
          ErrorTooComplicatedCode(node.sci());
          return false;
        }
        if(!rhs.IsAcc()) {
          semit(move(node.sci(),Register::kAccIndex,rhs.Get().index()));
        }
        semit(propset(node.sci(),lhs.Get().index(),ref));
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
        Register rhs_reg(rhs.Get());
        if(rhs_reg.IsAcc()) {
          Optional<Register> temp(SpillFromAcc());
          if(!temp) return false;
          rhs_reg = temp.Get();
        }

        ScopedRegister expr_reg;
        if(!VisitExpression(*last_comp.expr,&expr_reg)) return false;

        // idxset REG REG REG
        semit(idxset(node.sci(),lhs.Get().index(),expr_reg.Get().index(),
                                                  rhs_reg.index()));

        if(rhs.IsAcc()) ra_.Drop(rhs_reg); // Drop rhs_reg since in this case the rhs_reg
                                           // is not protected by the rhs ScopedRegister

      }
      break;
    default:
      lava_unreach("Cannot be in this case ending with a function call");
      return false;
  }
  return true;
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
    const If::Branch& br = node.br_list->Index(i);
    // If we have a previous branch, then its condition should jump
    // to this place
    if(prev_jmp) {
      prev_jmp.Patch(func_scope_->bb_builder()->CodePosition());
    }

    // Generate condition code if we need to
    if(br.cond) {
      ScopedRegister cond(this);
      if(!VisitExpression(*br.cond,&cond)) return false;
      prev_jmp = func_scope_->bb_builder()->jmpf(br.cond->sci(),cond.Get().index());
      if(!prev_jmp) {
        ErrorTooComplicatedCode(br.cond->sci());
        return false;
      }
    } else {
#ifdef LAVASCRIPT_CHECK_OBJECTS
      lava_verify( i == len - 1);
#endif // LAVASCRIPT_CHECK_OBJECTS
    }

    // Generate code body
    if(!VisitChunk(*br.body)) return false;

    // Generate the jump
    if(br.cond)
      label_vec.push(func_scope_->bb_builder()->jmp(br.cond->sci()));
  }

  for(auto &e : label_vec) e.Patch(func_scope_->bb_builder()->CodePosition());
  return true;
}

bool Generator::VisitForCondition( const ast::For& node ,
                                   const Register& var  ) {
  ExprResult cond;
  if(!VisitExpression(*node._2nd,&cond)) return false;
  switch(cond.kind()) {
    case KINT:
      semit(ltvi(node._2nd->sci(),var.index(),cond.ref()));
      break;
    case KREAL:
      semit(ltvr(node._2nd->sci(),var.index(),cond.ref()));
      break;
    case KSTR:
      semit(ltvs(node._2nd->sci(),var.index(),cond.ref()));
    default:
      {
        ScopedRegister r(this,ExprResultToRegister(cond));
        if(!r) {
          ErrorTooComplicatedCode();
          return false;
        }
        smit(ltvv(node._2nd->sci(),var.index(),r.Get().index()));
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
    forward = func_scope_->bb()->fstart( node.sci() );
  } else {
    if(node._1st) {
      if(!Visit(*node._1st,&induct_reg)) return false;
    }
    semit(fevrstart(node.sci())); // Mark for JIT
  }

  /* ------------------------------------------
   * Loop body                                |
   * -----------------------------------------*/
  {
    EnterLoopScope scope(this,*node.body);

    std::uint16_t header = static_cast<std::uint16_t>(
        func_scope_->bb()->CodePosition());

    if(!VisitChunk(*node.body,false)) return false;

    // Patch all contiune to jump here
    scope.loop_scope()->PatchContinue(static_cast<std::uint16_t>(
          func_scope_->bb()->CodePosition()));

    /**
     * Now generate loop header at the bottom of the loop. Basically
     * we have a simple loop inversion for the for loop
     */
    if(node._2nd) {
      if(!VisitForCondition(node,induct_reg)) return false;

      // Jump back to the loop header
      semit(fend(node.sci(),header));
    } else {
      semit(fevrend(node.sci(),header));
    }

    // Patch all break to jump here , basically jumps out of
    // the scope
    scope.loop_scope()->PatchBreak(static_cast<std::uint16_t>(
          func_scope_->bb()->CodePosition()));
  }
  if(forward) {
    forward.Patch( static_cast<std::uint16_t>(
          func_scope_->bb()->CodePosition()) );
  }

  return true;
}

bool Generator::Visit( const ast::ForEach& node ) {
  Register reg;
  if(!VisitExpression(*node.itr,&reg)) return false;

  // Pin the iterator's register to a name
  lexical_scope_->DefineAnonymousVar(reg);

  // Generate the festart
  BytecodeBuilder::Label forward = func_scope_->bb()->festart(node.sci());

  {
    EnterLoopScope scope(this,*node.body);

    std::uint16_t header = static_cast<std::uint16_t>(
        func_scope_->bb()->CodePosition());

    Optional<Register> v(func_scope_->DefineVar(*node.var->name));
    if(!v) {
      ErrorTooComplicatedCode(node.var->sci());
      return false;
    }

    // Deref the key from iterator register into the target register
    semit(idref(node.var->sci(),v.Get().index(),reg.index()));

    // Visit the chunk
    if(!VisitChunk(*node.body,false)) return false;

    scope.loop_scope()->PatchContinue(static_cast<std::uint16_t>(
          func_scope_->bb()->CodePosition()));

    semit(feend(node.sci(),reg.index(),header));


    scope.loop_scope()->PatchBreak(static_cast<std::uint16_t>(
          func_scope_->bb()->CodePosition()));
  }

  forward.Patch(static_cast<std::uint16_t>(
        func_scope_->bb()->CodePosition()));

  return true;
}

bool Generator::Visit( const ast::Continue& node ) {
  if(!lexical_scope_->AddContinue(node)) {
    ErrorTooComplicatedCode(node.sci());
    return false;
  }
  return true;
}

bool Generator::Visit( const ast::Break& node ) {
  if(!lexical_scope_->AddBreak(node)) {
    ErrorTooComplicatedCode(node.sci());
    return false;
  }
  return true;
}

/*
 * As long as the return value is a function call , then we can
 * tail call optimized it
 */
bool Generator::CanBeTailCallOptimized( const ast::Node& node ) {
  if(node.IsPrefix()) {
    const ast::Prefix& p = *node.AsPrefix();
    if(p.list->Last().IsCall()) {
      return true;
    }
  }
  return false;
}

bool Generator::Return( const ast::Return& node ) {
  if(!node.has_return_value()) {
    semit(ret0(node.sci()));
  } else {
    // Look for return function-call() style code and then perform
    // tail call optimization on this case. Basically, as long as
    // the return is returning a function call , then we can perform
    // tail call optimization since we don't need to return to the
    // previous call frame at all.
    if(CanBeTailCallOptimized(*node.expr)) {
#ifdef LAVASCRIPT_CHECK_OBJECTS
      lava_verify(node.expr->IsPrefix());
#endif // LAVASCRIPT_CHECK_OBJECTS
      ScopedRegister ret;
      if(!VisitPrefix(*node.expr->AsPrefix(),
                      node.expr->AsPrefix()->list->size(),
                      true, // allow the tail call optimization
                      &ret))
        return false;
      if(!ret.IsAcc()) SpillToAcc(ret);
    } else {
      ScopedRegister ret;
      if(!VisitExpression(*node.expr,&ret)) return false;
      if(!ret.IsAcc()) SpillToAcc(ret);
    }
    semit(ret(node.sci()));
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
    EnterLexicalScope scope(this,node);
    return VisitChunkNoLexicalScope(node);
  }
  return VisitChunkNoLexicalScope(node);
}

































} // namespace interpreter
} // namespace lavascript
