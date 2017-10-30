#include "bytecode-generate.h"
#include "bytecode-builder.h"

#include "src/parser/ast/ast.h"

#include <vector>
#include <memory>

namespace lavascript {
namespace interpreter {
namespace detail {

using namespace ::lavascript::parser;
const Register Register::kAccReg;
void* RegisterAllocator::kRegUsed = reinterpret_cast<void*>(0x1);


/* =========================================================
 * Code emit macro:
 * EEMIT is for expression level
 * SEMIT is for statement  level
 * ========================================================= */

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

void RegisterAllocator::Drop( const Register& reg ) {
  if(!reg.IsAcc() && !IsReserved(reg)) {
    Node* n = RegisterToSlot(reg);

    lava_debug(NORMAL,
      lava_verify(n->next == static_cast<Node*>(kRegUsed));
    );

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
          lava_debug(NORMAL,lava_verify(c->reg.index()<reg.index()););
          Node* next = c->next;
          if(next) {
            if(next->reg.index() > reg.index()) {
              c->next = n;
              n->next = next;
              break;
            }
          } else {
            c->next = n;
            n->next = NULL;
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

  lava_debug(CRAZY,
    for( Node* c = free_register_ ; c ; c = c->next ) {
      lava_verify( !c->next || c->reg.index() < c->next->reg.index() );
    }
  );
}

bool RegisterAllocator::EnterScope( std::size_t size , std::uint8_t* b ) {
  lava_debug(NORMAL,
      if(free_register_)
        lava_verify(free_register_->reg.index() == base());
      );

  if((base() + size) > kAllocatableBytecodeRegisterSize) {
    return false; // Too many registers so we cannot handle it
  } else {
    if(size >0) {
      std::size_t start = base();
      Node* cur;
      Node* next = NULL;
      *b = start; // record the original base

      lava_debug(NORMAL,lava_verify(free_register_););

      for( cur = free_register_ ; cur ; cur = next ) {
        lava_debug(NORMAL,lava_verify(cur->reg.index() == start););
        next = cur->next;
        cur->next = static_cast<Node*>(kRegUsed);
        ++start;
        if(start == base()+size) break;
      }

      size_ -= size;
      free_register_ = next;
      scope_base_.push_back(*b + size);
    } else {
      *b = base();
      scope_base_.push_back(base()); // duplicate the scope_base_ since LeaveScope will pop it
    }
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

  if(end > start) {
    if(end == kAllocatableBytecodeRegisterSize) {
      RegisterToSlot(end)->next = NULL;
    }

    std::size_t index = end - 1;
    for( ; index > start ; --index ) {
      Node* to = RegisterToSlot(Register(index+1));
      Node* from = RegisterToSlot(index);
      from->next = to;
    }
    /** avoid underflow of unsigned integer **/
    {
      Node* to = RegisterToSlot(Register(index+1));
      Node* from = RegisterToSlot(index);
      from->next = to;
      free_register_ = from;
    }
    size_ += (end-start);
  }

  lava_debug(CRAZY,
    for( Node* c = free_register_ ; c ; c = c->next ) {
      lava_verify( !c->next || c->reg.index() < c->next->reg.index() );
    }
  );
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

void LexicalScope::Init( const ast::Chunk& node ) {
  const std::size_t len = node.local_vars->size();
  for( std::size_t i = 0; i < len; ++i ) {
    const zone::String* name = node.local_vars->Index(i)->name;
    local_vars_.push_back(
        LocalVar(name,func_scope()->GetLocalVarRegister(*name).index()));
  }

  if(node.has_iterator) {
    iterator_.Set(func_scope()->GetScopeBoundIterator());
  }
}

void LexicalScope::Init( const ast::Function& node ) {
  if(!node.proto->empty()) {
    const std::size_t len = node.proto->size();
    for( std::size_t i = 0 ; i < len ; ++i ) {
      const zone::String* name = node.proto->Index(i)->name;
      local_vars_.push_back(LocalVar(name,
            func_scope()->GetLocalVarRegister(*name).index()));
    }
  }
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
        LexicalScope* back = func_scope()->lexical_scope_list_.back();
        lava_verify(back == parent());
      }
    );

  func_scope()->lexical_scope_list_.push_back(this);
  gen->lexical_scope_ = this;
}

LexicalScope::~LexicalScope() {
  lava_debug(NORMAL,
      lava_verify(func_scope()->lexical_scope_list_.back() == this);
      );

  func_scope()->lexical_scope_list_.pop_back();
  generator()->lexical_scope_ = parent()->IsFunctionScope() ? NULL :
                                                              parent()->AsLexicalScope();
  if(iterator_.Has()) {
    func_scope()->FreeScopeBoundIterator();
  }
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
    FunctionScope* scope = this;
    std::vector<FunctionScope*> scopes;

    while(scope) {
      // find the name inside of upvalue slot
      if(scope->FindUpValue(name,index)) {
        // find the name/symbol as upvalue in the |scope|
        for( std::vector<FunctionScope*>::reverse_iterator itr =
            scopes.rbegin() ; itr != scopes.rend() ; ++itr ) {
          std::uint16_t idx;
          scope = *itr;

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
          scope = *itr;

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

Optional<Register> FunctionScope::GetLocalVar( const zone::String& name ) {
  for( auto &e : lexical_scope_list_ ) {
    Optional<Register> r(e->GetLocalVarInPlace(name));
    if(r) return r;
  }
  return Optional<Register>();
}

bool FunctionScope::Init( const ast::LocVarContext& lctx ) {
  const std::size_t lvar_size = lctx.local_vars->size() + lctx.iterator_count;
  if(lvar_size == 0) return true;  // no need to anything

  std::uint8_t base;
  if(!ra()->EnterScope(lvar_size,&base))
    return false;

  // 1. allocate registers for local variables
  {
    const std::size_t l = lctx.local_vars->size();
    for( std::size_t i = 0 ; i < l ; ++i ) {
      local_vars_.push_back(LocalVar(lctx.local_vars->Index(i)->name,
                                     base+static_cast<std::uint8_t>(i)));
    }
    base += static_cast<std::uint8_t>(l);
  }

  // 2. allocate registers for iterators
  {
    for( std::size_t i = 0 ; i < lctx.iterator_count ; ++i ) {
      iterators_.push_back(Register(base + static_cast<std::uint8_t>(i)));
    }
  }

  return true;
}

Register FunctionScope::GetLocalVarRegister( const zone::String& name ) const {
  std::vector<LocalVar>::const_iterator itr =
    std::find( local_vars_.begin() , local_vars_.end() , name );
  lava_debug(NORMAL,lava_verify(itr != local_vars_.end()););
  return itr->reg;
}

Register FunctionScope::GetScopeBoundIterator() {
  lava_debug(NORMAL,lava_verify(next_iterator_ < iterators_.size()););
  return iterators_[next_iterator_++];
}

void FunctionScope::FreeScopeBoundIterator() {
  --next_iterator_;
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

bool Generator::GetBinaryOperatorBytecode( const SourceCodeInfo& sci ,
                                           const Token& tk ,
                                           BinOperandType type ,
                                           bool lhs ,
                                           bool rhs ,
                                           Bytecode* output ) const {
  lava_debug(NORMAL, lava_verify(!(rhs && lhs)); );

  int index = static_cast<int>(rhs) << 1 | static_cast<int>(lhs);
  int opindex = static_cast<int>(tk.token());
  Bytecode bc = static_cast<Bytecode>(
      kBinSpecialOpLookupTable[opindex][static_cast<int>(type)][index]);
  if(bc == BC_HLT) {
    Error(sci,"binary operator %s cannot be used between type %s",
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

Optional<Register> Generator::SpillRegister( const SourceCodeInfo& sci ,
                                             const Register& reg ) {
  Optional<Register> r(func_scope()->ra()->Grab());
  if(!r) {
    Error(ERR_REGISTER_OVERFLOW,sci);
    return r;
  }
  if(!func_scope()->bb()->move(sci,r.Get().index(),reg.index())) {
    Error(ERR_FUNCTION_TOO_LONG,sci);
    return Optional<Register>();
  }
  return r;
}

bool Generator::SpillToAcc( const SourceCodeInfo& sci , ScopedRegister* reg ) {
  lava_debug(NORMAL,lava_verify(*reg););
  EEMIT(move(sci,Register::kAccIndex,reg->Get().index()));
  reg->Reset();
  return true;
}

bool Generator::AllocateLiteral( const SourceCodeInfo& sci , const ast::Literal& lit ,
                                                             const Register& reg ) {
  switch(lit.literal_type) {
    case ast::Literal::LIT_INTEGER:
      if(lit.int_value == 0) {
        EEMIT(load0(sci,reg.index()));
      } else if(lit.int_value == 1) {
        EEMIT(load1(sci,reg.index()));
      } else if(lit.int_value == -1) {
        EEMIT(loadn1(sci,reg.index()));
      } else {
        std::int32_t iref = func_scope()->bb()->Add(lit.int_value);
        if(iref<0) {
          Error(ERR_TOO_MANY_LITERALS,lit.sci());
          return false;
        }
        EEMIT(loadi(sci,reg.index(),static_cast<std::uint16_t>(iref)));
      }
      break;
    case ast::Literal::LIT_REAL:
      {
        std::int32_t rref = func_scope()->bb()->Add(lit.real_value);
        if(rref<0) {
          Error(ERR_TOO_MANY_LITERALS,lit.sci());
          return false;
        }
        EEMIT(loadr(sci,reg.index(),static_cast<std::uint16_t>(rref)));
      }
      break;
    case ast::Literal::LIT_BOOLEAN:
      if(lit.bool_value) {
        EEMIT(loadtrue(sci,reg.index()));
      } else {
        EEMIT(loadfalse(sci,reg.index()));
      }
      break;
    case ast::Literal::LIT_STRING:
      {
        std::int32_t sref = func_scope()->bb()->Add(*lit.str_value,context_->gc());
        if(sref<0) {
          Error(ERR_TOO_MANY_LITERALS,lit.sci());
          return false;
        }
        EEMIT(loadstr(sci,reg.index(),static_cast<std::uint16_t>(sref)));
      }
      break;
    default:
      EEMIT(loadnull(sci,reg.index()));
      break;
  }
  return true;
}

Optional<Register> Generator::ExprResultToRegister( const SourceCodeInfo& sci ,
                                                    const ExprResult& expr ) {
  if(expr.IsReg())
    return Optional<Register>(expr.reg());
  else {
    switch(expr.kind()) {
      case KINT:
        if(!func_scope()->bb()->loadi(
              sci,Register::kAccIndex,static_cast<std::uint16_t>(expr.ref()))) {
          Error(ERR_FUNCTION_TOO_LONG,sci);
          return Optional<Register>();
        }
        break;
      case KREAL:
        if(!func_scope()->bb()->loadr(
              sci,Register::kAccIndex,static_cast<std::uint16_t>(expr.ref()))) {
          Error(ERR_FUNCTION_TOO_LONG,sci);
          return Optional<Register>();
        }
        break;
      case KSTR:
        if(!func_scope()->bb()->loadstr(
              sci,Register::kAccIndex,static_cast<std::uint16_t>(expr.ref()))) {
          Error(ERR_FUNCTION_TOO_LONG,sci);
          return Optional<Register>();
        }
        break;
      case KTRUE:
        if(!func_scope()->bb()->loadtrue(sci,Register::kAccIndex)) {
          Error(ERR_FUNCTION_TOO_LONG,sci);
          return Optional<Register>();
        }
        break;
      case KFALSE:
        if(!func_scope()->bb()->loadfalse(sci,Register::kAccIndex)) {
          Error(ERR_FUNCTION_TOO_LONG,sci);
          return Optional<Register>();
        }
        break;
      default:
        if(!func_scope()->bb()->loadnull(sci,Register::kAccIndex)) {
          Error(ERR_FUNCTION_TOO_LONG,sci);
          return Optional<Register>();
        }
        break;
    }
    return Optional<Register>(Register::kAccReg);
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
  } else {
    int ret = func_scope()->GetUpValue(*var.name,&upindex);
    if(ret == FunctionScope::UV_FAILED) {
      Error(ERR_UPVALUE_OVERFLOW,var.sci());
      return false;
    } else if(ret == FunctionScope::UV_NOT_EXISTED) {
      // It is a global variable so we need to EEMIT global variable stuff
      std::int32_t ref = func_scope()->bb()->Add(*var.name,context_->gc());
      if(ref<0) {
        Error(ERR_REGISTER_OVERFLOW,var);
        return false;
      }

      // Hold the global value inside of Acc register for now , in the future we will
      // work out a better register allocation strategy
      EEMIT(gget(var.sci(),Register::kAccIndex,ref));
      result->SetAcc();
    } else {
      EEMIT(uvget(var.sci(),Register::kAccIndex,upindex));
      result->SetAcc();
    }
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
          const std::size_t arglen = c.fc->args->size();
          std::vector<std::uint8_t> argset;
          argset.reserve(arglen);

          // if we have to spill the function itself then we spill it from Acc since
          // it is possible that it is held inside of Acc. We may not need to spill
          // it when the function doesn't have any argument.
          if(var_reg.IsAcc() && arglen >0) {
            Optional<Register> reg = SpillFromAcc(c.fc->sci());
            if(!reg) return false;
            // hold the new register
            var_reg = reg.Get();
          }

          // 2. Visit each argument and get all its related registers
          for( std::size_t i = 0; i < arglen; ++i ) {
            Register reg;
            Optional<Register> expected(func_scope()->ra()->Grab());
            if(!expected) {
              Error(ERR_REGISTER_OVERFLOW,c.fc->sci());
              return false;
            }

            if(!VisitExpressionWithHint(*c.fc->args->Index(i),expected.Get(),&reg))
              return false;

            if(reg != expected.Get()) {
              func_scope()->ra()->Drop(expected.Get()); // free the expected register
              Optional<Register> r(SpillRegister(c.fc->sci(),reg));
              if(!r) return false;
              reg = r.Get();
            }
            argset.push_back(reg.index());
          }

          lava_debug(NORMAL,
              if(!argset.empty()) {
                std::uint8_t p = argset.front();
                for( std::size_t i = 1 ; i < argset.size() ; ++i ) {
                  // all argument's register should be consequtive
                  lava_verify(p == argset[i]-1);
                  p = argset[i];
                }
              }
            );

          // 3. Generate call instruction

          // Check whether we can use tcall or not. The tcall instruction
          // *must be* for the last component , since then we know we will
          // forsure generate a ret instruction afterwards
          const bool tc = tcall && (i == (len-1));
          if(tc) {
            EEMIT(tcall(c.fc->sci(),
                        var_reg.index(),
                        func_scope()->ra()->base(),
                        static_cast<std::uint8_t>(c.fc->args->size())));
          } else {
            EEMIT(call(c.fc->sci(),
                       var_reg.index(),
                       func_scope()->ra()->base(),
                       static_cast<std::uint8_t>(c.fc->args->size())));
          }

          // 4. the result will be stored inside of Acc
          if(!var_reg.IsAcc()) {
            func_scope()->ra()->Drop(var_reg);
            var_reg.SetAcc();
          }

          // 5. free all temporary register used by the func-call
          for( auto &e : argset ) func_scope()->ra()->Drop(Register(e));
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
                                               const SourceCodeInfo& sci,
                                               ExprResult* result ) {
  const std::size_t entry_size = node.entry->size();

  if(entry_size == 0) {
    EEMIT(loadlist0(sci,reg.index()));
    result->SetRegister(reg);
  } else if(entry_size == 1) {
    ScopedRegister r1(this);
    if(!VisitExpression(*node.entry->Index(0),&r1)) return false;
    EEMIT(loadlist1(sci,reg.index(),r1.Get().index()));
    result->SetRegister(reg);
  } else if(entry_size == 2) {
    ScopedRegister r1(this) , r2(this);
    if(!VisitExpression(*node.entry->Index(0),&r1)) return false;
    if(r1.Get().IsAcc()) {
      if(!r1.Reset(SpillFromAcc(node.sci()))) return false;
    }
    if(!VisitExpression(*node.entry->Index(1),&r2)) return false;
    EEMIT(loadlist2(sci,reg.index(),r1.Get().index(),
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
    EEMIT(newlist(sci,r.index(),static_cast<std::uint16_t>(entry_size)));

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
                                                 const SourceCodeInfo& sci,
                                                 ExprResult* result ) {
  const std::size_t entry_size = node.entry->size();
  if(entry_size == 0) {
    EEMIT(loadobj0(sci,reg));
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
    EEMIT(loadobj1(sci,reg.index(),k.Get().index(),v.Get().index()));
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

    EEMIT(newobj(sci,reg.index(),static_cast<std::uint16_t>(entry_size)));

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
      if(!GetBinaryOperatorBytecode(node.sci(),node.op,t,node.lhs->IsLiteral(),
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

      if(!VisitExpression(*node.lhs,&lhs)) return false;
      if(lhs.Get().IsAcc()) {
        if(!lhs.Reset(SpillFromAcc(node.lhs->sci()))) return false;
      }

      if(!VisitExpression(*node.rhs,&rhs)) return false;

      if(!func_scope()->bb()->EmitE(
          node.sci(),
          static_cast<Bytecode>( kBinGeneralOpLookupTable[static_cast<int>(node.op.token())] ),
          lhs.Get().index(),
          rhs.Get().index())) {
        Error(ERR_FUNCTION_TOO_LONG,node);
        return false;
      }
    }

    result->SetAcc();
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
  *result = reg.Release();
  return true;
}

bool Generator::VisitExpression( const ast::Node& node , ScopedRegister* result ) {
  Register reg;
  if(!VisitExpression(node,&reg)) return false;
  result->Reset(reg);
  return true;
}

bool Generator::VisitExpressionWithHint( const ast::Node& node , const Register& hint ,
                                                                 Register* output ) {
  if(node.IsLiteral()) {
    if(!AllocateLiteral(node.sci(),*node.AsLiteral(),hint)) return false;
    *output = hint;
  } else if(node.IsList()) {
    ExprResult res;
    if(!Visit(*node.AsList(),hint,node.sci(),&res)) return false;
    lava_debug(NORMAL,lava_verify(res.IsReg() && (res.reg() == hint)););
    *output = hint;
  } else if(node.IsObject()) {
    ExprResult res;
    if(!Visit(*node.AsObject(),hint,node.sci(),&res)) return false;
    lava_debug(NORMAL,lava_verify(res.IsReg() && (res.reg() == hint)););
    *output = hint;
  } else {
    if(!VisitExpression(node,output)) return false;
  }
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
    Register reg;
    if(!VisitExpressionWithHint(*node.expr,lhs.Get(),&reg)) return false;
    if(reg != lhs.Get()) {
      SEMIT(move(node.sci(),lhs.Get().index(),reg.index()));
    }
  } else {
    // put a null to that value as default value
    SEMIT(loadnull(node.sci(),lhs.Get().index()));
  }

  if(holder) *holder = lhs.Get();
  return true;
}

bool Generator::VisitSimpleAssign( const ast::Assign& node ) {
  Optional<Register> r(lexical_scope()->GetLocalVar(*node.lhs_var->name));
  if(r) {
    /**
     * Optimize for common case since our expression generator allocate
     * register on demand , this may requires an extra move to move the
     * intermediate register used to hold rhs to lhs's register
     */
    ast::Node* rhs_node = node.rhs;
    Register reg;
    if(!VisitExpressionWithHint(*rhs_node,r.Get(),&reg)) return false;
    if(reg != r.Get()) {
      SEMIT(move(node.sci(),r.Get().index(),reg.index()));
    }
  } else {
    /**
     * Check it against upvalue and global variables. We support assignment
     * of upvalue since they are part of the closure
     */
    std::uint16_t upindex;
    int ret = func_scope()->GetUpValue(*node.lhs_var->name,&upindex);
    if(ret == FunctionScope::UV_FAILED) {
      Error(ERR_UPVALUE_OVERFLOW,node.sci());
      return false;
    } else if(ret == FunctionScope::UV_SUCCESS) {
      // set it as upvalue variable
      ScopedRegister reg(this);
      if(!VisitExpression(*node.rhs,&reg)) return false;
      SEMIT(uvset(node.sci(),upindex,reg.Get().index()));
    } else {
      // set it as global variable
      ScopedRegister reg(this);
      if(!VisitExpression(*node.rhs,&reg)) return false;
      std::int32_t ref = func_scope()->bb()->Add(*node.lhs_var->name,context_->gc());
      if(ref<0) {
        Error(ERR_REGISTER_OVERFLOW,node.sci());
        return false;
      }
      SEMIT(gset(node.sci(),static_cast<std::uint16_t>(ref),reg.Get().index()));
    }
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
      prev_jmp = BytecodeBuilder::Label(); // reset the jmp label
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

  // Patch prev_jmp if we need to
  if(prev_jmp) {
    prev_jmp.Patch(func_scope()->bb()->CodePosition());
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

  lava_debug(NORMAL,
      if(!node._1st) {
        // as long as there're no 1st init statement, a for cannot
        // have condition statment and step statement. and it must
        // be a forever loop
        lava_verify(!node._2nd && !node._3rd);
      }
    );

  if(node._2nd) {
    lava_verify(node._1st);
    if(!Visit(*node._1st,&induct_reg)) return false;
    if(!VisitForCondition(node,induct_reg)) return false;
    forward = func_scope()->bb()->fstart( node.sci() , induct_reg.index() );
  } else {
    if(node._1st) {
      if(!Visit(*node._1st,&induct_reg)) return false;
    } else {
      lava_debug(NORMAL,lava_verify(!node._3rd););
    }
    SEMIT(fevrstart(node.sci())); // Mark for JIT
  }

  /* ------------------------------------------
   * Loop body                                |
   * -----------------------------------------*/
  {
    LexicalScope scope(this,true);
    scope.Init(*node.body);

    std::uint16_t header = static_cast<std::uint16_t>(
        func_scope()->bb()->CodePosition());

    if(!VisitChunk(*node.body,false)) return false;

    // Patch all contiune to jump here
    scope.PatchContinue(
        static_cast<std::uint16_t>(func_scope()->bb()->CodePosition()));

    // Generate loop step
    if(node._3rd) {
      ScopedRegister r(this);
      if(!VisitExpression(*node._3rd,&r)) return false;

      // step the loop induction variable's register. NOTES, we use forinc
      // instead of addvv since addvv requires 2 bytecodes to do a step
      // operation. forinc will move register back to where it is
      SEMIT(forinc(node._3rd->sci(),induct_reg.index(),r.Get().index()));
    }

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
        static_cast<std::uint16_t>(func_scope()->bb()->CodePosition()));
  }

  return true;
}

bool Generator::Visit( const ast::ForEach& node ) {
  // Get the iterator register
  Register itr_reg(lexical_scope()->GetIterator());

  // Evaluate the interator initial value
  ScopedRegister init_reg(this);
  if(!VisitExpression(*node.iter,&init_reg)) return false;

  SEMIT(inew(node.iter->sci(),itr_reg.index(),init_reg.Get().index()));

  // Generate the festart
  BytecodeBuilder::Label forward =
    func_scope()->bb()->festart(node.sci(),itr_reg.index());

  {
    LexicalScope scope(this,true);
    scope.Init(*node.body);

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
    SEMIT(retnull(node.sci()));
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

      // MUST be in ACC since it is a call
      lava_debug(NORMAL,lava_verify(ret.Get().IsAcc()););
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
    scope.Init(node);
    return VisitChunkNoLexicalScope(node);
  }
  return VisitChunkNoLexicalScope(node);
}

Handle<Prototype> Generator::VisitFunction( const ast::Function& node ) {
  FunctionScope scope(this,node);
  if(!scope.Init(*node.lv_context)) {
    Error(ERR_REGISTER_OVERFLOW,node.sci());
    return Handle<Prototype>();
  }

  {
    LexicalScope body_scope(this,false);
    body_scope.Init(node);              // For argument
    body_scope.Init(*node.body);        // For local varaible

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
  return true;
}

bool Generator::Generate() {
  FunctionScope scope(this,*root_->body);
  if(!scope.Init(*root_->lv_context)) {
    Error(ERR_REGISTER_OVERFLOW,root_->sci());
    return false;
  }

  if(!VisitChunk(*root_->body,true)) return false;

  EEMIT(retnull(SourceCodeInfo()));

  Handle<Prototype> main(
      BytecodeBuilder::NewMain(context_->gc(),*scope.bb()));
  if(!main) return false;

  script_builder_->set_main(main);
  return true;
}
} // namespace detail

bool GenerateBytecode( Context* context , const ::lavascript::parser::ast::Root& root ,
                                          ScriptBuilder* sb ,
                                          std::string* error ) {
  detail::Generator gen(context,root,sb,error);
  return gen.Generate();
}

} // namespace interpreter
} // namespace lavascript
