#include "graph.h"

#include "dest/interpreter/bytecode.h"
#include "dest/interpreter/bytecode-iterator.h"

#include <vector>
#include <set>
#include <map>

namespace lavascript {
namespace cbase {
namespace ir    {
using namespace ::lavascript::interpreter;

namespace {

// Data structure record the pending jump that happened inside of the loop
// body. It is typically break and continue keyword , since they cause a
// unconditional jump happened
struct GraphBuilder::LoopJump {
  Jump* node;        // node that is created when break/continue jump happened
  std::uint16_t pc;  // record where the jump goes to, used for debugging purpose
  LoopJump( Jump* n , std::uint16_t t ) : node(n), pc(t) {}
};

// Hold information related IR graph during a loop is constructed. This is
// created on demand and pop out when loop is constructed
struct GraphBuilder::LoopInfo {
  // all pending break happened inside of this loop
  std::vector<LoopJump> pending_break;

  // all pending continue happened inside of this loop
  std::vector<LoopJump> pending_continue;

  // a pointer points to a LoopHeaderInfo object
  BytecodeAnalyze::LoopHeaderInfo* loop_header_info;

  // pending PHIS in this loop's body
  struct PhiVar {
    std::uint8_t reg;  // register index
    Phi*         phi;  // phi node
    PhiVar( std::uint8_t r , Phi* p ):
      reg(r),
      phi(p)
    {}
  };
  std::vector<PhiVar> phi_list;

 public:
  void AddBreak( Jump* node , std::uint16_t target ) {
    pending_break.push_back(LoopJump(node,target));
  }

  void AddContinue( Jump* node , std::uint16_t target ) {
    pending_continue.push_back(LoopJump(node,target));
  }

  void AddPhi( std::uint8_t index , Phi* phi ) {
    phi_list.push_back(PhiVar(index,phi));
  }

  LoopInfo(): pending_break(), pending_continue(), loop_header_info(NULL) {}
};

// Structure to record function level information when we do IR construction.
// Once a inline happened, then we will push a new FuncInfo object into func_info
// stack/vector
struct GraphBuilder::FuncInfo {
  Handle<Closure> closure;
  ControlFlow* region;
  std::uint32_t base;
  std::uint8_t  max_local_var_size;
  std::uint16_t  nested_loop_size;
  std::vector<LoopInfo> loop_info;

  // a stack of nested basic block while doing IR construction
  std::vector<BytecodeAnalyze::BuildBasicVariable*> bb_stack;
  BytecodeAnalyze bc_analyze;

  bool IsLocalVar( std::uint8_t slot ) const {
    return slot < max_local_var_size;
  }

  GraphBuilder::LoopInfo& current_loop() {
    return loop_info.back();
  }

  BytecodeAnalyze::LoopHeaderInfo* current_loop_header() {
    return current_loop().loop_header_info;
  }

 public: // Loop related stuff

  // Enter into a new loop scope, the corresponding basic block
  // information will be added into stack as part of the loop scope
  inline void EnterLoop( const std::uint32_t* pc );
  void LeaveLoop() { loop_info.pop_back(); bb_stack.pop(); }

  // Enter into a new basic block. Here the BB means any block that
  // is not a loop scope
  inline void EnterBB( const std::uint32_t* pc );
  void LeaveBB() { bb_stack.pop_back(); }

  // check whether we have loop currently
  bool HasLoop() const { return !loop_info.empty(); }

  // get the current loop's LoopInfo structure
  LoopInfo& current_loop() { return loop_info.back(); }
};

inline void FuncInfo::EnterLoop( const std::uint32_t* pc ) {
  loop_info.push_back(LoopInfo());
  {
    BytecodeAnalyze::LoopHeaderInfo* info = bc_analyze.LookUpLoopHeader(pc);
    lava_debug(NORMAL,lava_verify(info););
    loop_info.back().loop_header_info = info;
  }
  bb_stack.push_back(loop_info.back().loop_header_info);
}

inline void FuncInfo::EnterBB( const std::uint32_t* pc ) {
  BytecodeAnalyze::BasicBlockVariable* bb = bc_analyze.LookUpBasicBlock(pc);
  lava_debug(NORMAL,lava_verify(bb););
  bb_stack.push_back(bb);
}

} // namespace

class GraphBuilder::BasicBlockScope {
 public:
  BasicBlockScope( GraphBuilder* gb , const std::uint32_t* pc ) : gb_(gb) {
    gb->func_info().EnterBB(pc);
  }

  ~BasicBlockScope() {
    gb_->func_info().LeaveBB();
  }

 private:
  GraphBuilder* gb_;
};

class GraphBuilder::LoopScope {
 public:
  LoopScope( GraphBuilder* gb , const std::uint32_t* pc ) : gb_(gb) {
    gb->func_info().EnterLoop(pc);
  }

  ~LoopScope() {
    gb_->func_info().LeaveLoop();
  }

 private:
  GraphBuilder* gb_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(LoopScope)
};

class GraphBuilder::BackupStack {
 public:
  BackupStack( GraphBuilder::ValueStack* new_stack , GraphBuilder* gb ):
    old_stack_(gb->stack_),
    gb_(gb)
  {
    *new_stack = *gb->old_stack_; // do a deep copy
    gb->old_stack_ = new_stack;
  }

  ~BackupStack()
  {
    gb_->stack_ = old_stack_;
  }
};

IRInfo* GraphBuilder::NewIRInfo( const std::uint32_t* pc ) {
  IRInfo* ret;
  {
    void* mem = graph_->zone()->Malloc(sizeof(IRInfo));
    ret = ConstructFromBuffer(mem, func_info().closure ,
                                   pc,
                                   func_info().current_loop_header()->phi_list,
                                   func_info().base);
  }
  return ret;
}

void GraphBuilder::InsertPhi( ValueStack* dest , const ValueStack& false_stack ,
                                                 const ValueStack& true_stack ,
                                                 const std::uint32_t* pc ) {

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
        dest[i] = Phi::New(graph_,lhs,rhs,GetBytecodeInfo(pc));
      else
        dest[i] = lhs;
    }
  }
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
    itr->Next();
    StopReason reason = BuildBasicBlock(itr,end_pc);
    lava_verify(reason == STOP_END);
  }

  lava_debug(NORMAL,lava_verify(StackGet(reg)););

  if(op_and)
    StackSet(reg,And(graph_,lhs,StackGet(reg),GetBytecodeInfo(itr->pc())),itr->pc());
  else
    StackSet(reg,Or (graph_,lhs,StackGet(reg),GetBytecodeInfo(itr->pc())),itr->pc());

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
    for( itr->Next() ; itr->HasNext() ; itr->Next() ) {
      if(itr->opcode() == BC_JUMP) break; // end of the first ternary fall through branch
      BytecodeIterator(itr);
    }

    lava_debug(NORMAL,lava_verify(itr->opcode() == BC_JUMP););
    itr->GetOperand(&final_cursor);
    lhs = StackGet(result);
  }

  const std::uint32_t* end_pc = itr->OffsetAt(final_cursor);

  { // evaluate the jump branch
    lava_debug(NORMAL,StackReset(result););
    lava_debug(NORMAL,itr->Next();lava_verify(itr->pc() == itr->OffsetAt(offset)););

    for( itr->Next() ; itr->HasNext() ; itr->Next() ) {
      if(itr->pc() == end_pc) break;
      BytecodeIterator(itr);
    }

    lava_debug(NORMAL,lava_verify(StackGet(i)););
    rhs = StackGet(result);
  }

  StackSet(i, Ternary::New(graph_,StackGet(cond),lhs,rhs,GetBytecodeInfo(itr->pc())));

  return STOP_SUCCESS;
}

GraphBuilder::StopReason GraphBuilder::GotoIfEnd( BytecodeIterator* itr,
                                                      const std::uint32_t* pc ) {
  for( ; itr->HasNext() ; itr->Next() ) {
    if(itr->pc() == pc)          return STOP_END;
    if(itr->opcode() == BC_JUMP) return STOP_JUMP;
  }
  lava_unreachF("cannot reach here since it is end of the stream %p:%p",itr->pc(),pc);
  return STOP_EOF;
}

GraphBuilder::StopReason GraphBuilder::BuildIfBlock( BytecodeIterator* itr ,
                                                     const std::uint32_t* pc ) {
  for( ; itr->HasNext(); itr->Next() ) {
    // check whether we reache end of PC where we suppose to stop
    if(pc == itr->pc()) return STOP_END;

    // check whether we have a unconditional jump or not
    if(itr->opcode() == BC_JUMP) return STOP_JUMP;
    switch(itr->opcode()) {
      case BC_JUMP:
        return STOP_JUMP;

      case BC_CONT:
      case BC_BRK:
        BuildBytecode(itr); itr->Next();
        return GotoIfEnd(itr,pc);

      default:
        BuildBytecode(itr);
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
  lava_verify(itr->Next(););

  std::uint8_t cond;    // condition's register
  std::uint16_t offset; // jump target when condition evaluated to be false

  itr->GetOperand(&cond,&offset);

  // create the leading If node
  If* if_region        = Region::New(graph_,StackGet(cond),region());
  Region* false_region = Region::New(graph_,if_region);
  Region* true_region  = Region::New(graph_,if_region);
  Merge * merge        = Merge::New (graph_,false_region,true_region);

  ValueStack true_stack;

  const std::uint32_t* false_pc;
  std::uint16_t final_cursor;
  bool have_false_branch;

  // 1. Build code inside of the *true* branch and it will also help us to
  //    identify whether we have dangling elif/else branch
  {
    BasicBlockScope scope(this,itr->pc());

    itr->Next();              // skip the BC_JMPF
    BackupStack(&true_Stack); // back up the old stack and use new stack
    set_region(true_region);  // switch to true region

    StopReason reason = BuildIfBlock(itr,itr->OffsetAt(offset));

    if(reason == STOP_JUMP) {
      // we do have a none empty false_branch
      have_false_branch = true;
      lava_debug(NORMAL,lava_verify(itr->opcode() == BC_JUMP););
      itr->GetOperand(&final_cursor);
    } else {
      lava_debug(NORMAL,lava_verify(reason == STOP_END););
      have_false_branch = false;
    }
  }

  // 2. Build code inside of the *false* branch
  if(have_false_branch) {
    BasicBlockScope scope(this,itr->pc());

    set_region(false_region);
    itr->BranchTo(offset); // go to the false branch
    StopReason reason = BuildIfBlock(itr,itr->OffsetAt(final_cursor));
  } else {
    final_cursor = offset; // we don't have a else/elif branch
  }

  // 3. handle PHI node
  InsertPhi(*stack_ , true_stack , itr->OffsetAt(final_cursor));

  itr->BranchTo(final_cursor);
  set_region(merge);
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
  for( ; itr->HasNext(); itr->Next() ) {
    switch(itr->opcode()) {
      case BC_FEEND: case BC_FEND1: case BC_FEND2: case BC_FEVREND:
        return STOP_SUCCESS;
      case BC_CONT: case BC_BRK:
        BuildBytecode(itr);
        return STOP_SUCCESS;
      default:
        BuildBytecode(itr);
        break;
    }
  }

  lava_unreachF("must be closed by BC_FEEND/BC_FEND1/BC_FEND2/BC_FEVREND");
  return STOP_FAIL;
}

void GraphBuilder::GenerateLoopPhi() {
  const std::size_t len = func_info().current_loop_header()->phi.size();
  for( std::size_t i = 0 ; i < len ; ++i ) {
    if(func_info().current_loop_header()->phi[i]) {
      Expr* old = StackGet(static_cast<std::uint32_t>(i));
      lava_debug(NORMAL,lava_verify(old););

      Phi* phi = Phi::New(graph_,old,NULL,region());
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
    if(phi != node) phi->set_rhs(node);
  }
  func_info().current_loop().phi_list.clear();
}

GraphBuilder::StopReason
GraphBuilder::BuildLoop( BytecodeIterator* itr ) {
  lava_debug(NORMAL,lava_verify(itr->opcode() == BC_FSTART ||
                                itr->opcode() == BC_FESTART||
                                itr->opcode() == BC_FEVRSTART););

  std::uint16_t after_pc;
  std::uint16_t exit_pc;

  If*       loop_header = NULL;
  Loop*     body        = NULL;
  LoopExit* exit        = NULL;
  Merge*    after       = NULL; // the region right after the loop body

  // 1. construct the loop's first branch. all loop here are automatically
  //    inversed
  if(itr->opcode() == BC_FSTART) {
    std::uint8_t a1;
    itr->GetArgument(&a1,&after_pc);
    loop_header  = If::New(graph_,StackGet(a1),region());
  } else if(itr->opcode() == BC_FESTART) {
    std::uint8_t a1;
    itr->GetArgument(&a1,&after_pc);
    // create ir ItrNew which basically initialize the itr and also do
    // a test against the iterator to see whether it is workable
    ItrNew* inew = ItrNew::New(graph_,StackGet(a1),GetBytecode(pc->itr()));
    loop_header  = If::New(graph_,inew,region());
  } else {
    lava_debug(NORMAL,lava_verify(itr->opcode() == BC_FEVRSTART););
    /**
     * for forever loop, we still build the structure of inverse loop, but just
     * mark the condition to be true. later pass for eliminating branch will take
     * care of this false inversed loop if
     */
    loop_header = If::New(graph_,NewBoolean(true,itr->pc()),region());
  }

  itr->Next();

  set_region(loop_header);

  // reserve the after region and iterate its bytecode later on
  after = Merge::New(graph_,loop_header);

  // 2. enter into the loop's body
  {
    LoopScope lscope(this);

    // create new loop body node
    body = Loop::New(graph_,region());

    // set it as the current region node
    set_region(body);

    // generate PHI node at the head of the *block*
    GenerateLoopPhi();
 
    // iterate all BC inside of the loop body
    StopReason reason = BuildLoopBlock(itr);
    lava_debug(NORMAL, lava_verify(reason == STOP_SUCCESS || reason == STOP_JUMP););

    // now we should stop at the FEND1/FEND2/FEEND instruction
    if(itr->opcode() == BC_FEND1) {
      std::uint8_t a1,a2,a3,a4;
      itr->GetOperand(&a1,&a2,&a3,&a4);
      Binary* comparison =
        Binary::New(graph_,StackGet(a1), StackGet(a2), Binary::LT, GetBytecode(itr->pc()));

      // exit points back to the body of the loop
      lava_debug(NORMAL,lava_verify(exit == NULL););

      // create the loop exit node
      exit = LoopExit::New(graph_,comparison,after,body);

    } else if(itr->opcode() == BC_FEND2) {
      std::uint8_t a1,a2,a3,a4;
      itr->GetOperand(&a1,&a2,&a3,&a4);

      // |a1| + |a3| < |a2| , here we should take care of the PHI node insertion
      Binary* loop_induction = Phi::New(graph_,GetBytecode(itr->pc()));

      // left handside of the PHI are original a1
      loop_induction->AddDef( StackGet(a1) );

      // the addition node will use the PHI node as its left hand side
      Binary* addition = Binary::New(graph_,loop_induction,StackGet(a3),
                                                           Binary::ADD,
                                                           GetBytecode(itr->pc()));
      // finish the PHI node construction
      loop_induction->AddDef(addition);

      // store the PHI node back to the slot
      StackSet(a1,loop_induction,itr->pc());

      // construct comparison node
      Binary* comparison =
        Binary::New(graph_,addition,StackGet(a2), Binary::LT, GetBytecode(itr->pc()));

      // exit points back to the body of the loop
      lava_debug(NORMAL,lava_verify(exit == NULL););

      // create the loop exit node
      exit = LoopExit::New(graph_,comparison,after,body);
    } else {
      std::uint8_t a1; std::uint16_t pc;
      itr->GetOperand(&a1,&pc);

      ItrNext* comparison = ItrNext::New(graph_,StackGet(a1),GetBytecode(itr->pc()));

      // exit points back to the body of the loop
      lava_debug(NORMAL,lava_verify(exit == NULL););

      // create the loop exit node
      exit = LoopExit::New(graph_,comparison,after,body);
    }

    // what is the loop exit block's PC
    exit_pc = itr->pc();

    // skip the last end instruction
    itr->Next();

    // patch all the Phi node
    PatchLoopPhi();

    // patch all the pending continue and break node
    for( auto &e : func_info().current_loop().pending_break ) {
      lava_debug(NORMAL,lava_verify(e.pc == after_pc););
      after->AddBackwardEdge(e.node);
    }

    for( auto &e : func_info().current_loop().pending_continue ) {
      lava_debug(NORMAL,lava_verify(e.pc ==  exit_pc););
      exit->AddContinueEdge(e.node);
    }
  }

  lava_debug(NORMAL,lava_verify(itr->pc() == after_pc););
  set_region(after);

  return STOP_SUCCESS;
}

void GraphBuilder::BuildBytecode( BytecodeIterator* itr ) {
  switch(itr->opcode()) {
    /* binary arithmetic/comparison */
    case BC_ADDRV: case BC_SUBRV: case BC_MULRV: case BC_DIVRV: case BC_MODRV: case BC_POWRV:
    case BC_LTRV:  case BC_LERV : case BC_GTRV : case BC_GERV : case BC_EQRV : case BC_NERV :
      {
        std::uint8_t dest , a1, a2;
        itr->GetOperand(&dest,&a1,&a2);
        Binary* node = Binary::New(graph_,NewNumber(a1,itr->pc()),
                                          StackGet(a2),
                                          Binary::BytecodeToOperator(itr->opcode()),
                                          NewIRInfo(itr->pc()));
        StackSet(dest,node);
      }
      break;
    case BC_ADDVR: case BC_SUBVR: case BC_MULVR: case BC_DIVVR: case BC_MODVR: case BC_POWVR:
    case BC_LTVR : case BC_LEVR : case BC_GTVR : case BC_GEVR : case BC_EQVR : case BC_NEVR :
      {
        std::uint8_t dest , a1, a2;
        itr->GetOperand(&dest,&a1,&a2);
        Binary* node = Binary::New(graph_,StackGet(a1),
                                          NewNumber(a2,itr->pc()),
                                          Binary::BytecodeToOperator(itr->opcode()),
                                          NewIRInfo(itr->pc()));
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
                                          NewIRInfo(itr->pc()));
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
                                          NewIRInfo(itr->pc()));
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
                                          NewIRInfo(itr->pc()));
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
                                         NewIRInfo(itr->pc()));

        StackSet(dest,node);
      }
      break;
    /* move */
    case BC_MOVE:
      {
        std::uint8_t desst,src;
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
        StackSet(dest,NewConstNumber(num,itr->pc()));
      }
      break;
    case BC_LOADR:
      {
        std::uint8_t dest,src;
        itr->GetOperand(&dest,&src);
        StackSet(dest,NewNumber(src,itr->pc()));
      }
      break;
    case BC_LOADSTR:
      {
        std::uint8_t dest,src;
        itr->GetOperand(&dest,&src);
        StackSet(dest,NewString(src));
      }
      break;
    case BC_LOADTRUE: case BC_LOADFALSE:
      {
        std::uint8_t dest;
        itr->GetOperand(&dest);
        StackSet(dest,NewBoolean(itr->opcode() == BC_LOADTRUE,itr->pc()));
      }
      break;
    case BC_LOADNULL:
      {
        std::uint8_t dest;
        itr->GetOperand(&dest);
        StackSet(dest,Null::New(graph_,NewIRInfo(itr->pc())));
      }
      break;

    /* jumps */
    case BC_BRK: case BC_CONT:
      lava_debug(NORMAL,lava_verify(func_info().HasLoop()););

      {
        std::uint16_t pc;
        itr.GetArgument(&pc);

        Jump* jump = Jump::New(graph_,region(),NewIRInfo(itr->pc()));

        // modify the current region
        set_region(jump);

        if(itr.opcode() == BC_BRK)
          func_info().current_loop().AddBreak(jump,pc);
        else
          func_info().current_loop().AddContinue(jump,pc);
      }
      break;
  }

  itr->Next(); // consume this bytecode
}

GraphBuilder::StopReason
GraphBuilder::BuildBasicBlock( BytecodeIterator* itr , const std::uint32_t* end_pc ) {
  for( ; itr->HasNext() ; itr->Next() ) {
    if(itr->code_position() == end_pc)
      return STOP_END;

    // build this instruction
    BuildBytecode(itr);

    // check if last opcode is break or continue which is unconditional
    // jump. so we can just abort the construction of this basic block
    if(itr->opcode() == BC_BRK || itr->opcode() == BC_CONT)
      return STOP_JUMP;
  }

  return STOP_SUCCESS;
}


} // namespace ir
} // namespace cbase
} // namespace lavascript
