#include "graph-builder.h"
#include "fold.h"
#include "type-inference.h"

#include "src/interpreter/bytecode.h"
#include "src/interpreter/bytecode-iterator.h"

#include <cmath>
#include <vector>
#include <set>
#include <map>

namespace lavascript {
namespace cbase {
namespace hir    {
using namespace ::lavascript::interpreter;

void GraphBuilder::Environment::EnterFunctionScope( Graph* graph , const FuncInfo& func ) {
  // resize the stack to have 256 slots for new prototype
  stack_.resize( stack_.size() + interpreter::kRegisterSize );
}

void GraphBuilder::Environment::ExitFunctionScope( Graph* graph , const FuncInfo& func ) {
  stack_.resize( func.base );
}

Expr* GraphBuilder::GetUpValue( std::uint8_t index , const IRInfoProvider& irinfo ) const {
  // 1. tries to index into upvalue group
  {
    lava_debug(NORMAL,lava_verify(index < upvalue_.size()));
    auto grp = upvalue_[index];
    if(!grp.IsEmpty()) {
      // a read is always free to be reordered, so just return here
      return grp.value();
    }
  }
  // 2. fallback to the global groups and need to create a UGet node
  auto uget = UGet::New(gb_->graph_,index,gb_->method(),irinfo());
  // uget must be ordered by the previous write effect
  uget->AddEffect( root_.write_effect );
  // uget must be updated as the previous read effect
  root_.read_effect = uget;
  return uget;
}

Expr* GraphBuilder::GetGlobal ( const void* key , std::size_t length , const KeyProvider& key_provider ,
                                                                       const IRInfoProvider& irinfo ) const {
  // 1. tries to index into existed global table
  {
    auto itr = global_.find(Str(key,length));
    if(itr != global_.end()) {
      auto &eg = itr->second;
      lava_verify(!eg.IsEmpty());
      return eg.value();
    }
  }
  // 2. fallback to a global get node
  auto gget = GGet::New(gb_->graph_,key_provider(),irinfo(),gb_->region());
  // gget must be ordered by the previous write effect
  gget->AddEffect(root_.write_effect);
  // gget must be updated as previous read effect
  root_.read_effect = gget;
  return gget;
}

void GraphBuilder::SetUpValue( std::uint8_t index , Expr* value , const IRInfoProvider& irinfo ) {
  // 1. setup the upvalue set node USet , regardless whether we can track it or not
  //    this node must be used as a statement node into the current region to ensure
  //    it is flushed back at the correct timing.
  auto uset = USet::New(gb_->graph_,index,gb_->method(),value,irinfo());
  // add it as a statement
  gb_->region()->AddStatement(uset);

  // 2. check whether we have this node tracked
  {
    auto grp = upvalue_[index];
    if(grp.IsEmpty()) {
      // not tracked, update to global side effect node
    }
  }
}

void GraphBuilder::Environment::UpdateEffect( GraphBuilder* gb , SideEffect* effect ) {
  // update the effect node
  effect_ = effect;
  // Go through the *first* function's argument list and add effect to these
  // nodes. We don't need to go through inlined function argument list since
  // they are just be stored as local variables and we do know what value are
  // them since they are inlined
  {
    auto &e = gb->func_info_.front();
    auto arg_sz = e.prototype->argument_size();
    for( std::size_t i = e.base ; i < arg_sz + e.base ; ++i ) {
      auto n = gb->vstk()->at(i);
      if(n->IsArg()) {
        auto arg = n->AsArg();
        if(arg->HasRef()) {
          gb->vstk()->at(i) = Arg::New(gb->graph_,(i - e.base));
        } else {
          arg->AddEffect(effect_);
        }
      }
    }
  }
}

/* -------------------------------------------------------------
 *
 * RAII objects to handle different type of scopes when iterate
 * different bytecode along the way
 *
 * -----------------------------------------------------------*/
class GraphBuilder::OSRScope {
 public:
  OSRScope( GraphBuilder* , const Handle<Prototype>& , ControlFlow* , const std::uint32_t* );
  ~OSRScope() {
    gb_->env()->ExitFunctionScope(gb_->graph_,gb_->func_info_.back());
    gb_->func_info_.pop_back();
  }
 private:
  GraphBuilder* gb_;
};

class GraphBuilder::FuncScope {
 public:
  FuncScope( GraphBuilder* , const Handle<Prototype>& , ControlFlow* , std::uint32_t );
  ~FuncScope() {
    gb_->env()->ExitFunctionScope(gb_->graph_,gb_->func_info_.back());
    gb_->func_info_.pop_back();
  }
 private:
  GraphBuilder* gb_;
};

class GraphBuilder::LoopScope {
 public:
  LoopScope( GraphBuilder* gb , const std::uint32_t* pc ) : gb_(gb) { gb->func_info().EnterLoop(pc); }
  ~LoopScope() { gb_->func_info().LeaveLoop(); }
 private:
  GraphBuilder* gb_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(LoopScope)
};

class GraphBuilder::BackupEnvironment {
 public:
  BackupEnvironment( GraphBuilder::Environment* new_env , GraphBuilder* gb ):
    old_env_(gb->env_), gb_(gb) { gb->env_ = new_env; }

  ~BackupEnvironment() { gb_->env_ = old_env_; }
 private:
  Environment* old_env_;
  GraphBuilder* gb_;
};

GraphBuilder::OSRScope::OSRScope( GraphBuilder* gb , const Handle<Prototype>& proto ,
                                                     ControlFlow* region ,
                                                     const std::uint32_t* osr_start ):
  gb_(gb) {

  // initialize FuncInfo for OSR compilation entry
  FuncInfo temp(proto,region,osr_start);
  // get the loop header information and recursively register all its needed
  // loop info object inside of the FuncInfo object
  auto loop_header = temp.bc_analyze.LookUpLoopHeader(osr_start);
  lava_debug(NORMAL,lava_verify(loop_header););
  // need to iterate the loop one bye one from the top most loop
  // inside of the nested loop cluster so we will use a queue
  {
    std::vector<const BytecodeAnalyze::LoopHeaderInfo*> queue;
    auto cur_header = loop_header->prev; // skip the OSR loop
    while(cur_header) {
      queue.push_back(cur_header);
      cur_header = cur_header->prev;
    }
    for( auto ritr = queue.rbegin() ; ritr != queue.rend(); ++ritr ) {
      temp.loop_info.push_back(LoopInfo(*ritr));
    }
  }
  // add the prototype information into graph
  gb->graph_->AddPrototypeInfo(proto,0);
  // push current FuncInfo object to the trackings tack
  gb->func_info_.push_back(FuncInfo(std::move(temp)));
  // initialize environment object for this OSRScope
  gb->env()->EnterFunctionScope(gb->graph_,gb->func_info());
}

GraphBuilder::FuncScope::FuncScope( GraphBuilder* gb , const Handle<Prototype>& proto ,
                                                       ControlFlow* region ,
                                                       std::uint32_t base  ):
  gb_(gb) {

  gb->graph_->AddPrototypeInfo(proto,base);
  gb->func_info_.push_back(FuncInfo(proto,region,base));
  gb->env()->EnterFunctionScope(gb->graph_,gb->func_info());

  if(gb->func_info_.size() == 1) {
    // initialize the argument value inside of the stack
    auto arg_size = proto->argument_size();
    for( std::size_t i = 0 ; i < arg_size ; ++i ) {
      gb->vstk()->at(i) = Arg::New(gb->graph_,static_cast<std::uint32_t>(i));
    }
  }
}

Expr* GraphBuilder::NewConstNumber( std::int32_t ivalue , const BytecodeLocation& pc ) {
  return Float64::New(graph_,static_cast<double>(ivalue),NewIRInfo(pc));
}

Expr* GraphBuilder::NewConstNumber( std::int32_t ivalue ) {
  return Float64::New(graph_,static_cast<double>(ivalue),NULL);
}

Expr* GraphBuilder::NewNumber( std::uint8_t ref , IRInfo* info ) {
  double real = func_info().prototype->GetReal(ref);
  return Float64::New(graph_,real,info);
}

Expr* GraphBuilder::NewNumber( std::uint8_t ref , const BytecodeLocation& pc ) {
  return NewNumber(ref,NewIRInfo(pc));
}

Expr* GraphBuilder::NewNumber( std::uint8_t ref ) {
  return NewNumber(ref,NULL);
}

Expr* GraphBuilder::NewString( std::uint8_t ref , IRInfo* info ) {
  Handle<String> str(func_info().prototype->GetString(ref));
  if(str->IsSSO()) {
    return SString::New(graph_,str->sso(),info);
  } else {
    return LString::New(graph_,str->long_string(),info);
  }
}

Expr* GraphBuilder::NewString( std::uint8_t ref , const BytecodeLocation& pc ) {
  return NewString(ref,NewIRInfo(pc));
}

Expr* GraphBuilder::NewString( std::uint8_t ref ) {
  return NewString(ref,NULL);
}

Expr* GraphBuilder::NewSSO( std::uint8_t ref , IRInfo* info ) {
  const SSO& sso = *(func_info().prototype->GetSSO(ref)->sso);
  return SString::New(graph_,sso,info);
}

Expr* GraphBuilder::NewSSO( std::uint8_t ref , const BytecodeLocation& pc ) {
  return NewSSO(ref,NewIRInfo(pc));
}

Expr* GraphBuilder::NewSSO( std::uint8_t ref ) {
  const SSO& sso = *(func_info().prototype->GetSSO(ref)->sso);
  return SString::New(graph_,sso,NULL);
}

Expr* GraphBuilder::NewBoolean( bool value , const BytecodeLocation& pc ) {
  return Boolean::New(graph_,value,NewIRInfo(pc));
}

Expr* GraphBuilder::NewBoolean( bool value ) {
  return Boolean::New(graph_,value,NULL);
}

Expr* GraphBuilder::AddTypeFeedbackIfNeed( const StackSlot& idx ,
                                           TypeKind tp,
                                           const BytecodeLocation& pc ) {
  return AddTypeFeedbackIfNeed(idx,tp,NewIRInfo(pc));
}

Expr* GraphBuilder::AddTypeFeedbackIfNeed( const StackSlot& idx ,
                                           const Value& value ,
                                           const BytecodeLocation& pc ) {
  return AddTypeFeedbackIfNeed(idx,value,NewIRInfo(pc));
}


Expr* GraphBuilder::AddTypeFeedbackIfNeed( const StackSlot& idx ,
                                           const Value& value ,
                                           IRInfo* info ) {
  return AddTypeFeedbackIfNeed(idx,MapValueToTypeKind(value),info);
}

Expr* GraphBuilder::AddTypeFeedbackIfNeed( const StackSlot& idx ,
                                           TypeKind tp,
                                           IRInfo* info ) {
  auto n   = idx.node;
  auto stp = GetTypeInference(n);
  if(stp != TPKIND_UNKNOWN) {
    /**
     * This check means our static type inference should match the traced
     * type value here. If not match basically means our IR graph is wrong
     * fundementally so it is a *BUG* actually
     */
    lava_assertF(stp == tp,"This is a *SERIOUS BUG*, we get "
                           "type inference of value %s but "
                           "the traced type is actually %s!",
                           GetTypeKindName(stp), GetTypeKindName(tp));
    return n;
  }

  lava_debug(NORMAL,lava_verify(tp != TPKIND_UNKNOWN););
  auto cp = BuildCheckpoint(info->bc());
  auto guard = TypeGuard::New(graph_,n,tp,cp,info);
  // set the modified node back to where the stack slot is put
  if(idx.HasIndex()) StackSet(idx.index,guard);
  // return the new guarded node
  return guard;
}

// ========================================================================
//
// Unary Node
//
// ========================================================================
Expr* GraphBuilder::NewUnary( const StackSlot& index , Unary::Operator op ,
                                                       const BytecodeLocation& pc ) {
  // 1. try to do a constant folding
  auto new_node = FoldUnary(graph_,op,index.node,[this,pc]() {
        return NewIRInfo(pc);
      }
  );
  if(new_node) return new_node;
  // 2. now we know there's no way we can resolve the expression and
  //    we don't have any type information from static type inference.
  //    fallback to do speculative unary or dynamic dispatch if needed
  return TrySpeculativeUnary(index,op,pc);
}

Expr* GraphBuilder::TrySpeculativeUnary( const StackSlot& index , Unary::Operator op ,
                                                                  const BytecodeLocation& pc ) {
  auto node = index.node;
  // try to get the value feedback from type trace operations
  auto tt = type_trace_.GetTrace( pc.address() );
  if(tt) {
    auto v = tt->data[1]; // unary operation's 1st operand
    if(op == Unary::NOT) {
      // create a guard for this object's boolean value under boolean context
      node = AddTypeFeedbackIfNeed(index,v,pc);
      // do a static boolean inference here
      auto tp = GetTypeInference(node);
      bool bval;
      if(TPKind::ToBoolean(tp,&bval)) {
        return Boolean::New(graph_,!bval,node->ir_info());
      } else if(tp == TPKIND_BOOLEAN) {

        // if the guarded type is boolean, then we could just use BooleanNot
        // node which has type information and enable the inference on it
        return NewBoxNode<BooleanNot>(graph_, TPKIND_BOOLEAN, node->ir_info(),
                 NewUnboxNode(graph_,node,TPKIND_BOOLEAN,node->ir_info()),node->ir_info());
      }
    } else {
      if(v.IsReal()) {
        // add the type feedback for this node
        node = AddTypeFeedbackIfNeed(index,TPKIND_FLOAT64,pc);

        // create a boxed node with gut of Float64Negate node
        return NewBoxNode<Float64Negate>(graph_, TPKIND_FLOAT64, node->ir_info(),
                 NewUnboxNode(graph_,node,TPKIND_FLOAT64,node->ir_info()),node->ir_info());
      }
    }
  }

  // Fallback:
  // okay, we are not able to get *any* types of type information, fallback to
  // generate a fully dynamic dispatch node
  return NewUnaryFallback(index,op,pc);
}

Expr* GraphBuilder::NewUnaryFallback( const StackSlot& index , Unary::Operator op ,
                                                               const BytecodeLocation& pc ) {
  return Unary::New(graph_,index.node,op,NewIRInfo(pc));
}

// ========================================================================
//
// Binary Node
//
// ========================================================================
Expr* GraphBuilder::NewBinary  ( const StackSlot& lidx, const StackSlot& ridx,
                                                        Binary::Operator op ,
                                                        const BytecodeLocation& pc ) {
  auto lhs = lidx.node;
  auto rhs = ridx.node;
  auto new_node = FoldBinary(graph_,op,lhs,rhs,[this,pc]() {
      return NewIRInfo(pc);
  });
  if(new_node) return new_node;

  // try to specialize it into certain specific common cases which doesn't
  // require guard instruction and deoptimization
  new_node = TrySpecialTestBinary(lidx,ridx,op,pc);
  if(new_node) return new_node;
  // try speculative binary node
  new_node = TrySpeculativeBinary(lidx,ridx,op,pc);
  if(new_node) return new_node;
  // fallback to use normal binary dispatch
  return NewBinaryFallback(lidx,ridx,op,pc);
}

Expr* GraphBuilder::TrySpecialTestBinary( const StackSlot& lidx , const StackSlot& ridx ,
                                                                  Binary::Operator op ,
                                                                  const BytecodeLocation& pc ) {
  auto lhs = lidx.node;
  auto rhs = ridx.node;
  if(op == Binary::EQ || op == Binary::NE) {
    if((lhs->IsICall() && rhs->IsString()) || (rhs->IsICall() && lhs->IsString())) {
      /**
       * try to capture the special written code and convert it into IR node
       * which can be optimized later on
       */
      auto icall = lhs->IsICall() ? lhs->AsICall()      : rhs->AsICall();
      auto type  = lhs->IsString()? lhs->AsZoneString() : rhs->AsZoneString();

      if(type == "real") {
        return TestType::New(graph_,TPKIND_FLOAT64  ,icall->GetArgument(0),NewIRInfo(pc));
      } else if(type == "boolean") {
        return TestType::New(graph_,TPKIND_BOOLEAN  ,icall->GetArgument(0),NewIRInfo(pc));
      } else if(type == "null") {
        return TestType::New(graph_,TPKIND_NIL      ,icall->GetArgument(0),NewIRInfo(pc));
      } else if(type == "list") {
        return TestType::New(graph_,TPKIND_LIST     ,icall->GetArgument(0),NewIRInfo(pc));
      } else if(type == "object") {
        return TestType::New(graph_,TPKIND_OBJECT   ,icall->GetArgument(0),NewIRInfo(pc));
      } else if(type == "closure") {
        return TestType::New(graph_,TPKIND_CLOSURE  ,icall->GetArgument(0),NewIRInfo(pc));
      } else if(type == "iterator") {
        return TestType::New(graph_,TPKIND_ITERATOR ,icall->GetArgument(0),NewIRInfo(pc));
      } else if(type == "extension") {
        return TestType::New(graph_,TPKIND_EXTENSION,icall->GetArgument(0),NewIRInfo(pc));
      }
    }
  }

  return NULL; // fallback
}

Expr* GraphBuilder::TrySpeculativeBinary( const StackSlot& lidx , const StackSlot& ridx ,
                                                                  Binary::Operator op,
                                                                  const BytecodeLocation& pc ) {
  auto lhs = lidx.node;
  auto rhs = ridx.node;

  auto tt = type_trace_.GetTrace(pc.address());
  if(tt) {
    auto lhs_val = tt->data[1];
    auto rhs_val = tt->data[2];

    switch(op) {
      case Binary::ADD: case Binary::SUB: case Binary::MUL:
      case Binary::DIV: case Binary::POW: case Binary::MOD:
        if(lhs_val.IsReal() && rhs_val.IsReal()) {
          auto ir_info = NewIRInfo(pc);
          lhs = AddTypeFeedbackIfNeed(lidx,TPKIND_FLOAT64,ir_info);
          rhs = AddTypeFeedbackIfNeed(ridx,TPKIND_FLOAT64,ir_info);

          return NewBoxNode<Float64Arithmetic>(graph_, TPKIND_FLOAT64, ir_info,
                   NewUnboxNode(graph_,lhs,TPKIND_FLOAT64,ir_info),
                   NewUnboxNode(graph_,rhs,TPKIND_FLOAT64,ir_info), op,ir_info);
        }
        break;
      case Binary::LT: case Binary::LE: case Binary::GT:
      case Binary::GE: case Binary::EQ: case Binary::NE:
        if(lhs_val.IsReal() && rhs_val.IsReal()) {
          auto ir_info = NewIRInfo(pc);
          lhs = AddTypeFeedbackIfNeed(lidx,TPKIND_FLOAT64,ir_info);
          rhs = AddTypeFeedbackIfNeed(ridx,TPKIND_FLOAT64,ir_info);
          return NewBoxNode<Float64Compare>(graph_, TPKIND_BOOLEAN, ir_info,
                   NewUnboxNode(graph_,lhs,TPKIND_FLOAT64,ir_info),
                   NewUnboxNode(graph_,rhs,TPKIND_FLOAT64,ir_info), op, ir_info);

        } else if(lhs_val.IsString() && rhs_val.IsString()) {
          auto ir_info = NewIRInfo(pc);
          if((lhs_val.IsSSO() && rhs_val.IsSSO()) && (op == Binary::EQ || op == Binary::NE)) {
            lhs = AddTypeFeedbackIfNeed(lidx,TPKIND_SMALL_STRING,ir_info);
            rhs = AddTypeFeedbackIfNeed(ridx,TPKIND_SMALL_STRING,ir_info);
            if(op == Binary::EQ) {
              return NewBoxNode<SStringEq>(graph_, TPKIND_BOOLEAN, ir_info,
                       NewUnboxNode(graph_,lhs,TPKIND_SMALL_STRING,ir_info),
                       NewUnboxNode(graph_,rhs,TPKIND_SMALL_STRING,ir_info),ir_info);
            } else {
              return NewBoxNode<SStringNe>(graph_, TPKIND_BOOLEAN, ir_info,
                       NewUnboxNode(graph_,lhs,TPKIND_SMALL_STRING,ir_info),
                       NewUnboxNode(graph_,rhs,TPKIND_SMALL_STRING,ir_info),ir_info);
            }
          } else {
            lhs = AddTypeFeedbackIfNeed(lidx,TPKIND_STRING,ir_info);
            rhs = AddTypeFeedbackIfNeed(ridx,TPKIND_STRING,ir_info);
            return NewBoxNode<StringCompare>(graph_, TPKIND_BOOLEAN, ir_info,
                     NewUnboxNode(graph_,lhs,TPKIND_STRING,ir_info),
                     NewUnboxNode(graph_,rhs,TPKIND_STRING,ir_info), op,ir_info);
          }
        }
        break;

      case Binary::AND: case Binary::OR:
        {
          lhs = AddTypeFeedbackIfNeed(lidx,lhs_val,NewIRInfo(pc));
          // simplify the logic expression if we can do so
          auto ret = SimplifyLogic(graph_,lhs,rhs,op,[&]() { return NewIRInfo(pc); });
          if(ret)
            return ret;
          rhs = AddTypeFeedbackIfNeed(ridx,rhs_val,NewIRInfo(pc));
          auto ir_info = NewIRInfo(pc);
          if(GetTypeInference(lhs) == TPKIND_BOOLEAN &&
             GetTypeInference(rhs) == TPKIND_BOOLEAN) {
            return NewBoxNode<BooleanLogic>(graph_, TPKIND_BOOLEAN, ir_info,
                     NewUnboxNode(graph_,lhs,TPKIND_BOOLEAN,ir_info),
                     NewUnboxNode(graph_,rhs,TPKIND_BOOLEAN,ir_info), op,ir_info);
          }
        }
        break;
      default:
        break;
    }
  }

  return NULL;
}

Expr* GraphBuilder::NewBinaryFallback( const StackSlot& lidx , const StackSlot& ridx ,
                                                               Binary::Operator op,
                                                               const BytecodeLocation& pc ) {
  return Binary::New(graph_,lidx.node,ridx.node,op,NewIRInfo(pc));
}

// ========================================================================
//
// Ternary Node
//
// ========================================================================
Expr* GraphBuilder::NewTernary ( const StackSlot& cidx, Expr* lhs, Expr* rhs, const BytecodeLocation& pc ) {
  auto cond = cidx.node;
  auto new_node = FoldTernary(graph_,cond,lhs,rhs,[this,pc]() {
        return NewIRInfo(pc);
      }
  );
  if(new_node) return new_node;
  { // do a guess based on type trace
    auto tt = type_trace_.GetTrace(pc.address());
    if(tt) {
      auto ir_info = NewIRInfo(pc);
      auto a1 = tt->data[0]; // condition's value
      cond = AddTypeFeedbackIfNeed(cidx,a1,ir_info);
      bool bval;
      if(TPKind::ToBoolean(MapValueToTypeKind(a1),&bval)) {
        return (bval ? lhs : rhs);
      }
    }
  }
  // Fallback
  return Ternary::New(graph_,cond,lhs,rhs,NewIRInfo(pc));
}

// ========================================================================
//
// Intrinsic Call Node
//
// ========================================================================
Expr* GraphBuilder::NewICall   ( std::uint8_t a1 , std::uint8_t a2 , std::uint8_t a3 ,
                                                                     bool tcall ,
                                                                     const BytecodeLocation& pc ) {
  IntrinsicCall ic = static_cast<IntrinsicCall>(a1);
  auto base = a2; // new base to get value from current stack
  auto node = ICall::New(graph_,ic,tcall,NewIRInfo(pc));
  for( std::uint8_t i = 0 ; i < a3 ; ++i ) {
    node->AddArgument(StackGet(i,base));
  }
  lava_debug(NORMAL,lava_verify(GetIntrinsicCallArgumentSize(ic) == a3););
  // try to optimize the intrinsic call
  auto ret = FoldIntrinsicCall(graph_,node);

  if(ret) {
    return ret;
  } else {
    return (ret = LowerICall(node)) ? ret : node;
  }
}

Expr* GraphBuilder::LowerICall( ICall* node ) {
  switch(node->ic()) {
    case INTRINSIC_CALL_UPDATE: {
      auto k = node->GetArgument(1);
      if(k->IsString()) {
        return NewPSet(node->GetArgument(0),k,node->GetArgument(2),node->ir_info());
      } else {
        return NewISet(node->GetArgument(0),k,node->GetArgument(2),node->ir_info());
      }
    }
    break;

    case INTRINSIC_CALL_GET: {
      auto k = node->GetArgument(1);
      if(k->IsString()) {
        return NewPGet(node->GetArgument(0),k,node->ir_info());
      } else {
        return NewIGet(node->GetArgument(0),k,node->ir_info());
      }
    }
    break;

    case INTRINSIC_CALL_ITER: {
      return ItrNew::New(graph_,node->GetArgument(0),node->ir_info(),region());
    }
    break;

    default: break;
  }

  return NULL;
}

// ====================================================================
//
// Property Get/Set
//
// ====================================================================
Expr* GraphBuilder::NewPSet( Expr* object , Expr* key , Expr* value , IRInfo* ir_info ) {
  // try to fold the object if object is a literal
  auto v = FoldObjectSet(graph_,object,key,value,[=](){ return ir_info; });
  if(v) return v;
  auto ret = PSet::New(graph_,object,key,value,ir_info,region());
  // check if this pset node has side effect or not
  if(object->HasObservableSideEffect()) {
    // update itself as the new effect node
    env()->UpdateEffect(this,ret);
    // treat it as statement as well
    region()->AddStatement(ret);
  }
  return ret;
}

Expr* GraphBuilder::NewPGet( Expr* object , Expr* key , IRInfo* ir_info ) {
  // here we *do not* do any folding operations and let the later on pass handle it
  // and we just simply do a pget node test plus some guard if needed
  auto v = FoldObjectGet(graph_,object,key,[=](){ return ir_info; });
  if(v) return v;
  // when we reach here we needs to generate guard
  return PGet::New(graph_,object,key,ir_info,region());
}

// ====================================================================
//
// Index Get/Set
//
// ====================================================================
Expr* GraphBuilder::NewISet( Expr* object, Expr* index, Expr* value, IRInfo* ir_info ) {
  if(object->IsIRList() && index->IsFloat64()) {
    auto iidx = static_cast<std::uint32_t>(index->AsFloat64()->value());
    auto list = object->AsIRList();
    if(iidx < list->Size()) {
      auto new_list = IRList::New(graph_,list->Size(),ir_info);
      // create a new list
      std::uint32_t count = 0;
      for( auto itr(list->operand_list()->GetForwardIterator()) ;
           itr.HasNext() ; itr.Move() ) {
        if(iidx != count ) {
          new_list->AddOperand(itr.value());
        } else {
          new_list->AddOperand(value);
        }
        ++count;
      }
      lava_debug(NORMAL,lava_verify(count == list->operand_list()->size()););
      return new_list;
    }
  } else if(index->IsString()) {
    auto v = FoldObjectSet(graph_,object,index,value,[=](){ return ir_info; });
    if(v) return v;
  }

  auto ret = ISet::New(graph_,object,index,value,ir_info,region());
  if(object->HasObservableSideEffect()) {
    env()->UpdateEffect(this,ret);
    region()->AddStatement(ret);
  }
  return ret;
}

Expr* GraphBuilder::NewIGet( Expr* object, Expr* index, IRInfo* ir_info ) {
  if(object->IsIRList() && index->IsFloat64()) {
    auto iidx = static_cast<std::uint32_t>(index->AsFloat64()->value());
    auto list = object->AsIRList();
    if(iidx < list->Size()) {
      return list->operand_list()->Index(iidx);
    }
  } else if(index->IsString()) {
    auto v = FoldObjectGet(graph_,object,index,[=](){ return ir_info;});
    if(v) return v;
  }

  return IGet::New(graph_,object,index,ir_info,region());
}

// ========================================================================
//
// Global Variable
//
// ========================================================================
void GraphBuilder::NewGGet( std::uint8_t a1 , std::uint8_t a2 , const BytecodeLocation& loc , bool sso ) {
  auto info = NewIRInfo(loc);
  auto gget = GGet::New(graph_, (sso ? NewSSO(a2,info) : NewString(a2,info)) , info , region() );
  // add effect chain since global variable must observe the side effect
  gget->AddEffect(env()->effect());
  // put it into the destination slot
  StackSet(a1,gget);
}

void GraphBuilder::NewGSet( std::uint8_t a1 , std::uint8_t a2 , const BytecodeLocation& loc , bool sso ) {
  auto info = NewIRInfo(loc);
  auto gset = GSet::New(graph_, (sso ? NewSSO(a1,info) : NewString(a1,info)) ,StackGet(a2) , info, region());
  // update side effect field
  gset->AddEffect(env()->effect());
  env()->UpdateEffect(this,gset);
  // GSet is a statement instead of expression
  region()->AddStatement(gset);
}

// ========================================================================
//
// UpValue
//
// ========================================================================
void GraphBuilder::NewUGet( std::uint8_t a1 , std::uint8_t a2 , const BytecodeLocation& pc ) {
  auto uget = UGet::New(graph_,a2,method_index(),NewIRInfo(pc));
  uget->AddEffect(env()->effect());
  StackSet(a1,uget);
}

void GraphBuilder::NewUSet( std::uint8_t a1, std::uint8_t a2 , const BytecodeLocation& pc ) {
  auto arg  = StackGet(a2);
  auto uset = USet::New(graph_,a1,method_index(),arg,NewIRInfo(pc));
  // update side effect field
  uset->AddEffect(env()->effect());
  env()->UpdateEffect(this,uset);
  // upvalue set is a statement
  region()->AddStatement(uset);
}

// =========================================================================
//
// Graph Consturction
//
// =========================================================================
IRInfo* GraphBuilder::NewIRInfo( const BytecodeLocation& pc ) {
  IRInfo* ret;
  void* mem = graph_->zone()->Malloc(sizeof(IRInfo));
  ret = ConstructFromBuffer<IRInfo> (mem,method_index(),pc);
  return ret;
}

IRInfo* GraphBuilder::NewIRInfo( BytecodeIterator* itr ) {
  return NewIRInfo(itr->bytecode_location());
}

Checkpoint* GraphBuilder::BuildCheckpoint( const BytecodeLocation& pc ) {
  auto cp = Checkpoint::New(graph_);

  // get the register offset to decide offset for all the temporary register
  const std::uint32_t* pc_start = func_info().prototype->code_buffer();
  std::uint32_t diff = pc.address() - pc_start;
  std::uint8_t offset = func_info().prototype->GetRegOffset(diff);
  const std::uint32_t stack_end = func_info().base + offset;

  for( std::uint32_t i = 0 ; i < stack_end ; ++i ) {
    auto node = vstk()->at(i);
    if(node) cp->AddStackSlot(node,i);
  }
  return cp;
}

void GraphBuilder::GeneratePhi( ValueStack* dest , const ValueStack& lhs ,
                                                   const ValueStack& rhs ,
                                                   std::size_t base ,
                                                   ControlFlow* region ,
                                                   IRInfo* info ) {
  const std::size_t sz = std::min(lhs.size() , rhs.size());

  for( std::size_t i = base ; i < sz ; ++i ) {
    Expr* l = lhs[i];
    Expr* r = rhs[i];
    // only when l and r both exists we know this is variable that is available after
    // the branch or jump
    if(l && r) {
      if(l != r) {
        dest->at(i) = Phi::New(graph_,l,r,region,info);
      } else {
        dest->at(i) = l;
      }
    }
  }
}

void GraphBuilder::InsertPhi( Environment* lhs , Environment* rhs , ControlFlow* region , IRInfo* info ) {
  GeneratePhi(vstk() ,*lhs->stack()  ,*rhs->stack()  ,func_info().base   ,region,info);
  if(lhs->effect() != rhs->effect()) {
    // merge the effect node of both environment
    auto ephi = EffectPhi::New(graph_, lhs->effect(), rhs->effect(), region , info);
    // update the merged effect phi into current environment
    env()->UpdateEffect(this,ephi);
  }
}

void GraphBuilder::PatchUnconditionalJump( UnconditionalJumpList* jumps ,
                                           ControlFlow* region ,
                                           const BytecodeLocation& pc ) {
  IRInfo* info = NULL;
  for( auto& e : *jumps ) {
    lava_debug(NORMAL,lava_verify(e.pc == pc.address()););
    lava_verify(e.node->TrySetTarget(pc.address(),region));
    region->AddBackwardEdge(e.node);
    if(!info) info = NewIRInfo(pc);
    InsertPhi(env(),&e.env,region,info);
  }
  jumps->clear();
}

GraphBuilder::StopReason GraphBuilder::BuildLogic( BytecodeIterator* itr ) {
  lava_debug(NORMAL,lava_verify(itr->opcode() == BC_AND || itr->opcode() == BC_OR););
  bool op_and = itr->opcode() == BC_AND ? true : false;
  std::uint8_t lhs,rhs,dummy;
  std::uint32_t pc;
  itr->GetOperand(&lhs,&rhs,&dummy,&pc);
  // where we should end for the other part of the logical cominator
  const std::uint32_t* end_pc = itr->OffsetAt(pc);
  Expr* lhs_expr = StackGet(lhs);
  lava_debug(NORMAL,lava_verify(lhs_expr););
  StackSet(rhs,lhs_expr);
  { // evaluate the rhs
    itr->Move();
    StopReason reason = BuildBasicBlock(itr,end_pc);
    lava_verify(reason == STOP_END);
  }

  Expr* rhs_expr = StackGet(rhs);
  lava_debug(NORMAL,lava_verify(rhs_expr););
  if(op_and)
    StackSet(rhs,
        NewBinary(StackSlot(lhs_expr,lhs),StackSlot(rhs_expr,rhs),Binary::AND,itr->bytecode_location()));
  else
    StackSet(rhs,
        NewBinary(StackSlot(lhs_expr,lhs),StackSlot(rhs_expr,rhs),Binary::OR ,itr->bytecode_location()));
  return STOP_SUCCESS;
}

GraphBuilder::StopReason GraphBuilder::BuildTernary( BytecodeIterator* itr ) {
  lava_debug(NORMAL,lava_verify(itr->opcode() == BC_TERN););
  std::uint8_t cond , result , dummy;
  std::uint32_t offset;
  std::uint16_t final_cursor;
  Expr* lhs, *rhs;

  itr->GetOperand(&cond,&result,&dummy,&offset);
  { // evaluate the fall through branch
    for( itr->Move() ; itr->HasNext() ; ) {
      if(itr->opcode() == BC_JMP) break; // end of the first ternary fall through branch

      if(BuildBytecode(itr) == STOP_BAILOUT)
        return STOP_BAILOUT;
    }

    lava_debug(NORMAL,lava_verify(itr->opcode() == BC_JMP););
    itr->GetOperand(&final_cursor);
    lhs = StackGet(result);
  }

  const std::uint32_t* end_pc = itr->OffsetAt(final_cursor);
  { // evaluate the jump branch
    lava_debug(NORMAL,StackReset(result););
    lava_debug(NORMAL,itr->Move();lava_verify(itr->pc() == itr->OffsetAt(offset)););

    while(itr->HasNext()) {
      if(itr->pc() == end_pc) break;

      if(BuildBytecode(itr) == STOP_BAILOUT)
        return STOP_BAILOUT;
    }

    rhs = StackGet(result);
    lava_debug(NORMAL,lava_verify(rhs););
  }

  StackSet(result, NewTernary(StackGetSlot(cond),lhs,rhs,itr->bytecode_location()));
  return STOP_SUCCESS;
}

GraphBuilder::StopReason GraphBuilder::GotoIfEnd( BytecodeIterator* itr, const std::uint32_t* pc ) {
  StopReason ret;
  lava_verify(itr->SkipTo(
    [pc,&ret]( BytecodeIterator* itr ) {
      if(itr->pc() == pc) {
        ret = STOP_END;
        return false;
      } else if(itr->opcode() == BC_JMP) {
        ret = STOP_JUMP;
        return false;
      }
      return true;
  }));
  return ret;
}

GraphBuilder::StopReason GraphBuilder::BuildIfBlock( BytecodeIterator* itr , const std::uint32_t* pc ) {
  while( itr->HasNext() ) {
    // check whether we reache end of PC where we suppose to stop
    if(pc == itr->pc()) return STOP_END;

    // check whether we have a unconditional jump or not
    if(itr->opcode() == BC_JMP) {
      return STOP_JUMP;
    } else if(IsBlockJumpBytecode(itr->opcode())) {
      if(BuildBytecode(itr) == STOP_BAILOUT)
        return STOP_BAILOUT;
      return GotoIfEnd(itr,pc);
    } else {
      if(BuildBytecode(itr) == STOP_BAILOUT)
        return STOP_BAILOUT;
    }
  }
  lava_unreachF("cannot reach here since it is end of the stream %p:%p",itr->pc(),pc);
  return STOP_EOF;
}

GraphBuilder::StopReason
GraphBuilder::BuildIf( BytecodeIterator* itr ) {
  lava_debug(NORMAL,lava_verify(itr->opcode() == BC_JMPF););

  std::uint8_t cond;    // condition's register
  std::uint16_t offset; // jump target when condition evaluated to be false
  itr->GetOperand(&cond,&offset);

  // create the leading If node
  If*      if_region    = If::New(graph_,StackGet(cond),region());
  IfFalse* false_region = IfFalse::New(graph_,if_region);
  IfTrue*  true_region  = IfTrue::New(graph_,if_region);
  ControlFlow* lhs      = NULL;
  ControlFlow* rhs      = NULL;
  Region*  merge        = Region::New(graph_);

  if_region->set_merge(merge);

  Environment true_env(*env());

  std::uint16_t final_cursor;
  bool have_false_branch;

  // 1. Build code inside of the *true* branch and it will also help us to
  //    identify whether we have dangling elif/else branch
  {
    // skip BC_JMP
    itr->Move();
    // backup the old stack and use the new stack to do simulation
    BackupEnvironment backup(&true_env,this);
    // swith to a true region
    set_region(true_region);
    {
      StopReason reason = BuildIfBlock(itr,itr->OffsetAt(offset));
      if(reason == STOP_BAILOUT) {
        return STOP_BAILOUT;
      } else if(reason == STOP_JUMP) {
        // we do have a none empty false_branch
        have_false_branch = true;
        lava_debug(NORMAL,lava_verify(itr->opcode() == BC_JMP););
        itr->GetOperand(&final_cursor);
      } else {
        lava_debug(NORMAL,lava_verify(reason == STOP_END););
        have_false_branch = false;
      }
    }
    rhs = region();
  }

  // 2. Build code inside of the *false* branch
  if(have_false_branch) {
    set_region(false_region);
    // goto false branch
    itr->BranchTo(offset);
    if(BuildIfBlock(itr,itr->OffsetAt(final_cursor)) == STOP_BAILOUT)
      return STOP_BAILOUT;
    lhs = region();
  } else {
    // reach here means we don't have a elif/else branch
    final_cursor = offset;
    lhs = false_region;
  }
  // 3. set the merge backward edge
  merge->AddBackwardEdge(lhs);
  merge->AddBackwardEdge(rhs);
  itr->BranchTo(final_cursor);
  set_region(merge);

  // 3. handle PHI node
  InsertPhi(env(),&true_env,merge,NewIRInfo(itr->bytecode_location()));
  return STOP_SUCCESS;
}

GraphBuilder::StopReason
GraphBuilder::GotoLoopEnd( BytecodeIterator* itr ) {
  lava_verify(itr->SkipTo(
    []( BytecodeIterator* itr ) {
      return !(itr->opcode() == BC_FEEND || itr->opcode() == BC_FEND1 ||
               itr->opcode() == BC_FEND2 || itr->opcode() == BC_FEVREND);
    }
  ));

  return STOP_SUCCESS;
}

GraphBuilder::StopReason
GraphBuilder::BuildLoopBlock( BytecodeIterator* itr ) {
  while(itr->HasNext()) {
    if(IsLoopEndBytecode(itr->opcode())) {
      return STOP_SUCCESS;
    } else if(IsBlockJumpBytecode(itr->opcode())) {
      if(BuildBytecode(itr) == STOP_BAILOUT)
        return STOP_BAILOUT;
      return GotoLoopEnd(itr);
    } else {
      if(BuildBytecode(itr) == STOP_BAILOUT)
        return STOP_BAILOUT;
    }
  }

  lava_unreachF("%s","must be closed by BC_FEEND/BC_FEND1/BC_FEND2/BC_FEVREND");
  return STOP_BAILOUT;
}

void GraphBuilder::GenerateLoopPhi( const BytecodeLocation& pc ) {
  auto ir_info = NewIRInfo(pc);
  const std::size_t len = func_info().current_loop_header()->phi.var.size();
  for( std::size_t i = 0 ; i < len ; ++i ) {
    if(func_info().current_loop_header()->phi.var[i]) {
      Expr* old = StackGet(static_cast<std::uint32_t>(i));
      lava_debug(NORMAL,lava_verify(old););
      Phi* phi = Phi::New(graph_,region(),ir_info);
      phi->AddOperand(old);
      StackSet(static_cast<std::uint32_t>(i),phi);
      func_info().current_loop().AddPhi(static_cast<std::uint8_t>(i),phi);
    }
  }
}

void GraphBuilder::PatchLoopPhi() {
  for( auto & e: func_info().current_loop().phi_list ) {
    Phi*  phi  = e.phi;
    if(phi->IsUsed()) {
      Expr* node = StackGet(e.idx);
      lava_debug(NORMAL,lava_verify(phi != node););
      phi->AddOperand(node);
    } else {
      Phi::RemovePhiFromRegion(phi);
    }
  }
  func_info().current_loop().phi_list.clear();
}

Expr* GraphBuilder::BuildLoopEndCondition( BytecodeIterator* itr , ControlFlow* body ) {
  // now we should stop at the FEND1/FEND2/FEEND instruction
  if(itr->opcode() == BC_FEND1) {
    std::uint8_t a1,a2,a3; std::uint32_t a4;
    itr->GetOperand(&a1,&a2,&a3,&a4);
    return NewBinary(StackGetSlot(a1),StackGetSlot(a2),Binary::LT,itr->bytecode_location());
  } else if(itr->opcode() == BC_FEND2) {
    std::uint8_t a1,a2,a3; std::uint32_t a4;
    itr->GetOperand(&a1,&a2,&a3,&a4);

    Expr* induct = StackGet(a1);
    lava_debug(NORMAL,lava_verify(induct->IsPhi()););
    // the addition node will use the PHI node as its left hand side
    auto addition = NewBinary(StackGetSlot(a1),StackGetSlot(a3), Binary::ADD, itr->bytecode_location());
    // store the PHI node back to the slot
    StackSet(a1,addition);
    // construct comparison node
    return NewBinary(StackGetSlot(a1),StackGetSlot(a2), Binary::LT, itr->bytecode_location());
  } else if(itr->opcode() == BC_FEEND) {
    std::uint8_t a1;
    std::uint16_t pc;
    itr->GetOperand(&a1,&pc);

    ItrNext* comparison = ItrNext::New(graph_,StackGet(a1),NewIRInfo(itr->bytecode_location()),region());
    return comparison;
  } else {
    return NewBoolean(true,itr->bytecode_location());
  }
}

GraphBuilder::StopReason
GraphBuilder::BuildLoopBody( BytecodeIterator* itr , ControlFlow* loop_header ) {
  Loop*       body        = NULL;
  LoopExit*   exit        = NULL;
  Region*     after       = Region::New(graph_);
  Environment loop_env   (*env());

  if(loop_header->IsLoopHeader()) {
    loop_header->AsLoopHeader()->set_merge(after);
    // Only link the if_false edge back to loop header when it is actually a
    // loop header type. During OSR compilation , since we don't have a real
    // loop header , so we don't need to link it back
    after->AddBackwardEdge(loop_header);
  }

  BytecodeLocation cont_pc;
  BytecodeLocation brk_pc ;

  {
    // backup the old environment and use a temporary environment
    BackupEnvironment backup(&loop_env,this);
    // entier the loop scope
    LoopScope lscope(this,itr->pc());
    // create new loop body node
    body = Loop::New(graph_);
    // set it as the current region node
    set_region(body);
    // generate PHI node at the head of the *block*
    GenerateLoopPhi(itr->bytecode_location());
    // iterate all BC inside of the loop body
    StopReason reason = BuildLoopBlock(itr);
    if(reason == STOP_BAILOUT) return STOP_BAILOUT;
    lava_debug(NORMAL, lava_verify(reason == STOP_SUCCESS || reason == STOP_JUMP););
    cont_pc = itr->bytecode_location(); // continue should jump at current BC which is loop exit node
    // now we should stop at the FEND1/FEND2/FEEND instruction
    auto exit_cond = BuildLoopEndCondition(itr,body);
    lava_debug(NORMAL,lava_verify(!exit););
    exit = LoopExit::New(graph_,exit_cond);
    // connect each control flow node together
    exit->AddBackwardEdge(region());  // NOTES: do not link back to body directly since current
                                      //        region may changed due to new basic block creation

    body->AddBackwardEdge(loop_header);
    body->AddBackwardEdge(exit);
    after->AddBackwardEdge(exit);
    // skip the last end instruction
    itr->Move();
    // patch all the Phi node
    PatchLoopPhi();
    // break should jump here which is *after* the merge region
    brk_pc = itr->bytecode_location();
    // patch all the pending continue and break node
    PatchUnconditionalJump( &func_info().current_loop().pending_continue , exit , cont_pc );
    PatchUnconditionalJump( &func_info().current_loop().pending_break    , after, brk_pc  );
    lava_debug(NORMAL,
        lava_verify(func_info().current_loop().pending_continue.empty());
        lava_verify(func_info().current_loop().pending_break.empty());
        lava_verify(func_info().current_loop().phi_list.empty());
    );
  }
  set_region(after);
  // merge the loop header
  InsertPhi(env(),&loop_env,after,NewIRInfo(itr->bytecode_location()));
  return STOP_SUCCESS;
}

GraphBuilder::StopReason
GraphBuilder::BuildLoop( BytecodeIterator* itr ) {
  lava_debug(NORMAL,lava_verify(IsLoopStartBytecode(itr->opcode())););
  LoopHeader* loop_header = LoopHeader::New(graph_,region());
  // set the current region to be loop header
  set_region(loop_header);
  // construct the loop's first branch. all loop here are automatically
  // inversed
  if(itr->opcode() == BC_FSTART) {
    std::uint8_t a1; std::uint16_t a2;
    itr->GetOperand(&a1,&a2);
    loop_header->set_condition(StackGet(interpreter::kAccRegisterIndex));
  } else if(itr->opcode() == BC_FESTART) {
    std::uint8_t a1; std::uint16_t a2;
    itr->GetOperand(&a1,&a2);
    // create ir ItrNew which basically initialize the itr and also do
    // a test against the iterator to see whether it is workable
    IRInfo* info = NewIRInfo(itr->bytecode_location());
    ItrNew* inew = ItrNew::New(graph_,StackGet(a1),info,region());
    StackSet(a1,inew);
    ItrTest* itest = ItrTest::New(graph_,inew,info,region());
    loop_header->set_condition(itest);
  } else {
    lava_debug(NORMAL,lava_verify(itr->opcode() == BC_FEVRSTART););
    /**
     * for forever loop, we still build the structure of inverse loop, but just
     * mark the condition to be true. later pass for eliminating branch will take
     * care of this false inversed loop if
     */
    loop_header->set_condition(NewBoolean(true,itr->bytecode_location()));
  }
  // skip the loop start bytecode
  itr->Move();
  return BuildLoopBody(itr,loop_header);
}

GraphBuilder::StopReason GraphBuilder::BuildBytecode( BytecodeIterator* itr ) {
  switch(itr->opcode()) {
    /* binary arithmetic/comparison */
    case BC_ADDRV: case BC_SUBRV: case BC_MULRV: case BC_DIVRV: case BC_MODRV: case BC_POWRV:
    case BC_LTRV:  case BC_LERV : case BC_GTRV : case BC_GERV : case BC_EQRV : case BC_NERV :
      {
        std::uint8_t dest , a1, a2;
        itr->GetOperand(&dest,&a1,&a2);
        auto node = NewBinary(StackSlot(NewNumber(a1)), StackGetSlot(a1),
                                                        Binary::BytecodeToOperator(itr->opcode()),
                                                        itr->bytecode_location());
        StackSet(dest,node);
      }
      break;
    case BC_ADDVR: case BC_SUBVR: case BC_MULVR: case BC_DIVVR: case BC_MODVR: case BC_POWVR:
    case BC_LTVR : case BC_LEVR : case BC_GTVR : case BC_GEVR : case BC_EQVR : case BC_NEVR :
      {
        std::uint8_t dest , a1, a2;
        itr->GetOperand(&dest,&a1,&a2);
        auto node = NewBinary(StackGetSlot(a1), StackSlot(NewNumber(a2)),
                                                Binary::BytecodeToOperator(itr->opcode()),
                                                itr->bytecode_location());
        StackSet(dest,node);
      }
      break;
    case BC_ADDVV: case BC_SUBVV: case BC_MULVV: case BC_DIVVV: case BC_MODVV: case BC_POWVV:
    case BC_LTVV : case BC_LEVV : case BC_GTVV : case BC_GEVV : case BC_EQVV : case BC_NEVV :
      {
        std::uint8_t dest, a1, a2;
        itr->GetOperand(&dest,&a1,&a2);
        auto node = NewBinary(StackGetSlot(a1), StackGetSlot(a2),
                                                Binary::BytecodeToOperator(itr->opcode()),
                                                itr->bytecode_location());
        StackSet(dest,node);
      }
      break;
    case BC_EQSV: case BC_NESV:
      {
        std::uint8_t dest, a1, a2;
        itr->GetOperand(&dest,&a1,&a2);
        auto node = NewBinary(StackSlot(NewString(a1)), StackGetSlot(a2),
                                                        Binary::BytecodeToOperator(itr->opcode()),
                                                        itr->bytecode_location());
        StackSet(dest,node);
      }
      break;
    case BC_EQVS: case BC_NEVS:
      {
        std::uint8_t dest, a1, a2;
        itr->GetOperand(&dest,&a1,&a2);
        auto node = NewBinary(StackGetSlot(a1),StackSlot(NewString(a2)),
                                               Binary::BytecodeToOperator(itr->opcode()),
                                               itr->bytecode_location());
        StackSet(dest,node);
      }
      break;
    case BC_AND: case BC_OR:
      return BuildLogic(itr);

    case BC_TERN:
      return BuildTernary(itr);

    /* unary operation */
    case BC_NEGATE: case BC_NOT:
      {
        std::uint8_t dest, src;
        itr->GetOperand(&dest,&src);
        auto node = NewUnary(StackGetSlot(src), Unary::BytecodeToOperator(itr->opcode()) ,
                                                itr->bytecode_location());
        StackSet(dest,node);
      }
      break;
    /* move */
    case BC_MOVE:
      {
        std::uint8_t dest,src;
        itr->GetOperand(&dest,&src);
        StackSet(dest,StackGet(src));
      }
      break;
    /* loading */
    case BC_LOAD0: case BC_LOAD1: case BC_LOADN1:
      {
        std::uint8_t dest;
        itr->GetOperand(&dest);
        std::int32_t num = 0;
        if(itr->opcode() == BC_LOAD1)
          num = 1;
        else if(itr->opcode() == BC_LOADN1)
          num = -1;
        StackSet(dest,NewConstNumber(num,itr->bytecode_location()));
      }
      break;
    case BC_LOADR:
      {
        std::uint8_t dest,src;
        itr->GetOperand(&dest,&src);
        StackSet(dest,NewNumber(src,itr->bytecode_location()));
      }
      break;
    case BC_LOADSTR:
      {
        std::uint8_t dest,src;
        itr->GetOperand(&dest,&src);
        StackSet(dest,NewString(src,itr->bytecode_location()));
      }
      break;
    case BC_LOADTRUE: case BC_LOADFALSE:
      {
        std::uint8_t dest;
        itr->GetOperand(&dest);
        StackSet(dest,NewBoolean(itr->opcode() == BC_LOADTRUE,itr->bytecode_location()));
      }
      break;
    case BC_LOADNULL:
      {
        std::uint8_t dest;
        itr->GetOperand(&dest);
        StackSet(dest,Nil::New(graph_,NewIRInfo(itr->bytecode_location())));
      }
      break;
    /* list */
    case BC_LOADLIST0:
      {
        std::uint8_t a1;
        itr->GetOperand(&a1);
        StackSet(a1,IRList::New(graph_,0,NewIRInfo(itr->bytecode_location())));
      }
      break;
    case BC_LOADLIST1:
      {
        std::uint8_t a1,a2;
        itr->GetOperand(&a1,&a2);
        IRList* list = IRList::New(graph_,1,NewIRInfo(itr->bytecode_location()));
        list->Add(StackGet(a2));
        StackSet(a1,list);
      }
      break;
    case BC_LOADLIST2:
      {
        std::uint8_t a1,a2,a3;
        itr->GetOperand(&a1,&a2,&a3);
        IRList* list = IRList::New(graph_,2,NewIRInfo(itr->bytecode_location()));
        list->Add(StackGet(a2));
        list->Add(StackGet(a3));
        StackSet(a1,list);
      }
      break;
    case BC_NEWLIST:
      {
        std::uint8_t a1,a2;
        itr->GetOperand(&a1,&a2);
        IRList* list = IRList::New(graph_,a2,NewIRInfo(itr->bytecode_location()));
        StackSet(a1,list);
      }
      break;
    case BC_ADDLIST:
      {
        std::uint8_t a1,a2,a3;
        itr->GetOperand(&a1,&a2,&a3);
        IRList* l = StackGet(a1)->AsIRList();
        for( std::size_t i = 0 ; i < a3 ; ++i ) {
          l->Add(StackGet(a2+i));
        }
      }
      break;
    /* objects */
    case BC_LOADOBJ0:
      {
        std::uint8_t a1;
        itr->GetOperand(&a1);
        StackSet(a1,IRObject::New(graph_,0,NewIRInfo(itr->bytecode_location())));
      }
      break;
    case BC_LOADOBJ1:
      {
        std::uint8_t a1,a2,a3;
        auto ir_info = NewIRInfo(itr->bytecode_location());
        itr->GetOperand(&a1,&a2,&a3);
        IRObject* obj = IRObject::New(graph_,1,ir_info);
        obj->Add(StackGet(a2),StackGet(a3),ir_info);
        StackSet(a1,obj);
      }
      break;
    case BC_NEWOBJ:
      {
        std::uint8_t a1,a2;
        itr->GetOperand(&a1,&a2);
        StackSet(a1,IRObject::New(graph_,a2,NewIRInfo(itr->bytecode_location())));
      }
      break;
    case BC_ADDOBJ:
      {
        std::uint8_t a1,a2,a3;
        itr->GetOperand(&a1,&a2,&a3);
        IRObject* obj = StackGet(a1)->AsIRObject();
        obj->Add(StackGet(a2),StackGet(a3),NewIRInfo(itr->bytecode_location()));
      }
      break;
    case BC_LOADCLS:
      {
        std::uint8_t a1;
        std::uint16_t a2;
        itr->GetOperand(&a1,&a2);
        StackSet(a1,LoadCls::New(graph_,a2,NewIRInfo(itr->bytecode_location())));
      }
      break;
    /* property/upvalue/globals */
    case BC_PROPGET: case BC_PROPGETSSO:
      {
        std::uint8_t a1,a2,a3;
        itr->GetOperand(&a1,&a2,&a3);
        Expr* key = (itr->opcode() == BC_PROPGET ? NewString(a3): NewSSO   (a3));
        StackSet(a1,NewPGet(StackGet(a2),key,NewIRInfo(itr)));
      }
      break;
    case BC_PROPSET: case BC_PROPSETSSO:
      {
        std::uint8_t a1,a2,a3;
        itr->GetOperand(&a1,&a2,&a3);
        Expr* key = (itr->opcode() == BC_PROPSET ? NewString(a2): NewSSO   (a2));
        StackSet(a1,NewPSet(StackGet(a1),key,StackGet(a3),NewIRInfo(itr)));
      }
      break;
    case BC_IDXGET: case BC_IDXGETI:
      {
        std::uint8_t a1,a2,a3;
        itr->GetOperand(&a1,&a2,&a3);
        Expr* key = (itr->opcode() == BC_IDXGET ? StackGet(a3) : NewConstNumber(a3));
        StackSet(a1,NewIGet(StackGet(a2),key,NewIRInfo(itr)));
      }
      break;
    case BC_IDXSET: case BC_IDXSETI:
      {
        std::uint8_t a1,a2,a3;
        itr->GetOperand(&a1,&a2,&a3);
        Expr* key = (itr->opcode() == BC_IDXSET ? StackGet(a2) : NewConstNumber(a2));
        StackSet(a1,NewISet(StackGet(a1),key,StackGet(a3),NewIRInfo(itr)));
      }
      break;

    case BC_UVGET:
      {
        std::uint8_t a1,a2;
        itr->GetOperand(&a1,&a2);
        NewUGet(a1,a2,itr->bytecode_location());
      }
      break;
    case BC_UVSET:
      {
        std::uint8_t a1,a2;
        itr->GetOperand(&a1,&a2);
        NewUSet(a1,a2,itr->bytecode_location());
      }
      break;
    case BC_GGET: case BC_GGETSSO:
      {
        std::uint8_t a1,a2;
        itr->GetOperand(&a1,&a2);
        NewGGet(a1,a2,itr->bytecode_location(),itr->opcode() == BC_GGETSSO);
      }
      break;
    case BC_GSET: case BC_GSETSSO:
      {
        std::uint8_t a1,a2;
        itr->GetOperand(&a1,&a2);
        NewGSet(a1,a2,itr->bytecode_location(),itr->opcode() == BC_GSETSSO);
      }
      break;

    /* call/icall */
    case BC_ICALL:
      {
        std::uint8_t a1,a2,a3;
        itr->GetOperand(&a1,&a2,&a3);
        StackSet(kAccRegisterIndex,NewICall(a1,a2,a3,false,itr->bytecode_location()));
      }
      break;

    case BC_TICALL:
      {
        std::uint8_t a1,a2,a3;
        itr->GetOperand(&a1,&a2,&a3);
        StackSet(kAccRegisterIndex,NewICall(a1,a2,a3,true ,itr->bytecode_location()));
      }
      break;

    /* branch */
    case BC_JMPF:
      return BuildIf(itr);

    /* loop */
    case BC_FSTART: case BC_FESTART: case BC_FEVRSTART:
      return BuildLoop(itr);

    /* iterator dereference */
    case BC_IDREF:
      lava_debug(NORMAL,lava_verify(func_info().HasLoop()););
      {
        std::uint8_t a1,a2,a3;
        itr->GetOperand(&a1,&a2,&a3);
        ItrDeref*  iref = ItrDeref::New(graph_,StackGet(a3),NewIRInfo(itr->bytecode_location()),region());
        Projection* key = Projection::New(graph_,iref,ItrDeref::PROJECTION_KEY,
            NewIRInfo(itr->bytecode_location()));
        Projection* val = Projection::New(graph_,iref,ItrDeref::PROJECTION_VAL,
            NewIRInfo(itr->bytecode_location()));

        StackSet(a1,key);
        StackSet(a2,val);
      }
      break;

    /* loop control */
    case BC_BRK: case BC_CONT:
      lava_debug(NORMAL,lava_verify(func_info().HasLoop()););
      {
        std::uint16_t pc;
        itr->GetOperand(&pc);

        /** OffsetAt(pc) returns jump target address **/
        Jump* jump = Jump::New(graph_,itr->OffsetAt(pc),region());
        set_region(jump);

        if(itr->opcode() == BC_BRK)
          func_info().current_loop().AddBreak   (jump,itr->OffsetAt(pc),*env());
        else
          func_info().current_loop().AddContinue(jump,itr->OffsetAt(pc),*env());
      }
      break;

    /* return/return null */
    case BC_RET: case BC_RETNULL:
      {
        Expr* retval = itr->opcode() == BC_RET ? StackGet(interpreter::kAccRegisterIndex) :
                                                 Nil::New(graph_,NewIRInfo(itr->bytecode_location()));
        Return* ret= Return::New(graph_,retval,region());
        set_region(ret);
        func_info().return_list.push_back(ret);
      }
      break;

    default:
      lava_unreachF("ouch, bytecode %s cannot reach here !",itr->opcode_name());
      break;
  }

  itr->Move(); // consume this bytecode
  return STOP_SUCCESS;
}

GraphBuilder::StopReason
GraphBuilder::BuildBasicBlock( BytecodeIterator* itr , const std::uint32_t* end_pc ) {
  while(itr->HasNext()) {
    if(itr->pc() == end_pc) return STOP_END;
    // save the opcode from the bytecode iterator
    auto opcode = itr->opcode();
    // build this instruction
    if(BuildBytecode(itr) == STOP_BAILOUT) return STOP_BAILOUT;
    // check if last opcode is break or continue which is unconditional
    // jump. so we can just abort the construction of this basic block
    if(IsBlockJumpBytecode(opcode)) return STOP_JUMP;
  }
  return STOP_SUCCESS;
}

bool GraphBuilder::Build( const Handle<Prototype>& entry , Graph* graph ) {
  graph_ = graph;
  zone_  = graph->zone();

  // 1. create the start and end region
  Start* start = Start::New(graph_);
  End*   end = NULL;
  Fail* fail   = Fail::New(graph_);
  Success* succ= Success::New(graph_);

  // create the first region
  Region* region = Region::New(graph_,start);

  // 2. start the basic block building
  {
    // setup the main environment object
    Environment root_env(graph_);
    BackupEnvironment backup(&root_env,this);
    // enter into the top level function
    FuncScope scope(this,entry,region,0);
    // setup the bytecode iterator
    BytecodeIterator itr(entry->GetBytecodeIterator());
    // set the current region
    set_region(region);
    // start to execute the build basic block
    if(BuildBasicBlock(&itr) == STOP_BAILOUT)
      return false;
    // finish return value Phi generation
    {
      Phi* return_value = Phi::New(graph_,succ,NULL);
      for( auto &e : func_info().return_list ) {
        return_value->AddOperand(e->AsReturn()->value());
        succ->AddBackwardEdge(e);
      }
      end = End::New(graph_,succ,fail);
    }
  }
  // initialize the graph
  graph->Initialize(start,end);
  return true;
}

void GraphBuilder::BuildOSRLocalVariable() {
  auto loop_header = func_info().bc_analyze.LookUpLoopHeader(func_info().osr_start);
  lava_debug(NORMAL,lava_verify(loop_header););
  for( BytecodeAnalyze::LocalVariableIterator itr(loop_header->bb,func_info().bc_analyze);
       itr.HasNext() ; itr.Move() ) {
    lava_debug(NORMAL,lava_verify(!vstk()->at(itr.value())););
    vstk()->at(itr.value()) = OSRLoad::New(graph_,itr.value());
  }
}

GraphBuilder::StopReason
GraphBuilder::GotoOSRBlockEnd( BytecodeIterator* itr , const std::uint32_t* end_pc ) {
  StopReason ret;
  lava_verify(itr->SkipTo(
    [end_pc,&ret]( BytecodeIterator* itr ) {
      if(itr->pc() == end_pc) {
        ret = STOP_END;
        return false;
      } else if(itr->opcode() == BC_FEND1 || itr->opcode() == BC_FEND2 ||
                itr->opcode() == BC_FEEND || itr->opcode() == BC_FEVREND) {
        ret = STOP_SUCCESS;
        return false;
      } else {
        return true;
      }
    }
  ));
  return ret;
}

GraphBuilder::StopReason
GraphBuilder::BuildOSRLoop( BytecodeIterator* itr ) {
  lava_debug(NORMAL, lava_verify(func_info().IsOSR());
                     lava_verify(func_info().osr_start == itr->pc()););
  return BuildLoopBody(itr,region());
}

void GraphBuilder::SetupOSRLoopCondition( BytecodeIterator* itr ) {
  // now we should stop at the FEND1/FEND2/FEEND instruction
  if(itr->opcode() == BC_FEND1) {
    std::uint8_t a1,a2,a3; std::uint32_t a4;
    itr->GetOperand(&a1,&a2,&a3,&a4);

    auto comparison = NewBinary(StackGetSlot(a1), StackGetSlot(a2),
                                                  Binary::LT ,
                                                  itr->bytecode_location());

    StackSet(interpreter::kAccRegisterIndex,comparison);
  } else if(itr->opcode() == BC_FEND2) {
    std::uint8_t a1,a2,a3; std::uint32_t a4;
    itr->GetOperand(&a1,&a2,&a3,&a4);

    // the addition node will use the PHI node as its left hand side
    auto addition = NewBinary(StackGetSlot(a1),StackGetSlot(a3),
                                               Binary::ADD,
                                               itr->bytecode_location());
    StackSet(a1,addition);

    auto comparison = NewBinary(StackGetSlot(a1),StackGetSlot(a2),
                                                 Binary::LT,
                                                 itr->bytecode_location());

    StackSet(interpreter::kAccRegisterIndex,comparison);
  } else if(itr->opcode() == BC_FEEND) {
    std::uint8_t a1;
    std::uint16_t pc;
    itr->GetOperand(&a1,&pc);
    ItrNext* comparison = ItrNext::New(graph_,StackGet(a1),NewIRInfo(itr->bytecode_location()),region());

    StackSet(a1,comparison);
  } else {
    lava_debug(NORMAL,lava_verify(itr->opcode() == BC_FEVREND););
  }
}

GraphBuilder::StopReason
GraphBuilder::PeelOSRLoop( BytecodeIterator* itr ) {
  bool is_osr = true;

  UnconditionalJumpList temp_break;
  // check whether we have any parental loop , if so we peel everyone
  // until we hit the end
  do {

    if(is_osr) {
      // build the OSR loop
      if(BuildOSRLoop(itr) == STOP_BAILOUT) return STOP_BAILOUT;

      is_osr = false;
    } else {

      // rebuild the loop
      {
        StopReason reason = BuildLoop(itr);
        if(reason == STOP_BAILOUT) return STOP_BAILOUT;
      }

      // now we can link the peeled break part to here
      PatchUnconditionalJump(&temp_break,region(),itr->bytecode_location());
    }

    if(func_info().HasLoop()) {
      // now start to peel the parental loop's rest instructions/bytecodes
      // the input iterator should sits right after the loop end bytecode
      // of the nested loop
      while(itr->HasNext()) {
        if(IsLoopEndBytecode(itr->opcode())) {
          break;
        }
        auto opcode = itr->opcode();
        if(BuildBytecode(itr) == STOP_BAILOUT) return STOP_BAILOUT;

        if(IsBlockJumpBytecode(opcode)) {
          // skip util we hit a loop end bytecode
          lava_verify( itr->SkipTo(
              []( BytecodeIterator* itr ) {
                return !IsLoopEndBytecode(itr->opcode());
              }
          ));
          break;
        }
      }

      // now we should end up with the loop end bytecode and this bytecode will
      // be ignored entirely since we will rewind the iterator back to the very
      // first instruction of the parental loop
      lava_debug(NORMAL,lava_verify(IsLoopEndBytecode(itr->opcode())););

      // setup osr-loop's initial condition
      SetupOSRLoopCondition(itr);

      // skip the last loop end bytecode
      itr->Move();

      // patch continue region inside of the peel part
      if(!func_info().current_loop().pending_continue.empty()) {
        // create a new region lazily
        Region* r = Region::New(graph_,region());

        PatchUnconditionalJump(&(func_info().current_loop().pending_continue),
                               r,
                               itr->bytecode_location());
        set_region(r);
      }

      // save peeled part's all break
      temp_break.swap(func_info().current_loop().pending_break);

      // now we rewind the iterator to the start of this loop and regenerate everything
      // again as natural fallthrough
      //
      // The start instruction for current loop doesn't include BC_FSTART/FEVRSTART/FESTART
      // so we need to backward by 1
      itr->BranchTo( func_info().current_loop_header()->start - 1 );

      // leave the current loop
      func_info().LeaveLoop();
    } else {
      break;
    }
  } while(true);

  return STOP_SUCCESS;
}

GraphBuilder::StopReason
GraphBuilder::BuildOSRStart( const Handle<Prototype>& entry ,  const std::uint32_t* pc ,
                                                               Graph* graph ) {
  graph_ = graph;
  zone_  = graph->zone();
  // 1. create OSRStart node which is the entry of OSR compilation
  OSRStart* start = OSRStart::New(graph);
  OSREnd*   end   = NULL;
  // next region node connect back to the OSRStart
  Region* header = Region::New(graph,start);
  // setup the fail node which accepts guard bailout
  Fail* fail = Fail::New(graph);
  Success* succ = Success::New(graph);
  {
    // set up the value stack/expression stack
    Environment root_env(graph);
    BackupEnvironment backup(&root_env,this);
    // set up the OSR scope
    OSRScope scope(this,entry,header,pc);
    // set up OSR local variable
    BuildOSRLocalVariable();
    // craft a bytecode iterator *starts* at the OSR instruction entry
    // which should be a loop start instruction like FESTART,FSTART,FEVRSTART
    const std::uint32_t* code_buffer   = entry->code_buffer();
    const std::size_t code_buffer_size = entry->code_buffer_size();
    lava_debug(NORMAL,lava_verify(pc >= code_buffer););
    BytecodeIterator itr(code_buffer,code_buffer_size);
    itr.BranchTo(pc);
    lava_debug(NORMAL,lava_verify(itr.HasNext()););
    // peel all nested loop until hit the outermost one
    if(PeelOSRLoop(&itr) == STOP_BAILOUT) return STOP_BAILOUT;
    // create trap for current region since once we abort from the loop we should
    // fallback to interpreter
    {
      Trap* trap = Trap::New(graph_,BuildCheckpoint(itr.bytecode_location()),region());
      fail->AddBackwardEdge(trap);
    }
    // link each return node back to the success node
    for( auto & e : func_info().return_list ) {
      succ->AddBackwardEdge(e);
    }
    // lastly create the end node for the osr graph
    end = OSREnd::New(graph_,succ,fail);
  }
  // initialize the graph via OSR compilation
  graph->Initialize(start,end);
  return STOP_SUCCESS;
}

bool GraphBuilder::BuildOSR( const Handle<Prototype>& entry , const std::uint32_t* osr_start ,
                                                              Graph* graph ) {
  return BuildOSRStart(entry,osr_start,graph) == STOP_SUCCESS;
}


} // namespace hir
} // namespace cbase
} // namespace lavascript
