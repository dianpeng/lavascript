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

#define EEMIT(XX,...)                                                \
  do {                                                               \
    auto _ret = func_scope()->bb()->XX(func_scope()->ra()->base(),   \
                                       __VA_ARGS__);                 \
    if(!_ret) {                                                      \
      Error(ERR_FUNCTION_TOO_LONG,func_scope()->body());             \
      return false;                                                  \
    }                                                                \
  } while(false)

#define SEMIT EEMIT

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
        LocalVar(name,Register(func_scope()->GetLocalVarRegister(*name).index())));
  }

  for( std::size_t i = 0 ; i < node.iterator_count ; ++i )
    loop_iter_.push_back(func_scope()->GetScopeBoundIterator());
}

void LexicalScope::Init( const ast::Function& node ) {
  if(!node.proto->empty()) {
    const std::size_t len = node.proto->size();
    for( std::size_t i = 0 ; i < len ; ++i ) {
      const zone::String* name = node.proto->Index(i)->name;
      local_vars_.push_back(LocalVar(name,func_scope()->GetLocalVarRegister(*name)));
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
  loop_iter_ () ,
  loop_iter_avail_(0) {

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
  func_scope()->FreeScopeBoundIterator(loop_iter_.size());
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
                                     Register(base+static_cast<std::uint8_t>(i))));
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

void FunctionScope::FreeScopeBoundIterator( std::size_t cnt ) {
  next_iterator_ -= cnt;
}

std::uint8_t kBinSpecialOpLookupTable [][3] = {
  /* arithmetic operator */
  {BC_HLT,BC_ADDRV,BC_ADDVR},
  {BC_HLT,BC_SUBRV,BC_SUBVR},
  {BC_HLT,BC_MULRV,BC_MULVR},
  {BC_HLT,BC_DIVRV,BC_DIVVR},
  {BC_HLT,BC_MODRV,BC_MODVR},
  {BC_HLT,BC_POWRV,BC_POWVR},
  /* comparison operator */
  {BC_HLT,BC_LTRV,BC_LTVR},
  {BC_HLT,BC_LERV,BC_LEVR},
  {BC_HLT,BC_GTRV,BC_GTVR},
  {BC_HLT,BC_GERV,BC_GEVR},
  {BC_HLT,BC_EQRV,BC_EQVR},
  {BC_HLT,BC_NERV,BC_NEVR}
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
                                           bool lhs ,
                                           bool rhs ,
                                           Bytecode* output ) const {
  lava_debug(NORMAL, lava_verify(!(rhs && lhs)); );

  int index = static_cast<int>(rhs) << 1 | static_cast<int>(lhs);
  int opindex = static_cast<int>(tk.token());
  Bytecode bc = static_cast<Bytecode>(kBinSpecialOpLookupTable[opindex][index]);
  lava_debug(NORMAL, lava_verify(bc != BC_HLT););
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
  if(!func_scope()->bb()->move(func_scope()->ra()->base(),
                               sci,reg.Get().index(),Register::kAccIndex)) {
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
  if(!func_scope()->bb()->move(func_scope()->ra()->base(),
                               sci,r.Get().index(),reg.index())) {
    Error(ERR_FUNCTION_TOO_LONG,sci);
    return Optional<Register>();
  }
  return r;
}

bool Generator::SpillToAcc( const SourceCodeInfo& sci , ScopedRegister* reg ) {
  lava_debug(NORMAL,lava_verify(*reg););
  EEMIT(move,sci,Register::kAccIndex,reg->Get().index());
  reg->Reset();
  return true;
}

bool Generator::AllocateLiteral( const SourceCodeInfo& sci , const ast::Literal& lit ,
                                                             const Register& reg ) {
  switch(lit.literal_type) {
    case ast::Literal::LIT_REAL:
      {
        // Try to narrow the *REAL* to take advantage of LOAD0/LOAD1/LOADN1
        std::int32_t ival;
        if(NarrowReal(lit.real_value,&ival)) {
          switch(ival) {
            case 0: EEMIT(load0,sci, reg.index()); break;
            case 1: EEMIT(load1,sci, reg.index()); break;
            case -1:EEMIT(loadn1,sci,reg.index());break;
            default: goto fallback;
          }
        } else {
fallback:
          std::int32_t rref = func_scope()->bb()->Add(lit.real_value);
          if(rref<0) {
            Error(ERR_TOO_MANY_LITERALS,lit.sci());
            return false;
          }
          EEMIT(loadr,sci,reg.index(),static_cast<std::uint16_t>(rref));
        }
      }
      break;
    case ast::Literal::LIT_BOOLEAN:
      if(lit.bool_value) {
        EEMIT(loadtrue,sci,reg.index());
      } else {
        EEMIT(loadfalse,sci,reg.index());
      }
      break;
    case ast::Literal::LIT_STRING:
      {
        std::int32_t sref = func_scope()->bb()->Add(*lit.str_value,context_->gc());
        if(sref<0) {
          Error(ERR_TOO_MANY_LITERALS,lit.sci());
          return false;
        }
        EEMIT(loadstr,sci,reg.index(),static_cast<std::uint16_t>(sref));
      }
      break;
    default:
      EEMIT(loadnull,sci,reg.index());
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
      case KREAL:
        if(!func_scope()->bb()->loadr(
              func_scope()->ra()->base(),
              sci,Register::kAccIndex,static_cast<std::uint16_t>(expr.ref()))) {
          Error(ERR_FUNCTION_TOO_LONG,sci);
          return Optional<Register>();
        }
        break;
      case KSTR:
        if(!func_scope()->bb()->loadstr(
              func_scope()->ra()->base(),
              sci,Register::kAccIndex,static_cast<std::uint16_t>(expr.ref()))) {
          Error(ERR_FUNCTION_TOO_LONG,sci);
          return Optional<Register>();
        }
        break;
      case KTRUE:
        if(!func_scope()->bb()->loadtrue(func_scope()->ra()->base(),sci,Register::kAccIndex)) {
          Error(ERR_FUNCTION_TOO_LONG,sci);
          return Optional<Register>();
        }
        break;
      case KFALSE:
        if(!func_scope()->bb()->loadfalse(func_scope()->ra()->base(),sci,Register::kAccIndex)) {
          Error(ERR_FUNCTION_TOO_LONG,sci);
          return Optional<Register>();
        }
        break;
      default:
        if(!func_scope()->bb()->loadnull(func_scope()->ra()->base(),sci,Register::kAccIndex)) {
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
    lava_debug(NORMAL,lava_verify(result->GetHint()););

    int ret = func_scope()->GetUpValue(*var.name,&upindex);
    if(ret == FunctionScope::UV_FAILED) {
      Error(ERR_UPVALUE_OVERFLOW,var.sci());
      return false;
    } else if(ret == FunctionScope::UV_NOT_EXISTED) {

      // it is a global variable so we need to EEMIT global variable stuff
      std::int32_t ref = func_scope()->bb()->Add(*var.name,context_->gc());
      if(ref<0) {
        Error(ERR_REGISTER_OVERFLOW,var);
        return false;
      }

      // take care of the hint register
      EEMIT(gget,var.sci(),result->GetHint().Get().index(),ref);
      result->SetRegister(result->GetHint().Get());
    } else {

      // take care of the hint register
      EEMIT(uvget,var.sci(),result->GetHint().Get().index(),upindex);
      result->SetRegister(result->GetHint().Get());
    }
  }
  return true;
}

bool Generator::VisitPrefixComponent( const ast::Prefix::Component& c,
                                      bool tcall,
                                      const Register& input,
                                      const Register& output ) {
  switch(c.t) {
    case ast::Prefix::Component::DOT:
      {
        // Get the string reference
        std::int32_t ref = func_scope()->bb()->Add( *c.var->name , context_ ->gc() );
        if(ref<0) {
          Error(ERR_REGISTER_OVERFLOW,*c.var);
          return false;
        }

        // Use PROPGET instruction
        EEMIT(propget,c.var->sci(),output.index(),input.index(),ref);
      }
      break;
    case ast::Prefix::Component::INDEX:

      // Optimize to use idxgeti instruction which takes an embed integer part
      // of the instruction to avoid constant table lookup and loading. Also
      // it bypass the type check since we only have real type as number type
      // internally
      if(c.expr->IsLiteral() && c.expr->AsLiteral()->IsReal()) {
        // Try to narrow the implementation to find out whether we
        // can embed the index into bc idxgeti which saves us time
        std::uint8_t iref;
        if(NarrowReal(c.expr->AsLiteral()->real_value,&iref)) {
          EEMIT(idxgeti,c.expr->sci(),output.index(),input.index(),iref);
          break;
        }
      }

      // fallthrough here to handle common cases
      {
        ScopedRegister temp_input(this);  // Used when output is *Acc* this is highly
                                          // unlikely to happen though
        if(input.IsAcc()) {
          if(!temp_input.Reset(SpillFromAcc(c.expr->sci()))) return false;
        }

        // Register used to hold the expression
        ScopedRegister expr_reg(this);

        // Get the register for the expression
        if(!VisitExpression(*c.expr,&expr_reg))
          return false;

        if(input.IsAcc()) {
          // Emit the idxget instruction for indexing
          EEMIT(idxget,c.expr->sci(),output.index(),temp_input.Get().index(),
                                                    expr_reg.Get().index());
        } else {
          // Emit the idxget instruction for indexing
          EEMIT(idxget,c.expr->sci(),output.index(),input.index(),
                                                    expr_reg.Get().index());
        }
      }
      break;
    default:
      {
        // Reserve slot for IFrame object on the evaluation stack
        IFrameReserver fr(func_scope()->ra());
        ScopedRegister temp_input(this);

        const std::uint8_t base = func_scope()->ra()->base();
        const std::size_t arglen = c.fc->args->size();
        std::vector<std::uint8_t> argset;
        argset.reserve(arglen);

        if(arglen && input.IsAcc()) {
          if(!temp_input.Reset(SpillFromAcc(c.fc->sci()))) return false;
        }

        // 1. Visit each argument and get all its related registers
        for( std::size_t i = 0; i < arglen; ++i ) {
          Optional<Register> expected(func_scope()->ra()->Grab());
          if(!expected) {
            Error(ERR_REGISTER_OVERFLOW,c.fc->sci());
            return false;
          }

          // Force the argument goes into the correct registers that we want
          if(!VisitExpressionWithOutputRegister(*c.fc->args->Index(i),expected.Get()))
            return false;

          argset.push_back(expected.Get().index());
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

        // 2. Generate call instruction

        // Check whether we can use tcall or not. The tcall instruction
        // *must be* for the last component , since then we know we will
        // forsure generate a ret instruction afterwards
        {
          Register func( input.IsAcc() ? temp_input.Get() : input );
          if(tcall) {
            EEMIT(tcall,c.fc->sci(),
                        func.index(),
                        base,
                        static_cast<std::uint8_t>(c.fc->args->size()));
          } else {
            EEMIT(call,c.fc->sci(),
                       func.index(),
                       base,
                       static_cast<std::uint8_t>(c.fc->args->size()));
          }
        }

        // 3. Handle return value
        //
        // Since function's return value are in Acc registers, we need an
        // extra move to move it back to var_reg. This could potentially be
        // a performance problem due to the fact that we have too much chained
        // function call and all its intermediate return value are shuffling
        // around registers. Currently we have no way to specify return value
        // in function call
        if(!output.IsAcc())
          EEMIT(move,c.fc->sci(),output.index(),Register::kAccIndex);

        // 4. free all temporary register used by the func-call
        for( auto &e : argset ) func_scope()->ra()->Drop(Register(e));
      }
      break;
  }
  return true;
}

template< bool TCALL >
bool Generator::VisitPrefix( const ast::Prefix& node , std::size_t end ,
                                                       const Register& hint ) {
  lava_debug(NORMAL,lava_verify(end););

  ScopedRegister input  (this); // tmeporary register to be used to *hold* intermediate
  ScopedRegister temp   (this);
  Register output;

  if(!VisitExpression(*node.var,&input))
    return false;

  // Check if the input register is a temporary register , if so we don't need
  // allocate a new temporary output register just use the one allocated inside
  // of the call VisitExpression
  if(func_scope()->ra()->IsReserved(input.Get())) {
    // Okay this is a variable register, not a temporary one , so we need to
    // allocate a new temporary register for holding the output
    if(!temp.Reset(func_scope()->ra()->Grab())) return false;
    output = temp.Get();
  } else {
    output = input.Get();
  }

  // Handle first component specifically , pay attention to the input/output
  // register.
  {
    const ast::Prefix::Component& c = node.list->First();
    if(!VisitPrefixComponent(c,TCALL && (node.list->size()==1),input.Get(),
                                                               output))
      return false;
  }

  // Handle rest of the intermediate prefix expression. They all gonna use
  // output/temporary register to hold intermediate result.
  const std::size_t len = node.list->size();
  lava_verify(end <= len);
  for( std::size_t i = 1 ; i < end -1 ; ++i ) {
    const ast::Prefix::Component& c = node.list->Index(i);
    if(!VisitPrefixComponent(c,false,output,output))
      return false;
  }

  // Handle *last* expression since we need to set the output to the hint register
  // for the last one
  if(end > 1)
  {
    const ast::Prefix::Component& c = node.list->Last();
    if(!VisitPrefixComponent(c,TCALL,output,hint))
      return false;
  }

  return true;
}

bool Generator::Visit( const ast::Prefix& node , ExprResult* result ) {
  lava_debug(NORMAL,lava_verify(result->GetHint()););
  if(!VisitPrefix<false>(node,node.list->size(),result->GetHint().Get()))
    return false;
  result->SetRegister(result->GetHint().Get());
  return true;
}

bool Generator::Visit( const ast::List& node , const SourceCodeInfo& sci,
                                               ExprResult* result ) {
  lava_debug(NORMAL,lava_verify(result->GetHint()););

  const std::size_t entry_size = node.entry->size();
  Register output(result->GetHint().Get());

  if(entry_size == 0) {
    EEMIT(loadlist0,sci,output.index());
    result->SetRegister(output);

  } else if(entry_size == 1) {
    ScopedRegister r1(this);
    if(!VisitExpression(*node.entry->Index(0),&r1)) return false;
    EEMIT(loadlist1,sci,output.index(),r1.Get().index());
    result->SetRegister(output);

  } else if(entry_size == 2) {
    ScopedRegister r1(this) , r2(this);
    if(!VisitExpression(*node.entry->Index(0),&r1)) return false;
    if(r1.Get().IsAcc()) {
      if(!r1.Reset(SpillFromAcc(node.sci()))) return false;
    }
    if(!VisitExpression(*node.entry->Index(1),&r2)) return false;
    EEMIT(loadlist2,sci,output.index(),r1.Get().index(),r2.Get().index());
    result->SetRegister(output);

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
    EEMIT(newlist,sci,output.index(),static_cast<std::uint16_t>(entry_size));

    std::size_t idle_reg = func_scope()->ra()->size();
    if(idle_reg < entry_size) {
      /**
       * If we don't have enough idle register, we can always fallback to
       * this *slow* path which uses per entry per addlist instruction. This
       * is not optimal in terms of performance but it is a working solution
       * if we don't have enough free/idel registers
       */
      for( std::size_t i = 0 ; i < entry_size ; ++i ) {
        ScopedRegister r1(this);
        const ast::Node& e = *node.entry->Index(i);
        if(!VisitExpression(e,&r1)) return false;
        EEMIT(addlist,e.sci(),output.index(),r1.Get().index(),1);
      }
    } else {
      // When we reach here it means we have *enough* idle register to
      // optimize the number of *addlist* instruction. We should keep it
      // minimum here
#if LAVASCRIPT_DEBUG_LEVEL >= 1
      std::vector<Register> reg_set;
#endif // LAVASCRIPT_DEBUG_LEVEL
      std::uint8_t base = 255;

      for( std::size_t i = 0 ; i < entry_size ; ++i ) {
        Optional<Register> r(func_scope()->ra()->Grab());
        lava_debug(NORMAL,lava_verify(r););

        if(base == 255) base = r.Get().index();

        lava_debug(NORMAL,reg_set.push_back(r.Get()););

        const ast::Node& e = *node.entry->Index(i);
        if(!VisitExpressionWithOutputRegister(e,r.Get()))
          return false;
      }

      /** check all the intermediate register is sequencial **/
      lava_debug(NORMAL,
          std::uint8_t base = reg_set[0].index();
          for( std::size_t i = 1 ; i < reg_set.size() ; ++i )
            lava_verify(base+i == reg_set[i].index());
        );

      // Now we emit a *single addlist instruction here
      EEMIT(addlist,node.sci(),output.index(),
                               base,
                               static_cast<std::uint8_t>(entry_size));

      // Now free all the register here
      for( int i = static_cast<int>(entry_size) - 1 ; i >= 0 ; --i )
        func_scope()->ra()->Drop(Register(base+i));
    }

    result->SetRegister(output);
  }
  return true;
}

bool Generator::Visit( const ast::Object& node , const SourceCodeInfo& sci,
                                                 ExprResult* result ) {
  lava_debug(NORMAL,lava_verify(result->GetHint()););
  const std::size_t entry_size = node.entry->size();
  Register output(result->GetHint().Get());

  if(entry_size == 0) {
    EEMIT(loadobj0,sci,output.index());
    result->SetRegister(output);

  } else if(entry_size == 1) {
    ScopedRegister k(this);
    ScopedRegister v(this);
    const ast::Object::Entry& e = node.entry->Index(0);
    if(!VisitExpression(*e.key,&k)) return false;
    if(k.Get().IsAcc()) {
      if(!k.Reset(SpillFromAcc(node.sci()))) return false;
    }
    if(!VisitExpression(*e.val,&v)) return false;
    EEMIT(loadobj1,sci,output.index(),k.Get().index(),v.Get().index());
    result->SetRegister(output);

  } else {
    EEMIT(newobj,sci,output.index(),static_cast<std::uint16_t>(entry_size));

    for( std::size_t i = 0 ; i < entry_size ; ++i ) {
      const ast::Object::Entry& e = node.entry->Index(i);
      ScopedRegister k(this);
      ScopedRegister v(this);
      if(!VisitExpression(*e.key,&k)) return false;
      if(k.Get().IsAcc()) {
        if(!k.Reset(SpillFromAcc(e.key->sci()))) return false;
      }
      if(!VisitExpression(*e.val,&v)) return false;
      EEMIT(addobj,e.key->sci(),output.index(),k.Get().index(),v.Get().index());
    }
    result->SetRegister(output);

  }
  return true;
}

bool Generator::Visit( const ast::Unary& node , ExprResult* result ) {
  lava_debug(NORMAL,lava_verify(result->GetHint()););
  ScopedRegister temp(this);

  if(!VisitExpression(*node.opr,&temp)) return false;

  if(node.op == Token::kSub) {
    EEMIT(negate,node.sci(),result->GetHint().Get().index(),temp.Get().index());
  } else {
    EEMIT(not_,node.sci()  ,result->GetHint().Get().index(),temp.Get().index());
  }

  result->SetRegister(result->GetHint().Get());
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
  lava_debug(NORMAL,lava_verify(result->GetHint()););

  Register dst(result->GetHint().Get());

  // Left hand side
  if(!VisitExpressionWithOutputRegister(*node.lhs,dst))
    return false;

  // And or Or instruction
  BytecodeBuilder::Label label;

  if(node.op == Token::kAnd) {
    label = func_scope()->bb()->and_(func_scope()->ra()->base(), node.lhs->sci(),dst.index());
  } else {
    label = func_scope()->bb()->or_ (func_scope()->ra()->base(), node.lhs->sci(),dst.index());
  }
  if(!label) {
    Error(ERR_FUNCTION_TOO_LONG,func_scope()->body());
    return false;
  }

  // Right hand side expression evaluation
  if(!VisitExpressionWithOutputRegister(*node.rhs,dst)) return false;

  // Patch the branch label
  label.Patch( func_scope()->bb()->CodePosition() );

  // Output register is to the hint
  result->SetRegister(dst);
  return true;
}

bool Generator::Visit( const ast::Binary& node , ExprResult* result ) {
  lava_debug(NORMAL,lava_verify(result->GetHint()););
  Register output(result->GetHint().Get());

  if((node.op.IsArithmetic() || node.op.IsComparison())) {

    if((node.lhs->IsLiteral() && CanBeSpecializedLiteral(*node.lhs->AsLiteral())) ||
       (node.rhs->IsLiteral() && CanBeSpecializedLiteral(*node.rhs->AsLiteral())) ) {

      lava_debug(NORMAL,lava_verify(!(node.lhs->IsLiteral() && node.rhs->IsLiteral())); );

      Bytecode bc;

      // Get the bytecode for this expression
      if(!GetBinaryOperatorBytecode(node.sci(),node.op,node.lhs->IsLiteral(),
                                                       node.rhs->IsLiteral(),&bc))
        return false;

      // Evaluate each operand and its literal value
      if(node.lhs->IsLiteral()) {
        ScopedRegister rhs_reg(this);
        if(!VisitExpression(*node.rhs,&rhs_reg)) return false;

        // Get the reference for the literal
        ExprResult lhs_expr;
        if(!SpecializedLiteralToExprResult(*node.lhs->AsLiteral(),&lhs_expr))
          return false;

        if(!func_scope()->bb()->EmitD(
              func_scope()->ra()->base(),
              node.sci(),bc,output.index(),lhs_expr.ref(),rhs_reg.Get().index())) {
          Error(ERR_FUNCTION_TOO_LONG,func_scope()->body());
          return false;
        }

      } else {
        ScopedRegister lhs_reg(this);
        if(!VisitExpression(*node.lhs,&lhs_reg)) return false;

        ExprResult rhs_expr;
        if(!SpecializedLiteralToExprResult(*node.rhs->AsLiteral(),&rhs_expr))
          return false;

        if(!func_scope()->bb()->EmitD(
              func_scope()->ra()->base(),
              node.sci(),bc,output.index(),lhs_reg.Get().index(),rhs_expr.ref())) {
          Error(ERR_FUNCTION_TOO_LONG,node);
          return false;
        }
      }
    } else if ( (node.op.IsEQ() || node.op.IsNE()) &&
                (CanBeSpecializedString(node.lhs) || CanBeSpecializedString(node.rhs)) ) {

      // String specialization only applied with ==/!= operator
      if(CanBeSpecializedString(node.lhs)) {
        // left hand side
        ExprResult lhs_result;
        if(!SpecializedLiteralToExprResult(*node.lhs->AsLiteral(),&lhs_result))
          return false;

        // right hand side
        ScopedRegister rhs_reg(this);
        if(!VisitExpression(*node.rhs,&rhs_reg)) return false;

        if(node.op.IsEQ()) {
          if(!func_scope()->bb()->eqsv(func_scope()->ra()->base(),node.sci(),
                                                                  output.index(),
                                                                  lhs_result.ref(),
                                                                  rhs_reg.Get().index())) {
            Error(ERR_FUNCTION_TOO_LONG,func_scope()->body());
            return false;
          }
        } else {
          if(!func_scope()->bb()->nesv(func_scope()->ra()->base(),node.sci(),
                                                                  output.index(),
                                                                  lhs_result.ref(),
                                                                  rhs_reg.Get().index())) {
            Error(ERR_FUNCTION_TOO_LONG,func_scope()->body());
            return false;
          }
        }
      } else {
        ExprResult rhs_result;
        if(!SpecializedLiteralToExprResult(*node.rhs->AsLiteral(),&rhs_result))
          return false;

        // left hand side
        ScopedRegister lhs_reg(this);
        if(!VisitExpression(*node.lhs,&lhs_reg)) return false;

        if(node.op.IsEQ()) {
          if(!func_scope()->bb()->eqvs(func_scope()->ra()->base(),node.sci(),
                                                                  output.index(),
                                                                  lhs_reg.Get().index(),
                                                                  rhs_result.ref())) {
            Error(ERR_FUNCTION_TOO_LONG,func_scope()->body());
            return false;
          }
        } else {
          if(!func_scope()->bb()->nevs(func_scope()->ra()->base(),node.sci(),
                                                                  output.index(),
                                                                  lhs_reg.Get().index(),
                                                                  rhs_result.ref())) {
            Error(ERR_FUNCTION_TOO_LONG,func_scope()->body());
            return false;
          }
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

      if(!func_scope()->bb()->EmitD(
          func_scope()->ra()->base(),
          node.sci(),
          static_cast<Bytecode>( kBinGeneralOpLookupTable[static_cast<int>(node.op.token())] ),
          output.index(),
          lhs.Get().index(),
          rhs.Get().index())) {
        Error(ERR_FUNCTION_TOO_LONG,node);
        return false;
      }
    }

    result->SetRegister(output);
    return true;
  } else {
    lava_debug(NORMAL, lava_verify(node.op.IsLogic()); );
    return VisitLogic(node,result);
  }
}

bool Generator::Visit( const ast::Ternary& node , ExprResult* result ) {
  lava_debug(NORMAL,lava_verify(result->GetHint()););
  Register output(result->GetHint().Get());
  BytecodeBuilder::Label cond_label;

  {
    ScopedRegister cond(this);
    // Generate code for condition , don't have to be in Acc register
    if(!VisitExpression(*node._1st,&cond)) return false;
    // Based on the current condition to branch
    cond_label =
      func_scope()->bb()->jmpf( func_scope()->ra()->base(),node.sci(),cond.Get().index());
    if(!cond_label) {
      Error(ERR_FUNCTION_TOO_LONG,node);
      return false;
    }
  }

  // Now 2nd expression generated here , which is natural fallthrough
  if(!VisitExpressionWithOutputRegister(*node._2nd,output)) return false;

  // After the 2nd expression generated it needs to jump/skip the 3rd expression
  BytecodeBuilder::Label label_2nd = func_scope()->bb()->jmp(func_scope()->ra()->base(),
                                                             node._2nd->sci());
  if(!label_2nd) {
    Error(ERR_FUNCTION_TOO_LONG,node);
    return false;
  }

  // Now false / 3nd expression generated here , which is not a natural fallthrough
  // The conditional failed branch should jump here which is the false branch value
  cond_label.Patch(func_scope()->bb()->CodePosition());
  if(!VisitExpressionWithOutputRegister(*node._3rd,output)) return false;

  // Now merge 2nd expression jump label
  label_2nd.Patch(func_scope()->bb()->CodePosition());

  result->SetRegister(output);
  return true;
}

bool Generator::VisitExpression( const ast::Node& node , ExprResult* result ) {
  if(node.type == ast::LITERAL) {
    return Visit(*node.AsLiteral(),result);
  } else if(node.type == ast::VARIABLE) {
    bool init = false;
    if(!result->GetHint()) {
      init = true;
      Optional<Register> r(func_scope()->ra()->Grab());
      if(!r) {
        Error(ERR_REGISTER_OVERFLOW,node.sci());
        return false;
      }
      result->SetHint(r.Get());
    }

    if(!Visit(*node.AsVariable(),result)) return false;

    if(result->GetHint().Get() != result->reg() && init) {
      // Free temporary register created by this function when it is not
      // used since ast::Variable can have situation not using hintted register
      func_scope()->ra()->Drop(result->GetHint().Get());
    }

    return true;
  } else {
    if(!result->GetHint()) {
      Optional<Register> r(func_scope()->ra()->Grab());
      if(!r) {
        Error(ERR_REGISTER_OVERFLOW,node.sci());
        return false;
      }

      result->SetHint(r.Get());
    }

    switch(node.type) {
      case ast::PREFIX:   return Visit(*node.AsPrefix(),result);
      case ast::UNARY:    return Visit(*node.AsUnary(),result);
      case ast::BINARY:   return Visit(*node.AsBinary(),result);
      case ast::TERNARY:  return Visit(*node.AsTernary(),result);
      case ast::LIST:     return Visit(*node.AsList(),result);
      case ast::OBJECT:   return Visit(*node.AsObject(),result);
      case ast::FUNCTION:
        result->SetRegister(result->GetHint().Get());
        return VisitAnonymousFunction(result->GetHint().Get(),*node.AsFunction());
      default:
        lava_unreachF("Disallowed expression with node type %s",node.node_name());
        return false;
    }
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

bool Generator::VisitExpressionWithOutputRegister( const ast::Node& node , const Register& output ) {
  if(node.IsLiteral()) {
    if(!AllocateLiteral(node.sci(),*node.AsLiteral(),output))
      return false;
  } else {
    ExprResult expr;
    expr.SetHint(output);
    if(!VisitExpression(node,&expr)) return false;
    if(output != expr.reg()) {
      ScopedRegister r(this,expr.reg());
      EEMIT(move,node.sci(),output.index(),r.Get().index());
    }
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
    if(!VisitExpressionWithOutputRegister(*node.expr,lhs.Get())) return false;
  } else {
    // put a null to that value as default value
    SEMIT(loadnull,node.sci(),lhs.Get().index());
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
    if(!VisitExpressionWithOutputRegister(*rhs_node,r.Get()))
      return false;
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
      SEMIT(uvset,node.sci(),upindex,reg.Get().index());
    } else {
      // set it as global variable
      ScopedRegister reg(this);
      if(!VisitExpression(*node.rhs,&reg)) return false;
      std::int32_t ref = func_scope()->bb()->Add(*node.lhs_var->name,context_->gc());
      if(ref<0) {
        Error(ERR_REGISTER_OVERFLOW,node.sci());
        return false;
      }
      SEMIT(gset,node.sci(),ref,reg.Get().index());
    }
  }
  return true;
}

bool Generator::VisitPrefixAssign( const ast::Assign& node ) {
  ScopedRegister lhs(this);
  ScopedRegister rhs(this);

  {
    Optional<Register> r(func_scope()->ra()->Grab());
    if(!r) {
      Error(ERR_REGISTER_OVERFLOW,node.sci());
      return false;
    }
    lhs.Reset(r.Get());
  }

  if(!VisitExpression(*node.rhs,&rhs))
    return false;

  if(!VisitPrefix<false>(*node.lhs_pref,node.lhs_pref->list->size()-1,lhs.Get()))
    return false;

  // Handle the last component
  const ast::Prefix::Component& last_comp = node.lhs_pref->list->Last();
  switch(last_comp.t) {
    case ast::Prefix::Component::DOT:
      {
        std::int32_t ref = func_scope()->bb()->Add(*last_comp.var->name,context_->gc());
        if(ref<0) {
          Error(ERR_TOO_MANY_LITERALS,node);
          return false;
        }
        SEMIT(propset,node.sci(),lhs.Get().index(),rhs.Get().index(),ref);
      }
      break;
    case ast::Prefix::Component::INDEX:
      {
        if(last_comp.expr->IsLiteral() && last_comp.expr->AsLiteral()->IsReal()) {
          // try to narrow the real index to take advantage of idxseti instruction
          std::int32_t iref;
          if(NarrowReal(last_comp.expr->AsLiteral()->real_value,&iref)) {
            if(std::numeric_limits<std::uint8_t>::max() >= iref &&
               std::numeric_limits<std::uint8_t>::min() <= iref) {
              SEMIT(idxseti,node.sci(),lhs.Get().index(),iref,rhs.Get().index());
              break;
            }
          }
        }

        // fallthrough to handle common case
        {
          if(rhs.Get().IsAcc()) {
            if(rhs.Reset(SpillFromAcc(last_comp.expr->sci()))) return false;
          }

          {
            ScopedRegister expr_reg(this);
            if(!VisitExpression(*last_comp.expr,&expr_reg)) return false;

            // idxset REG REG REG
            SEMIT(idxset,node.sci(),lhs.Get().index(),expr_reg.Get().index(),
                                                      rhs.Get().index());
          }
        }
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
  // Discard the result of the evaluation, so we pass in a Acc as hint
  if(!VisitPrefix<false>(*node.call,node.call->list->size(),Register::kAccReg))
    return false;
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
      prev_jmp = func_scope()->bb()->jmpf(func_scope()->ra()->base(),
                                          br.cond->sci(),
                                          cond.Get().index());
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
      label_vec.push_back(func_scope()->bb()->jmp(func_scope()->ra()->base(),br.cond->sci()));
  }

  // Patch prev_jmp if we need to
  if(prev_jmp) {
    prev_jmp.Patch(func_scope()->bb()->CodePosition());
  }

  for(auto &e : label_vec)
    e.Patch(func_scope()->bb()->CodePosition());
  return true;
}

bool Generator::Visit( const ast::For& node ) {
  BytecodeBuilder::Label forward;
  Register induct_reg;
  Register second_reg;
  Register third_reg;

  lava_debug(NORMAL,
      if(!node._1st) {
        // as long as there're no 1st init statement, a for cannot
        // have condition statment and step statement. and it must
        // be a forever loop
        lava_verify(!node._2nd && !node._3rd);
      }
    );

  // handle 1st expression , induction variable
  if(node._1st) {
    if(!Visit(*node._1st,&induct_reg))
      return false;
  }

  // handle 2nd expression, condition variable
  if(node._2nd) {
    second_reg = lexical_scope()->GetLoopIter2();
    if(!VisitExpressionWithOutputRegister(*node._2nd,second_reg))
      return false;
  }

  // handle 3rd/step variable
  if(node._3rd) {
    third_reg = lexical_scope()->GetLoopIter3();
    if(!VisitExpressionWithOutputRegister(*node._3rd,third_reg))
      return false;
  }

  // loop condition comparison , basically a loop inversion
  if(node._2nd) {
    SEMIT(ltvv,node._2nd->sci(),Register::kAccIndex,induct_reg.index(),
                                                    second_reg.index());
  }

  // mark the start of the loop
  if(node._2nd) {
    lava_debug(NORMAL,lava_verify(node._1st););
    forward = func_scope()->bb()->fstart( func_scope()->ra()->base(), node.sci() ,
                                                                      induct_reg.index() );
  } else {
    lava_debug(NORMAL,if(!node._1st) lava_verify(!node._3rd););
    SEMIT(fevrstart,node.sci());
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

    if(node._2nd) {
      if(node._3rd) {
        // 1. We have step and condition variable, use fend2 instruction
        SEMIT(fend2,node.sci(),induct_reg.index(),second_reg.index(),
                                                  third_reg.index(),
                                                  header);
      } else {
        // 2. We only have condition variable, no stepping
        SEMIT(fend1,node.sci(),induct_reg.index(),second_reg.index(),
                                                  0,
                                                  header);
      }
    } else {
      // don't have 2nd and also don't have 3rd( guaranteed by the parser ).
      // this is a forever loop, use fevrend instruction
      SEMIT(fevrend,node.sci(),header);
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
  Register itr_reg(lexical_scope()->GetLoopIter1());

  // Evaluate the interator initial value and force it into iter_reg
  if(!VisitExpressionWithOutputRegister(*node.iter,itr_reg)) return false;


  // Generate the festart. Festart will convert itr_reg into iterator
  // and then do the comparison and other stuff
  BytecodeBuilder::Label forward =
    func_scope()->bb()->festart(func_scope()->ra()->base(),node.sci(),
                                                           itr_reg.index());

  {
    LexicalScope scope(this,true);
    scope.Init(*node.body);

    std::uint16_t header = static_cast<std::uint16_t>(
        func_scope()->bb()->CodePosition());

    Optional<Register> key(func_scope()->GetLocalVar(*node.key->name));
    Optional<Register> val(func_scope()->GetLocalVar(*node.val->name));

    lava_debug(NORMAL,lava_verify(key.Has()););
    lava_debug(NORMAL,lava_verify(val.Has()););

    // Deref the key from iterator register into the target register
    SEMIT(idref,node.key->sci(),key.Get().index(),val.Get().index(),
                                                  itr_reg.index());

    // Visit the chunk
    if(!VisitChunk(*node.body,false)) return false;

    scope.PatchContinue(
        static_cast<std::uint16_t>(func_scope()->bb()->CodePosition()));

    SEMIT(feend,node.sci(),itr_reg.index(),header);


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
    SEMIT(retnull,node.sci());
  } else {
    // Look for return function-call() style code and then perform
    // tail call optimization on this case. Basically, as long as
    // the return is returning a function call , then we can perform
    // tail call optimization since we don't need to return to the
    // previous call frame at all.
    if(CanBeTailCallOptimized(*node.expr)) {
      lava_debug(NORMAL, lava_verify(node.expr->IsPrefix()); );
      if(!VisitPrefix<true>(*node.expr->AsPrefix(),
                            node.expr->AsPrefix()->list->size(),
                            Register::kAccReg))
        return false;
    } else {
      ScopedRegister ret(this);
      if(!VisitExpression(*node.expr,&ret)) return false;
      if(!ret.Get().IsAcc())
        if(!SpillToAcc(node.expr->sci(),&ret))
          return false;
    }
  }
  SEMIT(ret,node.sci());
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

    SEMIT(initcls,node.sci(),idx);
    return true;
  }
  return false;
}

bool Generator::VisitAnonymousFunction( const Register& output ,
                                        const ast::Function& node ) {
  lava_debug(NORMAL,lava_verify(!node.name););
  Handle<Prototype> proto(VisitFunction(node));
  if(proto) {
    std::int32_t idx = script_builder_->AddPrototype(proto);
    if(idx <0) {
      Error(ERR_TOO_MANY_PROTOTYPES,node);
      return false;
    }
    EEMIT(loadcls,node.sci(),output.index(),static_cast<std::uint16_t>(idx));
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

  EEMIT(retnull,SourceCodeInfo());

  Handle<Prototype> main(
      BytecodeBuilder::NewMain(context_->gc(),*scope.bb(),
                                              root_->lv_context->local_variable_count()));
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
