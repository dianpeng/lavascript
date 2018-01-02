#include "graph-builder.h"

#include "src/interpreter/bytecode.h"
#include "src/interpreter/bytecode-iterator.h"

#include <vector>
#include <set>
#include <map>

namespace lavascript {
namespace cbase {
namespace ir    {
using namespace ::lavascript::interpreter;

/* -------------------------------------------------------------
 * RAII objects to handle different type of scopes when iterate
 * different bytecode along the way
 * -----------------------------------------------------------*/

class GraphBuilder::FuncScope {
 public:
  FuncScope( GraphBuilder* gb , const Handle<Closure>& cls ,
                                ControlFlow* region ,
                                std::uint32_t base ): gb_(gb) {
    gb->func_info_.push_back(FuncInfo(cls,region,base));
    gb->stack_->resize(base+interpreter::kRegisterSize);
  }

  ~FuncScope() { gb_->func_info_.pop_back(); }
 private:
  GraphBuilder* gb_;
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

class GraphBuilder::BackupStack {
 public:
  BackupStack( ValueStack* new_stack , GraphBuilder* gb ):
    old_stack_(gb->stack_),
    gb_(gb)
  {
    *new_stack = *gb->stack_; // do a deep copy
    gb->stack_ = new_stack;
  }

  ~BackupStack() { gb_->stack_ = old_stack_; }
 private:
  ValueStack* old_stack_;
  GraphBuilder* gb_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(BackupStack);
};

Expr* GraphBuilder::NewConstNumber( std::int32_t ivalue , const BytecodeLocation& pc ) {
  return Int32::New(graph_,ivalue,NewIRInfo(pc));
}

Expr* GraphBuilder::NewConstNumber( std::int32_t ivalue ) {
  return Int32::New(graph_,ivalue,NULL);
}

Expr* GraphBuilder::NewNumber( std::uint8_t ref , IRInfo* info ) {
  double real = func_info().prototype->GetReal(ref);
  // now try to narrow the real accordingly to int32 int64 or just float64
  double i32_min = static_cast<double>(std::numeric_limits<std::int32_t>::min());
  double i32_max = static_cast<double>(std::numeric_limits<std::int32_t>::max());

  if(i32_min < real && i32_max > real) {
    std::int32_t i32;
    if(NarrowReal(real,&i32)) {
      return Int32::New(graph_,i32,info);
    }
  }

  // try int64 narrow
  {
    std::int64_t i64;
    if(NarrowReal(real,&i64)) {
      return Int64::New(graph_,i64,info);
    }
  }

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

IRInfo* GraphBuilder::NewIRInfo( const BytecodeLocation& pc ) {
  IRInfo* ret;
  {
    void* mem = graph_->zone()->Malloc(sizeof(IRInfo));
    ret = ConstructFromBuffer<IRInfo> (mem,static_cast<std::uint32_t>(func_info_.size()),pc);
  }
  return ret;
}

void GraphBuilder::InsertIfPhi( ValueStack* dest , const ValueStack& false_stack ,
                                                   const ValueStack& true_stack  ,
                                                   ControlFlow* region ,
                                                   const BytecodeLocation& pc ) {

  lava_debug(NORMAL,lava_verify(false_stack.size() == true_stack.size()););

  for( std::size_t i = 0 ; i < false_stack.size() ; ++i ) {
    Expr* lhs = false_stack[i];
    Expr* rhs = true_stack [i];
    /**
     * if one of lhs and rhs is NULL, it basically means some lexical scope bounded
     * variable is mutated and obviously not variable that needs a PHI, so we just
     * need to skip these type of variable entirely
     */
    if(lhs && rhs) {
      if(lhs != rhs)
        (*dest)[i] = Phi::New(graph_,lhs, rhs, region , NewIRInfo(pc));
      else
        (*dest)[i] = lhs;
    }
  }
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
  std::uint8_t reg; std::uint16_t offset;
  itr->GetOperand(&reg,&offset);

  // where we should end for the other part of the logical cominator
  const std::uint32_t* end_pc = itr->OffsetAt(offset);

  // save the fallthrough value for PHI node later on
  Expr* lhs = StackGet(reg);

  lava_debug(NORMAL,StackReset(reg););

  { // evaluate the rhs
    itr->Move();
    StopReason reason = BuildBasicBlock(itr,end_pc);
    lava_verify(reason == STOP_END);
  }

  lava_debug(NORMAL,lava_verify(StackGet(reg)););

  if(op_and)
    StackSet(reg,Binary::New(graph_,lhs,StackGet(reg),Binary::AND,NewIRInfo(itr->bytecode_location())));
  else
    StackSet(reg,Binary::New(graph_,lhs,StackGet(reg),Binary::OR,NewIRInfo(itr->bytecode_location())));

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

    for( itr->Move() ; itr->HasNext() ; ) {
      if(itr->pc() == end_pc) break;

      if(BuildBytecode(itr) == STOP_BAILOUT)
        return STOP_BAILOUT;
    }

    rhs = StackGet(result);
    lava_debug(NORMAL,lava_verify(rhs););
  }

  StackSet(result, Ternary::New(graph_,StackGet(cond),lhs,rhs,NewIRInfo(itr->bytecode_location())));
  return STOP_SUCCESS;
}

GraphBuilder::StopReason GraphBuilder::GotoIfEnd( BytecodeIterator* itr,
                                                  const std::uint32_t* pc ) {
  for( ; itr->HasNext() ; itr->Move() ) {
    if(itr->pc() == pc)          return STOP_END;
    if(itr->opcode() == BC_JMP) return STOP_JUMP;
  }
  lava_unreachF("cannot reach here since it is end of the stream %p:%p",itr->pc(),pc);
  return STOP_EOF;
}

GraphBuilder::StopReason GraphBuilder::BuildIfBlock( BytecodeIterator* itr ,
                                                     const std::uint32_t* pc ) {
  while( itr->HasNext() ) {
    // check whether we reache end of PC where we suppose to stop
    if(pc == itr->pc()) return STOP_END;

    // check whether we have a unconditional jump or not
    if(itr->opcode() == BC_JMP) return STOP_JUMP;
    switch(itr->opcode()) {
      case BC_JMP:
        return STOP_JUMP;

      case BC_CONT:
      case BC_BRK:
        if(BuildBytecode(itr) == STOP_BAILOUT)
          return STOP_BAILOUT;
        return GotoIfEnd(itr,pc);

      default:
        if(BuildBytecode(itr) == STOP_BAILOUT)
          return STOP_BAILOUT;
        break;
    }
  }
  lava_unreachF("cannot reach here since it is end of the stream %p:%p",itr->pc(),pc);
  return STOP_EOF;
}

GraphBuilder::StopReason
GraphBuilder::BuildIf( BytecodeIterator* itr ) {
  lava_debug(NORMAL,lava_verify(itr->opcode() == BC_JMPF););

  // skip the BC_JMPF
  lava_verify(itr->Move());

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

  ValueStack true_stack;

  std::uint16_t final_cursor;
  bool have_false_branch;

  // 1. Build code inside of the *true* branch and it will also help us to
  //    identify whether we have dangling elif/else branch
  {
    itr->Move();              // skip the BC_JMPF
    BackupStack(&true_stack,this); // back up the old stack and use new stack
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
  InsertIfPhi(stack_ , *stack_ , true_stack , merge , itr->bytecode_location());
  return STOP_SUCCESS;
}

/* --------------------------------------------------------------------------------------
 *
 * Loop IR building
 *
 * The Loop IR is little complicated to build , and here we have to consider OSR.
 *
 * 1) normal loop
 *   the normal loop is built based on the normal way, one thing to note is loop will
 *   be inversed during construction. The phi node in loop will be generated ahead of
 *   the loop based on the information provided by the BytecodeAnalyze since it will
 *   tell us which variable that is not bounded in loop is modified during the loop.
 *   Due to the fact the PHI node requires a operand that is not available when loop
 *   header is generated, we will need to patch each PHI node afterwards when the loop
 *   body is done. We will record pending PHI node. For break/continue they will be
 *   recorded according during the loop construction and its jump target will be correct
 *   patched once loop_exit is generated.
 *
 *
 * 2) OSR loop
 *   If this is a OSR compliation, then we will have to generate OSR related code. The
 *   OSR code generation will be *start* at OSR. Due to the fact we will be able to tell
 *   which variable are alive at the header of the LOOP, we will generate OSR IR to load
 *   all the needed value from OSR provider's buffer according to OSR ABI. Afterwards the
 *   loop generation will be like some sort of loop peeling.
 *
 *   We start at the loop where OSR jumps into and generate IR for it , if any inner loop
 *   nested it will also be generated normally. Then we start to peel some instructions
 *   from the outer loop that enclose the OSR loop , example as following:
 *
 *   for(...) { // loop A
 *     for(...) {  // loop B
 *       for(...) { // OSR entry , loop C
 *         for( ... ) { // loop D
 *         }
 *         // blabla
 *       }
 *       // blabla
 *     }
 *     // blabla
 *   }
 *
 *
 *   So we will first generate loop body for "loop C" since it is OSR entry , including
 *   loop D will be generated. Then we will generate IR for the peeling part which is
 *   whatever that is left after the loop C finish its body. And then go back to the
 *   header of loop B to generate whatever block that is before the loop C. Same style
 *   goes to the loop A. Basically for all enclosed loop we gonna do a simple peeling;
 *   for whatever that is enclosed/nested, normal way.
 *
 *   After these nested loop cluster been generated, our code generation will be done
 *   here. In our case is when we exit loop A there will be a deoptimization happened
 *   and we will fallback to the interpreter.
 *
 * ---------------------------------------------------------------------------------------*/

GraphBuilder::StopReason
GraphBuilder::BuildLoopBlock( BytecodeIterator* itr ) {
  while(itr->HasNext()) {
    switch(itr->opcode()) {
      case BC_FEEND: case BC_FEND1: case BC_FEND2: case BC_FEVREND:
        return STOP_SUCCESS;
      case BC_CONT: case BC_BRK:
        if(BuildBytecode(itr) == STOP_BAILOUT)
          return STOP_BAILOUT;

        return STOP_SUCCESS;
      default:
        if(BuildBytecode(itr) == STOP_BAILOUT)
          return STOP_BAILOUT;
        break;
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

    // BytecodeAnalyze should only tell us variable that is being modified in the loop
    lava_debug(NORMAL,lava_verify(phi != node););
    phi->AddOperand(node);
  }
  func_info().current_loop().phi_list.clear();
}

GraphBuilder::StopReason
GraphBuilder::BuildLoop( BytecodeIterator* itr ) {
  lava_debug(NORMAL,lava_verify(itr->opcode() == BC_FSTART ||
                                itr->opcode() == BC_FESTART||
                                itr->opcode() == BC_FEVRSTART););

  const std::uint32_t* after_pc;

  LoopHeader* loop_header = NULL;
  Loop*       body        = NULL;
  LoopExit*   exit        = NULL;
  IfTrue*     if_true     = IfTrue::New(graph_);
  IfFalse*    if_false    = IfFalse::New(graph_);
  Region*     after       = Region::New(graph_);

  BytecodeLocation cont_pc;
  BytecodeLocation brk_pc ;

  // 1. construct the loop's first branch. all loop here are automatically
  //    inversed
  if(itr->opcode() == BC_FSTART) {
    std::uint8_t a1; std::uint16_t a2;
    itr->GetOperand(&a1,&a2); after_pc = itr->OffsetAt(a2);
    loop_header  = LoopHeader::New(graph_,StackGet(a1),region());
  } else if(itr->opcode() == BC_FESTART) {
    std::uint8_t a1; std::uint16_t a2;
    itr->GetOperand(&a1,&a2); after_pc = itr->OffsetAt(a2);
    // create ir ItrNew which basically initialize the itr and also do
    // a test against the iterator to see whether it is workable
    IRInfo* info = NewIRInfo(itr->bytecode_location());
    ItrNew* inew = ItrNew::New(graph_,StackGet(a1),info,region());
    loop_header  = LoopHeader::New(graph_,inew,region());
  } else {
    lava_debug(NORMAL,lava_verify(itr->opcode() == BC_FEVRSTART););
    /**
     * for forever loop, we still build the structure of inverse loop, but just
     * mark the condition to be true. later pass for eliminating branch will take
     * care of this false inversed loop if
     */
    loop_header = LoopHeader::New(graph_,NewBoolean(true,itr->bytecode_location()),region());
  }

  set_region(loop_header);

  // skip the loop start bytecode
  itr->Move();

  // 2. enter into the loop's body
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
    if(itr->opcode() == BC_FEND1) {
      std::uint8_t a1,a2,a3; std::uint32_t a4;
      itr->GetOperand(&a1,&a2,&a3,&a4);

      Binary* comparison = Binary::New(
          graph_,StackGet(a1), StackGet(a2), Binary::LT, NewIRInfo(itr->bytecode_location()));

      // exit points back to the body of the loop
      lava_debug(NORMAL,lava_verify(exit == NULL););

      // create the loop exit node
      exit = LoopExit::New(graph_,comparison);

    } else if(itr->opcode() == BC_FEND2) {
      std::uint8_t a1,a2,a3; std::uint32_t a4;
      itr->GetOperand(&a1,&a2,&a3,&a4);

      // |a1| + |a3| < |a2| , here we should take care of the PHI node insertion
      Phi* loop_induction = Phi::New(graph_,body,NewIRInfo(itr->bytecode_location()));

      // left handside of the PHI are original a1
      loop_induction->AddOperand(StackGet(a1));

      // the addition node will use the PHI node as its left hand side
      Binary* addition = Binary::New(graph_,loop_induction,StackGet(a3),
                                                           Binary::ADD,
                                                           NewIRInfo(itr->bytecode_location()));
      // finish the PHI node construction
      loop_induction->AddOperand(addition);

      // store the PHI node back to the slot
      StackSet(a1,loop_induction);

      // construct comparison node
      Binary* comparison = Binary::New(graph_,addition,StackGet(a2), Binary::LT,
                                                                     NewIRInfo(itr->bytecode_location()));

      // exit points back to the body of the loop
      lava_debug(NORMAL,lava_verify(exit == NULL););

      // create the loop exit node
      exit = LoopExit::New(graph_,comparison);
    } else {
      std::uint8_t a1;
      std::uint16_t pc;
      itr->GetOperand(&a1,&pc);

      ItrNext* comparison = ItrNext::New(graph_,StackGet(a1),NewIRInfo(itr->bytecode_location()),region());

      // exit points back to the body of the loop
      lava_debug(NORMAL,lava_verify(exit == NULL););

      // create the loop exit node
      exit = LoopExit::New(graph_,comparison);
    }

    // connect each control flow node together
    exit->AddBackwardEdge(region());  // NOTES: do not link back to body directly since current
                                      //        region may changed due to new basic block creation

    body->AddBackwardEdge(loop_header);
    body->AddBackwardEdge(if_true);
    if_true->AddBackwardEdge (exit);
    if_false->AddBackwardEdge(loop_header);
    if_false->AddBackwardEdge(exit);
    after->AddBackwardEdge(if_false);

    // skip the last end instruction
    itr->Move();

    // patch all the Phi node
    PatchLoopPhi();
  }

  lava_debug(NORMAL,lava_verify(itr->pc() == after_pc););
  set_region(after);

  // break should jump here which is *after* the merge region
  brk_pc = itr->bytecode_location();

  // patch all the pending continue and break node
  PatchUnconditionalJump( &func_info().current_loop().pending_continue , exit , cont_pc );
  PatchUnconditionalJump( &func_info().current_loop().pending_break    , after, brk_pc  );

  return STOP_SUCCESS;
}

GraphBuilder::StopReason GraphBuilder::BuildBytecode( BytecodeIterator* itr ) {
  switch(itr->opcode()) {
    /* binary arithmetic/comparison */
    case BC_ADDRV: case BC_SUBRV: case BC_MULRV: case BC_DIVRV: case BC_MODRV: case BC_POWRV:
    case BC_LTRV:  case BC_LERV : case BC_GTRV : case BC_GERV : case BC_EQRV : case BC_NERV :
      {
        std::uint8_t dest , a1, a2;
        itr->GetOperand(&dest,&a1,&a2);
        Binary* node = Binary::New(graph_,NewNumber(a1),
                                          StackGet(a2),
                                          Binary::BytecodeToOperator(itr->opcode()),
                                          NewIRInfo(itr->bytecode_location()));
        StackSet(dest,node);
      }
      break;
    case BC_ADDVR: case BC_SUBVR: case BC_MULVR: case BC_DIVVR: case BC_MODVR: case BC_POWVR:
    case BC_LTVR : case BC_LEVR : case BC_GTVR : case BC_GEVR : case BC_EQVR : case BC_NEVR :
      {
        std::uint8_t dest , a1, a2;
        itr->GetOperand(&dest,&a1,&a2);
        Binary* node = Binary::New(graph_,StackGet(a1),
                                          NewNumber(a2),
                                          Binary::BytecodeToOperator(itr->opcode()),
                                          NewIRInfo(itr->bytecode_location()));
        StackSet(dest,node);
      }
      break;
    case BC_ADDVV: case BC_SUBVV: case BC_MULVV: case BC_DIVVV: case BC_MODVV: case BC_POWVV:
    case BC_LTVV : case BC_LEVV : case BC_GTVV : case BC_GEVV : case BC_EQVV : case BC_NEVV :
      {
        std::uint8_t dest, a1, a2;
        itr->GetOperand(&dest,&a1,&a2);

        Binary* node = Binary::New(graph_,StackGet(a1),
                                          StackGet(a2),
                                          Binary::BytecodeToOperator(itr->opcode()),
                                          NewIRInfo(itr->bytecode_location()));
        StackSet(dest,node);
      }
      break;
    case BC_EQSV: case BC_NESV:
      {
        std::uint8_t dest, a1, a2;
        itr->GetOperand(&dest,&a1,&a2);

        Binary* node = Binary::New(graph_,NewString(a1),
                                          StackGet(a2),
                                          Binary::BytecodeToOperator(itr->opcode()),
                                          NewIRInfo(itr->bytecode_location()));
        StackSet(dest,node);
      }
      break;
    case BC_EQVS: case BC_NEVS:
      {
        std::uint8_t dest, a1, a2;
        itr->GetOperand(&dest,&a1,&a2);

        Binary* node = Binary::New(graph_,StackGet(a1),
                                          NewString(a2),
                                          Binary::BytecodeToOperator(itr->opcode()),
                                          NewIRInfo(itr->bytecode_location()));
        StackSet(dest,node);
      }
      break;
    /* unary operation */
    case BC_NEGATE: case BC_NOT:
      {
        std::uint8_t dest, src;
        itr->GetOperand(&dest,&src);
        Unary* node = Unary::New( graph_,StackGet(src),
                                         Unary::BytecodeToOperator(itr->opcode()),
                                         NewIRInfo(itr->bytecode_location()));

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
        StackSet(a1,PGet::New(graph_,StackGet(a2),key,NewIRInfo(itr->bytecode_location()),region()));
      }
      break;
    case BC_PROPSET: case BC_PROPSETSSO:
      {
        std::uint8_t a1,a2,a3;
        itr->GetOperand(&a1,&a2,&a3);
        Expr* key = (itr->opcode() == BC_PROPSET ? NewString(a2):
                                                   NewSSO   (a2));
        PSet::New(graph_,StackGet(a1),key,StackGet(a3),NewIRInfo(itr->bytecode_location()),region());
      }
      break;
    case BC_IDXGET: case BC_IDXGETI:
      {
        std::uint8_t a1,a2,a3;
        itr->GetOperand(&a1,&a2,&a3);
        Expr* key = (itr->opcode() == BC_IDXGET ? StackGet(a3) :
                                                  NewConstNumber(a3));
        StackSet(a1,IGet::New(graph_,StackGet(a2),key,NewIRInfo(itr->bytecode_location()),region()));
      }
      break;
    case BC_IDXSET: case BC_IDXSETI:
      {
        std::uint8_t a1,a2,a3;
        itr->GetOperand(&a1,&a2,&a3);
        Expr* key = (itr->opcode() == BC_IDXGET ? StackGet(a2) :
                                                  NewConstNumber(a2));
        ISet::New(graph_,StackGet(a1),key,StackGet(a3), NewIRInfo(itr->bytecode_location()),region());
      }
      break;

    case BC_UVGET:
      {
        std::uint8_t a1,a2;
        itr->GetOperand(&a1,&a2);
        StackSet(a1,UGet::New(graph_,a2,NewIRInfo(itr->bytecode_location())));
      }
      break;
    case BC_UVSET:
      {
        std::uint8_t a1,a2;
        itr->GetOperand(&a1,&a2);
        USet::New(graph_,a1,StackGet(a2),NewIRInfo(itr->bytecode_location()));
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

    /* loop */
    case BC_FSTART:
    case BC_FEVRSTART:
      return BuildLoop(itr);

    /* iterator dereference */
    case BC_IDREF:
      lava_debug(NORMAL,lava_verify(func_info().HasLoop()););
      {
        std::uint8_t a1,a2,a3;
        itr->GetOperand(&a1,&a2,&a3);

        ItrDeref*  iref = ItrDeref::New(graph_,StackGet(a1),NewIRInfo(itr->bytecode_location()),region());
        Projection* key = Projection::New(graph_,iref,ItrDeref::PROJECTION_KEY,
            NewIRInfo(itr->bytecode_location()));
        Projection* val = Projection::New(graph_,iref,ItrDeref::PROJECTION_VAL,
            NewIRInfo(itr->bytecode_location()));

        StackSet(a2,key);
        StackSet(a3,val);
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
        Return* ret= Return::New(graph_,StackGet(interpreter::kAccRegisterIndex),region());
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

    // build this instruction
    BuildBytecode(itr);

    // check if last opcode is break or continue which is unconditional
    // jump. so we can just abort the construction of this basic block
    if(itr->opcode() == BC_BRK || itr->opcode() == BC_CONT)
      return STOP_JUMP;
  }

  return STOP_SUCCESS;
}

bool GraphBuilder::Build( const Handle<Closure>& entry , Graph* graph ) {
  graph_ = graph;

  // 1. create the start and end region
  Start* start = Start::New(graph_);
  End*   end;

  // create the first region
  Region* region = Region::New(graph_,start);
  set_region(region);


  // 2. start the basic block building
  {
    // set up the evaluation stack all evaluation stack are triansient so I
    // just put it on to the stack
    ValueStack stack;
    BackupStack(&stack,this);

    FuncScope scope(this,entry,region,0);
    Handle<Prototype> proto(entry->prototype());
    BytecodeIterator itr(proto->GetBytecodeIterator());

    if(BuildBasicBlock(&itr) == STOP_BAILOUT)
      return false;

    // NOTES:: we don't need to link natural fallthrough edge from *end* region
    //         since bytecode will *always* generate a RET instruction at last.
    //         For OSR compliation, we need to since OSR only compiles a loop
    //
    // patch all return nodes to merge back to the end
    end = End::New(graph_);
    Phi* return_value = Phi::New(graph_,end,NULL);
    end->set_return_value(return_value);

    for( auto &e : func_info().return_list ) {
      return_value->AddOperand(e->value());
      end->AddBackwardEdge(e);
    }
  }

  graph->set_start(start);
  graph->set_end  (end);
  return true;
}

} // namespace ir
} // namespace cbase
} // namespace lavascript
