#ifndef BYTECODE_GENERATOR_H_
#define BYTECODE_GENERATOR_H_
#include <string>
#include <algorithm>

#include "bytecode-builder.h"
#include "src/parser/ast/ast.h"
#include "src/objects.h"
#include "src/common.h"
#include "src/util.h"
#include "src/error-report.h"
#include "src/context.h"
#include "src/script-builder.h"
#include "src/trace.h"

namespace lavascript {

namespace interpreter {
namespace detail {

using namespace ::lavascript::parser;

class RegisterAllocator;
class Register;
class ScopedRegister;
class Generator;
class FunctionScope;
class LexicalScope;

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
    reg_(reg),
    empty_(false)
  {}

  ScopedRegister( Generator* generator ):
    generator_(generator),
    reg_(),
    empty_(true)
  {}

  ~ScopedRegister();
 public:
  bool IsEmpty() const { return empty_; }
  operator bool() const { return !IsEmpty(); }
  const Register& Get() const { lava_verify(!IsEmpty()); return reg_; }
  Register Release();
  void Reset( const Register& reg );
  void Reset();
  bool Reset( const Optional<Register>& reg );
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
  void Drop( const Register& );
  inline bool IsAvailable( const Register& );
  inline bool IsUsed     ( const Register& );
  bool IsEmpty() const { return free_register_ == NULL; }
  std::size_t size() const { return size_; }
  inline std::uint8_t base() const;
 public:
  // The following API are used for local variable reservation
  bool EnterScope( std::size_t , std::uint8_t* base );
  void LeaveScope();

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
  void Init( const ast::Chunk& );

  // Initialize LexicalScope if this scope is the immdiet lexical scope
  // of a certain function
  void Init( const ast::Function& );

  FunctionScope* func_scope() const { return func_scope_; }
 public:
  /* ------------------------------------
   * local variables                    |
   * -----------------------------------*/

  // Define a local variable with the given Register
  inline bool DefineLocalVar( const zone::String& , const Register& );

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
  // Get a local variable from current lexical scope
  inline Optional<Register> GetLocalVarInPlace( const zone::String& );

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

  friend class FunctionScope;

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
  // initialize all local variables register mapping
  bool Init( const ast::LocVarContext& );

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
  inline bool FindUpValue( const zone::String& , std::uint16_t* );
  inline void AddUpValue ( const zone::String& , std::uint16_t  );

  // get local variable register mapping
  Register GetLocalVarRegister( const zone::String& ) const;

  // get next iterator register mapping
  Register GetScopeBoundIterator();
  void FreeScopeBoundIterator   ();

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

  // all register mapping for local variables inside of this function
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

  // all *iterator* registers
  std::vector<Register> iterators_;

  // cursor points next avaiable iterator register
  std::size_t next_iterator_;

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
  inline bool GetBinaryOperatorBytecode( const SourceCodeInfo& , const Token& tk ,
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
  bool Visit( const ast::List&   , const Register& , const SourceCodeInfo& sci , ExprResult* );
  bool Visit( const ast::Object& , const Register& , const SourceCodeInfo& sci , ExprResult* );

  bool Visit( const ast::List&  node , ExprResult* result ) {
    return Visit(node,Register::kAccReg,node.sci(),result);
  }
  bool Visit( const ast::Object& node , ExprResult* result ) {
    return Visit(node,Register::kAccReg,node.sci(),result);
  }

  bool VisitExpression( const ast::Node& , ExprResult* );
  bool VisitExpression( const ast::Node& , Register* );
  bool VisitExpression( const ast::Node& , ScopedRegister* );

  // The following version of Expression code generation will try its
  // best to put the final result inside of the *hint* register and this
  // is typically a type of optimization since our code gen will allocate
  // register on demand but this is not optimal in terms of the final
  // code generation.
  bool VisitExpressionWithHint( const ast::Node& , const Register& hint ,
                                                   ScopedRegister* );

  bool VisitExpressionWithHint( const ast::Node& , const Register& hint ,
                                                   Register* );

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
  Optional<Register> SpillRegister( const SourceCodeInfo& , const Register& );

  // Allocate literal with in certain registers
  bool AllocateLiteral( const SourceCodeInfo& sci , const ast::Literal& ,
                                                    const Register& );

  // Convert ExprResult to register, it may allocate new register
  // to hold it if it is literal value
  Optional<Register> ExprResultToRegister( const SourceCodeInfo& sci , const ExprResult& );

 private: // Errors ---------------------------------------------
  inline void Error( const SourceCodeInfo& , const char* fmt , ... ) const;

  // Predefined error category for helping manage consistent error reporting
#define BYTECODE_COMPILER_ERROR_LIST(__) \
  __(UPVALUE_OVERFLOW,"too many upvalue for closure")                          \
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
  inline void Error( ErrorCategory , const ast::Node& , const char* fmt , ... ) const;
  inline void Error( ErrorCategory , const ast::Node& ) const;
  inline void Error( ErrorCategory , const SourceCodeInfo& sci ) const;

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

} // namespace detail


/** ---------------------------------------------------------------------------------
 * Generate bytecode from a AST into an Script objects ; if failed then return
 * false and put the error description inside of the error buffer
 *  -------------------------------------------------------------------------------*/
bool GenerateBytecode( Context* , const ::lavascript::parser::ast::Root& , ScriptBuilder* ,
                                                                           std::string* );


} // namespace interpreter
} // namespace lavascript


namespace lavascript {
namespace interpreter{
namespace detail {

/* ===============================================================================
 *
 * Inline Definition of Function
 *
 * =============================================================================*/

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
  empty_= false;
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
    empty_ = false;
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

inline bool LexicalScope::DefineLocalVar( const zone::String& name ,
                                          const Register& reg ) {
  lava_debug( NORMAL , lava_verify(GetLocalVarInPlace(name).Has()); );
  local_vars_.push_back( LocalVar( &name , reg ) );
  return true;
}

inline Optional<Register> LexicalScope::GetLocalVarInPlace( const zone::String& name ) {
  std::vector<LocalVar>::iterator itr =
    std::find( local_vars_.begin() , local_vars_.end() , name );
  return itr  == local_vars_.end() ? Optional<Register>() :
                                     Optional<Register>( itr->reg );
}

inline Optional<Register> LexicalScope::GetLocalVar( const zone::String& name ) {
  return func_scope()->GetLocalVar(name);
}

inline bool LexicalScope::AddBreak( const ast::Break& node ) {
  BytecodeBuilder::Label l( func_scope()->bb()->brk(func_scope()->ra()->base(),node.sci()) );
  if(!l) return false;
  if(is_loop_)
    break_list_.push_back(l);
  else
    GetNearestLoopScope()->break_list_.push_back(l);
  return true;
}

inline bool LexicalScope::AddContinue( const ast::Continue& node ) {
  BytecodeBuilder::Label l( func_scope()->bb()->cont(func_scope()->ra()->base(),node.sci()) );
  if(!l) return false;
  if(is_loop_)
    continue_list_.push_back(l);
  else
    GetNearestLoopScope()->continue_list_.push_back(l);
  return true;
}

inline bool FunctionScope::FindUpValue( const zone::String& name ,
                                        std::uint16_t* index ) {
  std::vector<UpValue>::iterator
    ret = std::find( upvalue_.begin() , upvalue_.end() , name );
  if(ret == upvalue_.end()) return false;
  *index = ret->index;
  return true;
}

inline void FunctionScope::AddUpValue ( const zone::String& name ,
                                 std::uint16_t index ) {
  upvalue_.push_back(UpValue(&name,index));
}

inline FunctionScope::FunctionScope( Generator* gen , const ast::Function& node ):
  Scope(gen,gen->lexical_scope_),
  bb_  (),
  ra_  (),
  upvalue_ (),
  lexical_scope_list_(),
  body_(node.body),
  local_vars_(),
  iterators_ (),
  next_iterator_(0)
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
  body_(&node),
  local_vars_(),
  iterators_ (),
  next_iterator_(0)
{
  gen->func_scope_ = this;
  gen->lexical_scope_ = NULL;
}

inline FunctionScope::~FunctionScope() {
  lava_debug(NORMAL,
      lava_verify( lexical_scope_list_.empty() );
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

inline void Generator::Error( const SourceCodeInfo& sci , const char* fmt , ... ) const {
  va_list vl;
  va_start(vl,fmt);
  ReportErrorV(error_,"[bytecode-compiler]",script_builder_->source().c_str(),
                                            sci.start,
                                            sci.end,
                                            fmt,
                                            vl);
}

inline void Generator::Error( ErrorCategory ec , const ast::Node& node ,
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

inline void Generator::Error( ErrorCategory ec , const ast::Node& node ) const {
  ReportError(error_,"[bytecode-compiler]",script_builder_->source().c_str(),
                                           node.sci().start,
                                           node.sci().end,
                                           "%s",
                                           GetErrorCategoryDescription(ec));
}

inline void Generator::Error( ErrorCategory ec , const SourceCodeInfo& sci ) const {
  ReportError(error_,"[bytecode-compiler]",script_builder_->source().c_str(),
                                           sci.start,
                                           sci.end,
                                           "%s",
                                           GetErrorCategoryDescription(ec));
}

} // namespace detail
} // namespace interpreter
} // namespace lavascript
#endif // BYTECODE_GENERATOR_H_
