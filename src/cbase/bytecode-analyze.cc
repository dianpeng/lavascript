#include "bytecode-analyze.h"

namespace lavascript {
namespace cbase      {

using namespace lavascript::interpreter;

class BytecodeAnalyze::LoopScope {
 public:
  LoopScope( BytecodeAnalyze* ba , const std::uint32_t* bb_start ,
                                   const std::uint32_t* loop_start ):
    ba_(ba) {
    // the input PC is a FESTART/FSTART bytecode which is not part
    // of the basic block
    ba->basic_block_stack_.push_back(ba->NewBasicBlockVar(bb_start));
    ba->loop_stack_.push_back(ba->NewLoopHeaderInfo(current_bb(),loop_start));
  }

  ~LoopScope() {
    ba_->loop_stack_.pop_back();
    ba_->basic_block_stack_.pop_back();
  }

 private:
  BytecodeAnalyze* ba_;
};

class BytecodeAnalyze::BasicBlockScope {
 public:
  BasicBlockScope( BytecodeAnalyze* ba , const std::uint32_t* pc ):
    ba_(ba) {
    ba->basic_block_stack_.push_back(ba->NewBasicBlockVar(pc));
  }

  ~BasicBlockScope() {
    ba_->basic_block_stack_.pop_back();
  }
 private:
  BytecodeAnalyze* ba_;
};

bool BytecodeAnalyze::BasicBlockVariable::IsAlive( std::uint8_t reg ) const {
  BasicBlockVariable* scope = this;
  do {
    if(scope->variable[reg]) return true;
    scope = scope->prev;
  } while(scope);
  return false;
}

void BytecodeAnalyze::Kill( std::uint8_t reg ) {
  // update information of the basic block
  current_bb()->Add(reg);

  /** Update the variable use case if we are in loop body. */
  if(current_loop()) {
    lava_debug(NORMAL,lava_verify(current_loop()->enclosed_bb()););

    // since we have a loop so we try to check whether we need to
    // insert this reg into loop modified variable
    if(current_loop()->enclosed_bb()->IsAlive(reg)) {
      // this means this variable is not bounded inside of this loop
      // block but in its enclosed lexical scope chain and now it is
      // modified, so we need to insert a PHI to capture it at the
      // head of the loop
      current_loop()->phi[reg] = true;
    }
  }
}

// build the liveness against basic block
void BytecodeAnalyze::BuildBasicBlock( BytecodeIterator* itr ) {
  BasicBlockScope scope(this,itr->pc());
  for( ; itr->HasNext() && BuildBytecode(itr) ; itr->Next() )
    ;
  current_bb()->end = itr->pc();
}

bool BytecodeAnalyze::BuildIfBlock( BytecodeIterator* itr ,
                                    const std::uint32_t* pc ,
                                    std::uint32_t** end ) {
  bool skip_bytecode = false;

  for( ; itr->HasNext() ; itr->Next() ) {
    if(itr->pc() == pc) {
      if(end) *end = itr->pc();
      return false;
    } else if(itr->opcode() == BC_JMP) {
      if(end) *end = itr->pc();
      return true;
    } else if(!skip_bytecode && !BuildBytecode(itr)) {
      skip_bytecode = true;
      if(end) *end = itr->pc();
    }
  }
  lava_unreachF("should never reach here since we meet a EOF of "
                "bytecode stream");
  return false;
}

void BytecodeAnalyze::BuildIf( BytecodeIterator* itr ) {
  lava_debug(NORMAL,lava_verify(itr->bytecode() == BC_JMPF););
  std::uint8_t a1; std::uint16_t a2;
  itr->GetOperand(&a1,&a2);
  const std::uint32_t* false_pc = itr->OffsetAt(a2);
  const std::uint32_t* final_cursor = NULL;
  bool has_else_branch = false;

  // true branch
  itr->Next();
  {
    BasicBlockScope scope(this,itr->pc()); // true branch basic block
    has_else_branch = BuildIfBlock(itr,false_pc,&scope.end);

    if(has_else_branch) {
      lava_debug(NORMAL,lava_verify(itr->opcode() == BC_JMP););
      std::uint16_t pc;
      itr->GetOperand(&pc);
      final_cursor = itr->OffsetAt(pc);
    }
  }

  // false branch
  lava_debug(NORMAL,lava_verify(itr->pc() == false_pc););

  if(has_else_branch) {
    BasicBlockScope scope(this,itr->pc()); // false branch basic block
    BuildIfBlock(itr,final_cursor,NULL);
  }
  lava_debug(NORMAL,lava_verify(itr->pc() == final_cursor););
}

void BytecodeAnalyze::BuildLogic( BytecodeIterator* itr ) {
  lava_debug(NORMAL,lava_verify(itr->opcode() == BC_OR ||
                                itr->opcode() == BC_AND););
  /**
   * Don't need to do anything and we can safely skip the body of OR/AND since
   * this is expression level control flow and doesn't really have any needed
   * information
   */
  std::uint8_t a1; std::uint16_t a2;
  itr->GetOperand(&a1,&a2);
  if(IsLocalVar(a1)) Kill(a1);
  itr->BranchTo(a2);
}

void BytecodeAnalyze::BuildTernary( BytecodeIterator* itr ) {
  lava_debug(NORMAL,lava_verify(itr->opcode() == BC_TERN););
  /**
   * Pretty much like BuildLogic just need to figure out the value/register
   * and Kill it
   */
  std::uint8_t a1,a2,a3; std::uint32_t x;
  itr->GetOperand(&a1,&a2,&a3,&x);
  if(IsLocalVar(a1)) Kill(a2); // a1 is the condition , a2 is the value/variable register
  itr->BranchTo(x);
}

void BytecodeAnalyze::BuildLoop( BytecodeIterator* itr ) {
  lava_debug(NORMAL,lava_verify(itr->opcode() == BC_FSTART ||
                                itr->opcode() == BC_FESTART););
  const std::uint32_t* loop_start_pc = itr->pc();
  std::uint16_t offset;

  {
    std::uint8_t a1;
    itr->GetOperand(&a1,&offset);
    if(IsLocalVar(a1)) Kill(a1); // loop induction variable
  }

  itr->Next();
  { // enter into loop body
    LoopScope scope(this,itr->pc(),loop_start_pc);

    for( ; itr->HasNext(); itr->Next()) {
      if(itr->opcode() == BC_FEND1 || itr->opcode() == BC_FEND2 ||
                                      itr->opcode() == BC_FEEND)
        break;
      else
        if(!BuildBytecode(itr)) break;
    }

    scope.end = itr->pc();

    lava_debug(NORMAL,
          if(itr->opcode() == BC_FEND1 || itr->opcode() == BC_FEND2 ||
                                          itr->opcode() == BC_FEEND) {
            itr->Next();
            lava_verify(itr->pc() == itr->OffsetAt(offset));
          }
        );
    itr->BranchTo(offset);
  }
}

void BytecodeAnalyze::BuildForeverLoop( BytecodeIterator* itr ) {
  lava_debug(NORMAL,lava_verify(itr->opcode() == BC_FEVRSTART););
  std::uint16_t offset;
  const std::uint32_t* loop_start_pc = itr->pc();

  itr->GetOperand(&offset);
  itr->Next();

  { // enter into loop body
    LoopScope scope(this,itr->pc(),loop_start_pc);

    for( ; itr->HasNext() ; itr->Next()) {
      if(itr->opcode() == BC_FEVREND)
        break;
      else {
        if(!BuildBytecode(itr)) break;
      }
    }

    scope.end = itr->pc();

    lava_debug(NORMAL,
        if(itr->opcode() == BC_FEVREND) {
          itr->Next();
          lava_verify(itr->pc() == itr->OffsetAt(offset));
        }
      );
  }
  itr->BranchTo(offset);
}

bool BytecodeAnalyze::BuildBytecode( BytecodeIterator* itr ) {
  const BytecodeUsage& bu = itr->usage();
  for( int i = 0 ; i < BytecodeUsage::kMaxBytecodeArgumentSize ; ++i ) {
    if(bu.GetArgument(i) == BytecodeUsage::OUTPUT) {
      std::uint32_t reg;
      itr->GetArgumentByIndex(i,&reg);
      if(IsLocalVar(reg)) Kill(reg);
    }
  }

  // do a dispatch based on the bytecode type
  switch(itr->opcode()) {
    case BC_JMPF:      BuildIf(itr);      break;
    case BC_TERN:      BuildTernary(itr); break;
    case BC_AND:       BuildLogic(itr);   break;
    case BC_OR:        BuildLogic(itr);   break;
    case BC_FSTART:    BuildLoop(itr);    break;
    case BC_FESTART:   BuildLoop(itr);    break;
    case BC_FEVRSTART: BuildLoop(itr);    break;

    // bytecode that gonna terminate current basic block
    case BC_CONT:
    case BC_BRK:
    case BC_RET:
    case BC_RETNULL:
      return false;
    default: break;
  }
  return true;
}

} // namespace cbase
} // namespace lavascript
