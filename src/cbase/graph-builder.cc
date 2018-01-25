#include "graph-builder.h"

#include "src/interpreter/bytecode.h"
#include "src/interpreter/bytecode-iterator.h"

#include "src/cbase/optimization/expression-simplification.h"

#include <vector>
#include <set>
#include <map>

namespace lavascript {
namespace cbase {
namespace hir    {
using namespace ::lavascript::interpreter;

/* -------------------------------------------------------------
 *
 * RAII objects to handle different type of scopes when iterate
 * different bytecode along the way
 *
 * -----------------------------------------------------------*/
class GraphBuilder::OSRScope {
 public:
  OSRScope( GraphBuilder* gb , const Handle<Prototype>& proto ,
                               ControlFlow* region ,
                               const std::uint32_t* osr_start ):
    gb_(gb) ,
    old_upvalue_(gb->upvalue_) {

    FuncInfo temp(proto,region,osr_start); // initialize a FuncInfo as OSR entry

    // get the loop header information and recursively register all its needed
    // loop info object inside of the FuncInfo object
    auto loop_header = temp.bc_analyze.LookUpLoopHeader(osr_start);
    lava_debug(NORMAL,lava_verify(loop_header););

    /**
     * We need to iterate the loop one by one from the top most loop
     * inside of the nested loop cluster so we need to use a queue to
     * help us
     */
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

    gb->graph_->AddPrototypeInfo(proto,0);
    gb->func_info_.push_back(FuncInfo(std::move(temp)));
    gb->stack_->resize(interpreter::kRegisterSize);

    // populate upvalue array for this function
    {
      FuncInfo &ctx = gb->func_info_.back();
      for( std::size_t i = 0 ; i < ctx.upvalue.size(); ++i ) {
        ctx.upvalue[i] = UVal::New(gb->graph_,static_cast<std::uint8_t>(i));
      }
      gb->upvalue_ = &(ctx.upvalue);
    }
  }

  ~OSRScope() {
    gb_->func_info_.pop_back();
    gb_->upvalue_ = old_upvalue_;
  }
 private:
  GraphBuilder* gb_;
  ValueStack* old_upvalue_;
};

class GraphBuilder::FuncScope {
 public:
  FuncScope( GraphBuilder* gb , const Handle<Prototype>& proto ,
                                ControlFlow* region ,
                                std::uint32_t base ):
    gb_(gb),
    old_upvalue_(gb->upvalue_) {

    gb->graph_->AddPrototypeInfo(proto,base);
    gb->func_info_.push_back(FuncInfo(proto,region,base));
    gb->stack_->resize(base+interpreter::kRegisterSize);

    if(gb->func_info_.size() == 1) {
      /**
       * Initialize function argument for entry function. When we hit
       * inline frame, we dont need to populate its argument since they
       * will be taken care of by the graph builder
       */
      auto arg_size = proto->argument_size();
      for( std::size_t i = 0 ; i < arg_size ; ++i ) {
        gb->stack_->at(i) = Arg::New(gb->graph_,static_cast<std::uint32_t>(i));
      }
    }

    // populate upvalue array for this function
    {
      FuncInfo &ctx = gb->func_info_.back();
      for( std::size_t i = 0 ; i < ctx.upvalue.size(); ++i ) {
        ctx.upvalue[i] = UVal::New(gb->graph_,static_cast<std::uint8_t>(i));
      }
      gb->upvalue_ = &(ctx.upvalue);
    }
  }

  ~FuncScope() {
    gb_->func_info_.pop_back();
    gb_->upvalue_ = old_upvalue_;
  }

 private:
  GraphBuilder* gb_;
  ValueStack* old_upvalue_;
};

class GraphBuilder::LoopScope {
 public:
  LoopScope( GraphBuilder* gb , const std::uint32_t* pc ) : gb_(gb) {
    gb->func_info().EnterLoop(pc);
  }

  ~LoopScope() { gb_->func_info().LeaveLoop(); }
 private:
  GraphBuilder* gb_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(LoopScope)
};

struct GraphBuilder::VMState {
  ValueStack stack;
  ValueStack upvalue;
};

class GraphBuilder::BackupState {
 public:
  BackupState( VMState* state , GraphBuilder* gb ):
    old_stack_(gb->stack_),
    old_upvalue_(gb->upvalue_),
    gb_(gb),
    has_upvalue_(true)
  {
    if(gb->stack_) state->stack = *gb->stack_;
    if(gb->upvalue_) state->upvalue = *gb->upvalue_;

    gb->stack_ = &(state->stack);
    gb->upvalue_ = &(state->upvalue);
  }

  BackupState( ValueStack* stack , GraphBuilder* gb ):
    old_stack_(gb->stack_),
    old_upvalue_(NULL),
    gb_(gb),
    has_upvalue_(false)
  {
    if(gb->stack_) *stack = *gb->stack_;
    gb->stack_ = stack;
  }

  ~BackupState() {
    gb_->stack_ = old_stack_;
    if(has_upvalue_) gb_->upvalue_ = old_upvalue_;
  }

 private:
  ValueStack* old_stack_;
  ValueStack* old_upvalue_;
  GraphBuilder* gb_;
  bool has_upvalue_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(BackupState);
};

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

Expr* GraphBuilder::NewSSO( std::uint8_t ref , const BytecodeLocation& pc ) {
  const SSO& sso = *(func_info().prototype->GetSSO(ref)->sso);
  return SString::New(graph_,sso,NewIRInfo(pc));
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

Expr* GraphBuilder::NewUnary  ( Expr* node , Unary::Operator op ,
                                             const BytecodeLocation& pc ) {
  // try constant folding
  auto new_node = SimplifyUnary(graph_,op,node,[this,pc]() {
      return NewIRInfo(pc);
  });
  if(new_node) return new_node;

  auto checkpoint = BuildCheckpoint(pc);
  auto unary = Unary::New(graph_,node,op,NewIRInfo(pc));
  unary->set_checkpoint(checkpoint);
  return unary;
}

Expr* GraphBuilder::NewBinary  ( Expr* lhs , Expr* rhs , Binary::Operator op ,
                                                         const BytecodeLocation& pc ) {
  auto new_node = SimplifyBinary(graph_,op,lhs,rhs,[this,pc]() {
      return NewIRInfo(pc);
  });
  if(new_node) return new_node;

  auto checkpoint = BuildCheckpoint(pc);
  auto binary = Binary::New(graph_,lhs,rhs,op,NewIRInfo(pc));
  binary->set_checkpoint(checkpoint);
  return binary;
}

Expr* GraphBuilder::NewTernary ( Expr* cond , Expr* lhs , Expr* rhs,
                                                          const BytecodeLocation& pc ) {
  auto new_node = SimplifyTernary(graph_,cond,lhs,rhs,[this,pc]() {
      return NewIRInfo(pc);
  });
  if(new_node) return new_node;

  auto checkpoint = BuildCheckpoint(pc);
  auto ternary = Ternary::New(graph_,cond,lhs,rhs,NewIRInfo(pc));
  ternary->set_checkpoint(checkpoint);
  return ternary;
}

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

  // intrinsic call doesn't have checkpoint since it should not be bailout
  return node;
}

Expr* GraphBuilder::FoldObjectSet( IRObject* object , const zone::String& key ,
                                                      Expr* value,
                                                      const BytecodeLocation& pc ) {
  auto itr  = object->operand_list()->FindIf(
      [key]( const OperandList::ConstForwardIterator& itr ) {
        auto v = itr.value()->AsIRObjectKV();
        return (v->key()->IsString() && v->key()->AsZoneString() == key);
      }
    );
  if(itr.HasNext()) {
    auto ir_info = NewIRInfo(pc);
    auto new_obj = IRObject::New(graph_,object->Size(),ir_info);

    for( auto i(object->operand_list()->GetForwardIterator());
         i.HasNext(); i.Move() ) {
      auto kv = i.value()->AsIRObjectKV();
      if(kv == itr.value()) {
        new_obj->Add(kv->key(),value);
      } else {
        new_obj->AddOperand(kv);
      }
    }
    return new_obj;
  }
  return NULL;
}

Expr* GraphBuilder::FoldObjectGet( IRObject* object , const zone::String& key ,
                                                      const BytecodeLocation& pc ) {
  auto itr = object->operand_list()->FindIf(
      [key]( const OperandList::ConstForwardIterator& itr ) {
        auto v = itr.value()->AsIRObjectKV();
        return (v->key()->IsString() && v->key()->AsZoneString() == key);
      }
    );

  if(itr.HasNext()) {
    return itr.value()->AsIRObjectKV()->value(); // forward the value
  }
  return NULL;
}

Expr* GraphBuilder::NewPSet( Expr* object , Expr* key , Expr* value ,
                                                        const BytecodeLocation& pc ) {
  // try to fold the object if object is a literal
  if(object->IsIRObject()) {
    auto kstr = key->AsZoneString();
    auto obj  = object->AsIRObject();
    auto v    = FoldObjectSet(obj,kstr,value,pc);
    if(v) return v;
  }

  auto ir_info = NewIRInfo(pc);
  return PSet::New(graph_,object,key,value,ir_info,region());
}

Expr* GraphBuilder::NewPGet( Expr* object , Expr* key , const BytecodeLocation& pc ) {
  // here we *do not* do any folding operations and let the later on pass handle it
  // and we just simply do a pget node test plus some guard if needed
  if(object->IsIRObject()) {
    auto kstr= key->AsZoneString();
    auto obj = object->AsIRObject();
    auto v   = FoldObjectGet(obj,kstr,pc);
    if(v) return v;
  }

  // when we reach here we needs to generate guard
  auto ir_info = NewIRInfo(pc);
  return PGet::New(graph_,object,key,ir_info,region());
}

Expr* GraphBuilder::NewISet( Expr* object, Expr* index, Expr* value,
                                                        const BytecodeLocation& pc ) {
  if(object->IsIRList() && index->IsFloat64()) {
    auto iidx = static_cast<std::uint32_t>(index->AsFloat64()->value());
    auto list = object->AsIRList();
    if(iidx < list->Size()) {
      auto ir_info = NewIRInfo(pc);
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
  } else if(object->IsIRObject() && index->IsString()) {
    auto key = index->AsZoneString();
    auto obj = object->AsIRObject();
    auto v   = FoldObjectSet(obj,key,value,pc);
    if(v) return v;
  }

  auto ir_info = NewIRInfo(pc);
  return ISet::New(graph_,object,index,value,ir_info,region());
}

Expr* GraphBuilder::NewIGet( Expr* object, Expr* index, const BytecodeLocation& pc ) {
  if(object->IsIRList() && index->IsFloat64()) {
    auto iidx = static_cast<std::uint32_t>(index->AsFloat64()->value());
    auto list = object->AsIRList();
    if(iidx < list->Size()) {
      return list->operand_list()->Index(iidx);
    }
  } else if(object->IsIRObject() && index->IsString()) {
    auto key = index->AsZoneString();
    auto obj = object->AsIRObject();
    auto v   = FoldObjectGet(obj,key,pc);
    if(v) return v;
  }

  auto ir_info = NewIRInfo(pc);
  return IGet::New(graph_,object,index,ir_info,region());
}

IRInfo* GraphBuilder::NewIRInfo( const BytecodeLocation& pc ) {
  IRInfo* ret;
  {
    void* mem = graph_->zone()->Malloc(sizeof(IRInfo));
    ret = ConstructFromBuffer<IRInfo> (mem,method_index(),pc);
  }
  return ret;
}

Checkpoint* GraphBuilder::BuildCheckpoint( const BytecodeLocation& pc ) {
  auto cp = Checkpoint::New(graph_);

  // 1. generate stack register expression states
  {
    // get the register offset to decide offset for all the temporary register
    const std::uint32_t* pc_start = func_info().prototype->code_buffer();
    std::uint32_t diff = pc.address() - pc_start;
    std::uint8_t offset = func_info().prototype->GetRegOffset(diff);
    const std::uint32_t stack_end = func_info().base + offset;

    for( std::uint32_t i = 0 ; i < stack_end ; ++i ) {
      auto node = stack_->at(i);
      if(node) cp->AddStackSlot(node,i);
    }
  }

  // 2. generate upvalue states
  {
    std::uint8_t index = 0;

    for( auto & e : *upvalue_ ) {
      cp->AddUValSlot(e,index);
      ++index;
    }
  }
  return cp;
}

void GraphBuilder::GeneratePhi( ValueStack* dest , const ValueStack& lhs ,
                                                   const ValueStack& rhs ,
                                                   ControlFlow* region ,
                                                   const interpreter::BytecodeLocation& pc ) {
  lava_debug(NORMAL,lava_verify(lhs.size() == rhs.size()););

  for( std::size_t i = 0 ; i < lhs.size() ; ++i ) {
    Expr* l = lhs[i];
    Expr* r = rhs[i];
    /**
     * if one of lhs and rhs is NULL, it basically means some lexical scope bounded
     * variable is mutated and obviously not variable that needs a PHI, so we just
     * need to skip these type of variable entirely
     */
    if(l && r) {
      if(l != r)
        dest->at(i) = Phi::New(graph_,l,r,region,NewIRInfo(pc));
      else
        dest->at(i) = l;
    }
  }
}

void GraphBuilder::InsertIfPhi( const ValueStack& false_stack ,
                                const ValueStack& true_stack  ,
                                const ValueStack& false_uval  ,
                                const ValueStack& true_uval   ,
                                ControlFlow* region ,
                                const BytecodeLocation& pc ) {
  GeneratePhi(stack_,false_stack,true_stack,region,pc);
  GeneratePhi(upvalue_,false_uval,true_uval,region,pc);
}

void GraphBuilder::InsertUnconditionalJumpPhi( const ValueStack& stk , ControlFlow* region ,
                                                                       const BytecodeLocation& pc ) {
  for( std::size_t i = 0 ; i < stack_->size(); ++i ) {
    Expr* lhs = stack_->at(i);
    if(i == stk.size()) break;

    Expr* rhs = stk[i];

    if(lhs && rhs) {
      /**
       * We try to reuse the PHI node at current region if possible.
       * Our IR's PHI node can accept multiple inputs not just 2.
       * Nested PHI node are essentially the same as multiple-input PHI
       * but mutiple-input PHI is just more clean
       */
      if(lhs->IsPhi() && lhs->AsPhi()->region() == region) {
        lhs->AsPhi()->AddOperand( rhs );
        lava_debug(NORMAL,
            lava_verify(lhs->AsPhi()->operand_list()->size() == region->backward_edge()->size()););
        stack_->at(i) = lhs;
      } else if(rhs->IsPhi() && rhs->AsPhi()->region() == region) {
        rhs->AsPhi()->AddOperand( lhs );
        lava_debug(NORMAL,
            lava_verify(rhs->AsPhi()->operand_list()->size() == region->backward_edge()->size()););
        stack_->at(i) = rhs;
      } else {
        stack_->at(i) = Phi::New(graph_,lhs,rhs,region,NewIRInfo(pc));
        lava_debug(NORMAL,lava_verify(region->backward_edge()->size() == 2););
      }
    }
  }
}

void GraphBuilder::PatchUnconditionalJump( UnconditionalJumpList* jumps ,
                                           ControlFlow* region ,
                                           const BytecodeLocation& pc ) {
  for( auto& e : *jumps ) {
    lava_debug(NORMAL,lava_verify(e.pc == pc.address()););
    lava_verify(e.node->TrySetTarget(pc.address(),region));
    region->AddBackwardEdge(e.node);
    InsertUnconditionalJumpPhi(e.stack,region,pc);
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

  lava_debug(NORMAL,lava_verify(StackGet(rhs)););

  if(op_and)
    StackSet(rhs,NewBinary(lhs_expr,StackGet(rhs),Binary::AND,itr->bytecode_location()));
  else
    StackSet(rhs,NewBinary(lhs_expr,StackGet(rhs),Binary::OR,itr->bytecode_location()));

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

  StackSet(result, NewTernary(StackGet(cond),lhs,rhs,itr->bytecode_location()));
  return STOP_SUCCESS;
}

GraphBuilder::StopReason GraphBuilder::GotoIfEnd( BytecodeIterator* itr,
                                                  const std::uint32_t* pc ) {
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

GraphBuilder::StopReason GraphBuilder::BuildIfBlock( BytecodeIterator* itr ,
                                                     const std::uint32_t* pc ) {
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

  VMState true_stack;

  std::uint16_t final_cursor;
  bool have_false_branch;

  // 1. Build code inside of the *true* branch and it will also help us to
  //    identify whether we have dangling elif/else branch
  {
    itr->Move();              // skip the BC_JMPF
    BackupState backup(&true_stack,this); // back up the old stack and use new stack
    set_region(true_region);  // switch to true region

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
    itr->BranchTo(offset); // go to the false branch

    if(BuildIfBlock(itr,itr->OffsetAt(final_cursor)) == STOP_BAILOUT)
      return STOP_BAILOUT;
    lhs = region();
  } else {
    final_cursor = offset; // we don't have a else/elif branch
    lhs = false_region;
  }

  // 3. set the merge backward edge
  merge->AddBackwardEdge(lhs);
  merge->AddBackwardEdge(rhs);

  itr->BranchTo(final_cursor);
  set_region(merge);

  // 3. handle PHI node
  InsertIfPhi(*stack_ , true_stack.stack , *upvalue_ , true_stack.upvalue ,
                                                       merge ,
                                                       itr->bytecode_location());
  return STOP_SUCCESS;
}

GraphBuilder::StopReason
GraphBuilder::GotoLoopEnd( BytecodeIterator* itr ) {
  lava_verify(itr->SkipTo(
        []( BytecodeIterator* itr ) {
          return !(itr->opcode() == BC_FEEND ||
                   itr->opcode() == BC_FEND1 ||
                   itr->opcode() == BC_FEND2 ||
                   itr->opcode() == BC_FEVREND);
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
  const std::size_t len = func_info().current_loop_header()->phi.size();
  for( std::size_t i = 0 ; i < len ; ++i ) {
    if(func_info().current_loop_header()->phi[i]) {
      Expr* old = StackGet(static_cast<std::uint32_t>(i));
      lava_debug(NORMAL,lava_verify(old););

      Phi* phi = Phi::New(graph_,region(),NewIRInfo(pc));
      phi->AddOperand(old);

      StackSet(static_cast<std::uint32_t>(i),phi);

      // insert the phi node into the current loop's pending phi list
      func_info().current_loop().AddPhi(static_cast<std::uint8_t>(i),phi);
    }
  }
}

void GraphBuilder::PatchLoopPhi() {
  for( auto & e: func_info().current_loop().phi_list ) {
    Phi*  phi  = e.phi;
    Expr* node = StackGet(e.reg);

    lava_debug(NORMAL,lava_verify(phi != node););
    phi->AddOperand(node);
  }
  func_info().current_loop().phi_list.clear();
}

Expr* GraphBuilder::BuildLoopEndCondition( BytecodeIterator* itr , ControlFlow* body ) {
  // now we should stop at the FEND1/FEND2/FEEND instruction
  if(itr->opcode() == BC_FEND1) {
    std::uint8_t a1,a2,a3; std::uint32_t a4;
    itr->GetOperand(&a1,&a2,&a3,&a4);
    return NewBinary(StackGet(a1),StackGet(a2),Binary::LT,itr->bytecode_location());
  } else if(itr->opcode() == BC_FEND2) {
    std::uint8_t a1,a2,a3; std::uint32_t a4;
    itr->GetOperand(&a1,&a2,&a3,&a4);

    Expr* induct = StackGet(a1);
    lava_debug(NORMAL,lava_verify(induct->IsPhi()););

    // the addition node will use the PHI node as its left hand side
    auto addition = NewBinary(induct,StackGet(a3),Binary::ADD,
                                                  itr->bytecode_location());
    // store the PHI node back to the slot
    StackSet(a1,addition);

    // construct comparison node
    return NewBinary(addition,StackGet(a2),Binary::LT,itr->bytecode_location());
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
  IfTrue*     if_true     = IfTrue::New(graph_);
  IfFalse*    if_false    = IfFalse::New(graph_);
  Region*     after       = Region::New(graph_);

  BytecodeLocation cont_pc;
  BytecodeLocation brk_pc ;

  {
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
    body->AddBackwardEdge(if_true);
    if_true->AddBackwardEdge(exit);

    // Only link the if_false edge back to loop header when it is actually a
    // loop header type. During OSR compilation , since we don't have a real
    // loop header , so we don't need to link it back
    if(loop_header->IsLoopHeader())
      if_false->AddBackwardEdge(loop_header);

    if_false->AddBackwardEdge(exit);
    after->AddBackwardEdge(if_false);

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
        auto node = NewBinary(NewNumber(a1),
                              StackGet(a1),
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
        auto node = NewBinary(StackGet(a1),
                              NewNumber(a2),
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
        auto node = NewBinary(StackGet(a1),
                              StackGet(a2),
                              Binary::BytecodeToOperator(itr->opcode()),
                              itr->bytecode_location());
        StackSet(dest,node);
      }
      break;
    case BC_EQSV: case BC_NESV:
      {
        std::uint8_t dest, a1, a2;
        itr->GetOperand(&dest,&a1,&a2);

        auto node = NewBinary(NewString(a1), StackGet(a2),
                                             Binary::BytecodeToOperator(itr->opcode()),
                                             itr->bytecode_location());
        StackSet(dest,node);
      }
      break;
    case BC_EQVS: case BC_NEVS:
      {
        std::uint8_t dest, a1, a2;
        itr->GetOperand(&dest,&a1,&a2);
        auto node = NewBinary(StackGet(a1),NewString(a2),
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
        auto node = NewUnary(StackGet(src), Unary::BytecodeToOperator(itr->opcode()) ,
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
        itr->GetOperand(&a1,&a2,&a3);
        IRObject* obj = IRObject::New(graph_,1,NewIRInfo(itr->bytecode_location()));
        obj->Add(StackGet(a2),StackGet(a3));
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
        obj->Add(StackGet(a2),StackGet(a3));
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
        Expr* key = (itr->opcode() == BC_PROPGET ? NewString(a3):
                                                   NewSSO   (a3));
        StackSet(a1,NewPGet(StackGet(a2),key,itr->bytecode_location()));
      }
      break;
    case BC_PROPSET: case BC_PROPSETSSO:
      {
        std::uint8_t a1,a2,a3;
        itr->GetOperand(&a1,&a2,&a3);
        Expr* key = (itr->opcode() == BC_PROPSET ? NewString(a2):
                                                   NewSSO   (a2));
        StackSet(a1,NewPSet(StackGet(a1),key,StackGet(a3),itr->bytecode_location()));
      }
      break;
    case BC_IDXGET: case BC_IDXGETI:
      {
        std::uint8_t a1,a2,a3;
        itr->GetOperand(&a1,&a2,&a3);
        Expr* key = (itr->opcode() == BC_IDXGET ? StackGet(a3) : NewConstNumber(a3));
        StackSet(a1,NewIGet(StackGet(a2),key,itr->bytecode_location()));
      }
      break;
    case BC_IDXSET: case BC_IDXSETI:
      {
        std::uint8_t a1,a2,a3;
        itr->GetOperand(&a1,&a2,&a3);
        Expr* key = (itr->opcode() == BC_IDXSET ? StackGet(a2) :
                                                  NewConstNumber(a2));

        StackSet(a1,NewISet(StackGet(a1),key,StackGet(a3),itr->bytecode_location()));
      }
      break;

    case BC_UVGET:
      {
        std::uint8_t a1,a2;
        itr->GetOperand(&a1,&a2);
        auto uval = func_info().upvalue[a2];
        lava_debug(NORMAL,lava_verify(uval););
        StackSet(a1,uval);
      }
      break;
    case BC_UVSET:
      {
        std::uint8_t a1,a2;
        itr->GetOperand(&a1,&a2);
        auto uset =
          USet::New(graph_,method_index(),StackGet(a2),NewIRInfo(itr->bytecode_location()),region());
        func_info().upvalue[a1] = uset;
      }
      break;
    case BC_GGET: case BC_GGETSSO:
      {
        std::uint8_t a1,a2;
        itr->GetOperand(&a1,&a2);
        Expr* key = (itr->opcode() == BC_GGET ? NewString(a2) :
                                                NewSSO   (a2));
        StackSet(a1,GGet::New(graph_,key,NewIRInfo(itr->bytecode_location()),region()));
      }
      break;
    case BC_GSET: case BC_GSETSSO:
      {
        std::uint8_t a1,a2;
        itr->GetOperand(&a1,&a2);
        Expr* key = (itr->opcode() == BC_GSET ? NewString(a1) :
                                                NewSSO   (a1));
        GSet::New(graph_,key,StackGet(a2),NewIRInfo(itr->bytecode_location()),region());
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
    case BC_FSTART:
    case BC_FESTART:
    case BC_FEVRSTART:
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
          func_info().current_loop().AddBreak(jump,itr->OffsetAt(pc),*stack_);
        else
          func_info().current_loop().AddContinue(jump,itr->OffsetAt(pc),*stack_);
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
    // set up the evaluation stack all evaluation stack are triansient so I
    // just put it on to the stack
    VMState stack;
    BackupState backup(&stack,this);

    FuncScope scope(this,entry,region,0);
    BytecodeIterator itr(entry->GetBytecodeIterator());

    // set the current region
    set_region(region);

    // start to execute the build basic block
    if(BuildBasicBlock(&itr) == STOP_BAILOUT)
      return false;

    {
      Phi* return_value = Phi::New(graph_,end,NULL);
      succ->set_return_value(return_value);

      for( auto &e : func_info().return_list ) {
        return_value->AddOperand(e->AsReturn()->value());
        succ->AddBackwardEdge(e);
      }

      end = End::New(graph_,succ,fail);
    }
  }

  graph->Initialize(start,end);
  return true;
}

void GraphBuilder::BuildOSRLocalVariable() {
  auto loop_header = func_info().bc_analyze.LookUpLoopHeader(func_info().osr_start);
  lava_debug(NORMAL,lava_verify(loop_header););
  for( BytecodeAnalyze::LocalVariableIterator itr(loop_header->bb,func_info().bc_analyze);
       itr.HasNext() ; itr.Move() ) {
    lava_debug(NORMAL,lava_verify(!stack_->at(itr.value())););
    stack_->at(itr.value()) = OSRLoad::New(graph_,itr.value());
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

    auto comparison = NewBinary(StackGet(a1),
                                StackGet(a2),
                                Binary::LT ,
                                itr->bytecode_location());

    StackSet(interpreter::kAccRegisterIndex,comparison);
  } else if(itr->opcode() == BC_FEND2) {
    std::uint8_t a1,a2,a3; std::uint32_t a4;
    itr->GetOperand(&a1,&a2,&a3,&a4);

    // the addition node will use the PHI node as its left hand side
    auto addition = NewBinary(StackGet(a1),StackGet(a3),Binary::ADD,
                                                        itr->bytecode_location());
    StackSet(a1,addition);

    auto comparison = NewBinary(addition,StackGet(a2),Binary::LT,
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

  Fail* fail = Fail::New(graph);
  Success* succ = Success::New(graph);

  {
    // set up the value stack/expression stack
    ValueStack stack;
    BackupState backup_stack(&stack,this);

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
      Trap* trap = Trap::New(graph_,region());
      succ->AddBackwardEdge(trap);
    }

    // create trap for each return block
    for( auto & e : func_info().return_list ) {
      Trap* trap = Trap::New(graph_,e);
      succ->AddBackwardEdge(trap);
    }

    end = OSREnd::New(graph_,succ,fail);
  }

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
