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
class Generator;
class RegisterAllocator;

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

class ScopedRegister {
 public:
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
  bool IsAcc() const { return reg_.IsAcc(); }
  std::uint8_t index() const { return reg_.index(); }
  operator int() const { return index(); }
  const Register& Get() const { return reg_; }
  inline Register Release();
  inline void Reset( const Register& reg );
 private:
  Generator* generator_;
  Register reg_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(ScopedRegister);
};

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

class RegisterAllocator {
 public:
  RegisterAllocator();

  inline Optional<Register> Grab();
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
  Optional<Register> DefineVar( const zone::String& );

  // Get a local variable from current lexical scope
  Optional<Register> GetLocalVar( const zone::String& );

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
  BytecodeBuilder* bc_builder() { return &bc_builder_; }
 private:
  BytecodeBuilder bc_builder_;
};

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
  ExprResult( Generator* gen ):
    kind_(KNULL),
    ref_(0),
    reg_(0),
    generator_(gen),
    release_(false)
  {}

  ~ExprResult();
 public:

  ExprResultKind kind() const { return kind_; }
  bool IsRefType() const { return kind_ == KINT || kind_ == KREAL || kind_ == KSTR; }
  bool IsInt() const { return kind_ == KINT; }
  bool IsReal()const { return kind_ == KREAL;}
  bool IsReg() const { return kind_ == KREG; }
  bool IsAcc() const { return kind_ == KREG && reg_.IsAcc(); }
  bool IsTrue() const { return kind_ == KTRUE; }
  bool IsFalse() const { return kind_ == KFALSE; }
  bool IsNull() const { return kind_ == KNULL; }
  std::int32_t ref() const { lava_verify(IsRefType()); return ref_; }
  const Register& reg() const { lava_verify(IsReg()); return reg_; }

  void SetIRef( std::int32_t iref ) {
    FreeRegister();
    ref_ = iref;
    kind_ = KINT;
  }

  void SetRRef( std::int32_t rref ) {
    FreeRegister();
    ref_ = rref;
    kind_ = KREAL;
  }

  void SetSRef( std::int32_t sref ) {
    FreeRegister();
    ref_ = sref;
    kind_ = KSTR;
  }

  void SetTrue() { FreeRegister(); kind_ = KTRUE; }
  void SetFalse(){ FreeRegister(); kind_ = KFALSE;}
  void SetNull() { FreeRegister(); kind_ = KNULL; }
 public:
  /* -------------------------------------------------------
   * Register related APIs. Once the register is set
   * to the ExprResult then it is *owned* by the ExprResult
   * unless user manually Release the register. Otherwise
   * in ExprResult's destructor , the register will be freed
   * automatically
   * -------------------------------------------------------*/
  void SetRegister( const Register& reg ) {
    kind_ = KREG;
    reg_  = reg;
  }

  inline Register NewRegister();

  Register ReleaseRegister() {
    Register ret = reg_;
    kind_ = KNULL;
    return ret;
  }

 private:
  inline void FreeRegister();

 private:
  ExprResultKind kind_;
  std::int32_t ref_;
  Register reg_;
  Generator* generator_;
  bool release_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(ExprResult);
};

#define emit(RESULT,XX)                            \
  do {                                             \
    auto _ret = func_scope()->bc_builder()->XX;    \
    if(!_ret) {                                    \
      ErrorFunctionTooLong((RESULT));              \
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
 *    SpillAcc to move your result from Acc to another register that owns by you.
 * 3. Any function's result *CAN* be held in Acc register. So calling some internal
 *    Visit function can result in the value held in Acc register.
 */

class Generator {
 public:
  /* ------------------------------------------------
   * Generate expression
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

  inline BinOperandType GetBinOperandType( const ast::Literal& ) const;

  const char* GetBinOperandTypeName( BinOperandType t ) {
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
   * Expression Code Generation
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
  bool Visit( const List&   , ExprResult* );
  bool Visit( const Object& , ExprResult* );
  bool VisitExpression( const ast::Node&   , ExprResult* );

  // This Visit will not generate any intermediate expression result stored in
  // ExprResult but will put everything inside of the Register. It is used for
  // cases that we can only work with register.
  bool VisitExpression( const ast::Node& expr , Register* );
  bool VisitExpression( const ast::Node& expr , ScopedRegister* );

  /* -------------------------------------------
   * Statement Code Generation                 |
   * ------------------------------------------*/
  bool Visit( const Function& , const Optional<Register>& holder , ExprResult* );

 private:
  // Spill the Acc register to another register
  Optional<Register> SpillAcc();
  bool SpillAcc(ScopedRegister*);
  void SpillToAcc( const Register& );

 private: // Errors
  void Error( const SourceCodeInfo& , ExprResult* , const char* fmt , ... );

  // The following ErrorXXX function is common or frequently used error report
  // function which captures certain common cases
  void ErrorTooComplicatedFunc( const SourceCodeInfo& , ExprResult* );
  void ErrorFunctionTooLong   ( ExprResult* );

 private:
  FunctionScope* func_scope_;
  Scope* cur_scope_;
  RegisterAllocator ra_;
  Context* context_;
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

bool Generator::Visit( const ast::Literal& lit , ExprResult* result ) {
  std::int32_t ref;
  switch(lit.literal_type) {
    case ast::Literal::LIT_INTEGER:
      if((ref=func_scope()->bc_builder()->Add(lit.int_value))<0) {
        ErrorTooComplicatedFunc(lit.sci(),result);
        return false;
      }
      result->SetIRef(ref);
      return true;
    case ast::Literal::LIT_REAL:
      if((ref=func_scope()->bc_builder()->Add(lit.real_value))<0) {
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
      if((ref = func_scope()->bc_builder()->Add(*lit.str_value,context_))<0) {
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
    // It is a global variable so we need to emit global variable stuff
    std::int32_t ref = func_scope_->bc_builder()->Add(*var.name,context_);
    if(ref<0) {
      ErrorTooComplicatedFunc(var.sci(),result);
      return false;
    }

    // Hold the global value inside of Acc register since we can
    emit(result,gget(var.sci(),Register::kAccIndex,ref));
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

bool Generator::Visit( const ast::Prefix& node , ExprResult* result ) {
  // Allocate register for the *var* field in node
  Register var_reg;
  if(!Visit(*node.var,&var_reg)) return false;

  // Handle the component part
  const std::size_t len = node.list->size();
  for( std::size_t i = 0 ; i < len ; ++i ) {
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
          emit(result,propget(c.var->sci(),var_reg.index(),ref));

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
          emit(result,idxgeti(c.expr->sci(),var_reg.index(),ref));

          // Drop register if it is not Acc
          if(!var_reg.IsAcc()) { ra_.Drop(var_reg); var_reg.SetAcc(); }
        } else {
          // Register used to hold the expression
          ScopedRegister expr_reg(this);

          // We need to spill ACC register if our current value are held inside
          // of ACC register due to *this* expression evaluating may use Acc.
          if(var_reg.IsAcc()) {
            Optional<Register> new_reg(SpillAcc());
            if(!new_reg) return false;
            var_reg = new_reg.Get();
          }

          // Get the register for the expression
          if(!VisitExpression(*c.expr,&expr_reg)) return false;

          // emit the code for getting the index and the value is stored
          // inside of the ACC register
          emit(result,idxget(c.expr->sci(),var_reg.index(),expr_reg.index()));

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
            Optional<Register> reg = SpillAcc();
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
          if(areg_set.size() == 0) {
            emit(result,call0(c.func->sci(),var_reg.index()));
          } else if(areg_set.size() == 1) {
            emit(result,call1(c.func->sci(),var_reg.index(),areg_set[0]));
          } else if(areg_set.size() == 2) {
            emit(result.call2(c.func->sci(),var_reg.index(),areg_set[0],areg_set[1]));
          } else {
            emit(result,call( c.func->sci() ,
                  static_cast<std::uint32_t>(c.func->args->size()),
                  var_reg.index() ));
            emit(result,xarg(areg_set));
          }

          // 4.  the result will be stored inside of Acc
          if(!var_reg.IsAcc()) { ra_.Drop(var_reg); var_reg.SetAcc(); }
        }
        break;
    }
  }

  result->SetRegister(var_reg);
  return true;
}

bool Generator::Visit( const ast::List& node , ExprResult* result ) {
}

bool Generator::Visit( const ast::Unary& node , ExprResult* result ) {
  Register reg;
  if(!VisitExpression(*node.opr,&reg)) return false;
  if(node.op == Token::kSub) {
    emit(result,negate(node.sci(),reg.index()));
  } else {
    emit(result,not(node.sci(),reg.index()));
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
  Register reg;
  // Left hand side
  if(!VisitExpression(*node.lhs,&reg)) return false;
  if(!reg.IsAcc()) { SpillToAcc(reg); reg.SetAcc(); }

  // And or Or instruction
  Bytecode::Label label;

  if(node.op == Token::kAnd) {
    label = func_scope()->bc_builder()->and(node.lhs->sci());
  } else {
    label = func_scope()->bc_builder()->or(node.lhs->sci());
  }
  if(!label) {
    ErrorFunctionTooLong(result);
    return false;
  }

  // Right hand side expression evaluation
  if(!VisitExpression(*node.rhs,&reg)) return false;
  if(!reg.IsAcc()) { SpillToAcc(reg); reg.SetAcc(); }

  // Patch the branch label
  label.Patch( func_scope()->bc_builder()->CodePosition() );

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
        Register rhs_reg;
        if(!VisitExpression(*node.rhs,&rhs_reg)) return false;
        std::int32_t ref = func_scope()->bc_builder()->Add(
            node.lhs->AsLiteral()->int_value );
        if(ref<0) {
          ErrorTooComplicatedFunc(node.sci(),result);
          return false;
        }
        if(!func_scope()->bc_builder()->EmitC(node.sci(),ref,rhs_reg.reg().index())) {
          ErrorFunctionTooLong(result);
          return false;
        }
      } else {
        Register lhs_reg;
        if(!VisitExpression(*node.lhs,&lhs_reg)) return false;
        std::int32_t ref = func_scope()->bc_builder()->Add(
            node.rhs->AsLiteral()->int_value );
        if(ref <0) {
          ErrorTooComplicatedFunc(node.sci(),result);
          return false;
        }
        if(!func_scope()->bc_builder()->EmitB(node.sci(),lhs_reg.index(),ref)) {
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
        if(!SpillAcc(&lhs)) return false;
      }
      if(!VisitExpression(*node.rhs,&rhs)) return false;

      if(!func_scope()->bc_builder()->EmitE(
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
  Register reg;
  if(!VisitExpression(*node._1st,&reg)) return false;

  // Based on the current condition to branch
  Bytecode::Label cond_label = func_scope()->bc_builder->jmpf(node.sci());
  if(!cond_label) {
    ErrorFunctionTooLong(result);
    return false;
  }

  // Now true / 2nd expression generated here , which is natural fallthrough
  Register reg_2nd;
  if(!VisitExpression(*node._2nd,&reg_2nd)) return false;
  if(!reg_2nd.IsAcc()) { SpillToAcc(reg_2nd); reg_2nd.SetAcc(); }

  // After the 2nd expression generated it needs to jump/skip the 3rd expression
  Bytecode::Label label_2nd = func_scope()->bc_builder()->jmp();
  if(!label_2nd) {
    ErrorFunctionTooLong(result);
    return false;
  }

  // Now false / 3nd expression generated here , which is not a natural fallthrough

  // The conditional failed branch should jump here which is the false branch value
  cond_label.Patch(func_scope()->bc_builder()->CodePosition());
  Register reg_3rd;
  if(!VisitExpression(*node._3rd,&reg_3rd)) return false;
  if(!reg_3rd.IsAcc()) { SpillToAcc(reg_3rd); reg_3rd.SetAcc(); }

  // Now merge 2nd expression jump label
  label_2nd.Patch(func_scope()->bc_builder()->CodePosition());

  result->SetAcc();
  return true;
}



} // namespace interpreter
} // namespace lavascript
