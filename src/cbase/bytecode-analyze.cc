#include "bytecode-analyze.h"
#include "helper.h"

#include "src/trace.h"

namespace lavascript {
namespace cbase      {

using namespace lavascript::interpreter;

bool BytecodeAnalyze::LocalVariableIterator::Move( std::uint8_t start ) {
  while(scope_) {
    for( ; start <= max_ ; ++start ) {
      if(scope_->variable[start]) {
        cursor_ = start;
        return true;
      }
    }

    scope_ = scope_->prev; // go to its parental scope
    start  = 0;            // reset the cursor of start
  }

  lava_debug(NORMAL,lava_verify(!scope_););
  return false;
}

class BytecodeAnalyze::LoopScope {
 public:
  LoopScope( BytecodeAnalyze* ba , const std::uint32_t* bb_start ,
                                   const std::uint32_t* loop_start ):
    ba_(ba) {
    // the input PC is a FESTART/FSTART bytecode which is not part
    // of the basic block
    ba->basic_block_stack_.push_back(ba->NewBasicBlockVar(bb_start));
    ba->loop_stack_.push_back(ba->NewLoopHeaderInfo(ba->current_bb(),loop_start));
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
  const BasicBlockVariable* scope = this;
  do {
    if(scope->variable[reg]) return true;
    scope = scope->prev;
  } while(scope);
  return false;
}

void BytecodeAnalyze::Kill( std::uint8_t reg ) {
  if(current_bb()->variable[reg] || !current_bb()->IsAlive(reg)) {
    /**
     * When we reach here it means this register is not alive at
     * this bb's parent scope or it is alive at this scope, so
     * we need to record this variable as alive in the current
     * scope
     */
    current_bb()->Add(reg);
  } else {
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
        current_loop()->phi.var[reg] = true;
      }
    }
  }
}

// build the liveness against basic block
void BytecodeAnalyze::BuildBasicBlock( BytecodeIterator* itr ) {
  BasicBlockScope scope(this,itr->pc());
  while( itr->HasNext() && BuildBytecode(itr) )
    ;
  current_bb()->end = itr->pc();
}

bool BytecodeAnalyze::BuildIfBlock( BytecodeIterator* itr ,
                                    const std::uint32_t* pc ,
                                    const std::uint32_t** end ) {
  bool skip_bytecode = false;

  while( itr->HasNext() ) {
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
  lava_unreachF("%s","should never reach here since we meet a EOF of bytecode stream");
  return false;
}

bool BytecodeAnalyze::CheckElifBranch( BytecodeIterator* itr , const std::uint32_t* end ) {
  helper::BackupBytecodeIterator backup(itr);

  for( ; itr->HasNext() ; itr->Move() ) {
    if(itr->opcode() == BC_JMPF) return true;
    if(itr->pc() == end)         return false;
  }

  lava_unreachF("%s","cannot reach here since there must be a jmpf or end of stream");
  return false;
}

void BytecodeAnalyze::BuildIf( BytecodeIterator* itr ) {
  lava_debug(NORMAL,lava_verify(itr->opcode() == BC_JMPF););
  std::uint8_t a1; std::uint16_t a2;
  itr->GetOperand(&a1,&a2);
  const std::uint32_t* false_pc = itr->OffsetAt(a2);
  const std::uint32_t* final_cursor = NULL;
  bool has_else_branch = false;

  // true branch
  itr->Move();
  {
    BasicBlockScope scope(this,itr->pc()); // true branch basic block
    has_else_branch = BuildIfBlock(itr,false_pc,&(current_bb()->end));

    if(has_else_branch) {
      lava_debug(NORMAL,lava_verify(itr->opcode() == BC_JMP););
      std::uint16_t pc;
      itr->GetOperand(&pc);
      final_cursor = itr->OffsetAt(pc);
      itr->Move(); // skip the last JUMP
    }
  }

  // false branch
  lava_debug(NORMAL,lava_verify(itr->pc() == false_pc););

  if(has_else_branch) {
    if(!CheckElifBranch(itr,final_cursor)) {
      // We do not have else if branch so we need to evaluate
      // this block as a separate else block which needs to
      // setup a basic block
      BasicBlockScope scope(this,itr->pc());
      // this is a else branch so must not have any other branch
      lava_verify(!BuildIfBlock(itr,final_cursor,&(current_bb()->end)));
      lava_debug(NORMAL,lava_verify(itr->pc() == final_cursor););
    }
  }
}

void BytecodeAnalyze::BuildLogic( BytecodeIterator* itr ) {
  lava_debug(NORMAL,lava_verify(itr->opcode() == BC_OR ||
                                itr->opcode() == BC_AND););
  /**
   * Don't need to do anything and we can safely skip the body of OR/AND since
   * this is expression level control flow and doesn't really have any needed
   * information
   */
  std::uint8_t a1, a2, a3;
  std::uint32_t pc;
  itr->GetOperand(&a1,&a2,&a3,&pc);
  if(IsLocalVar(a1)) Kill(a2);
  itr->BranchTo(pc);
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
  std::uint16_t offset;
  std::uint8_t induct ;

  itr->GetOperand(&induct,&offset);
  lava_debug(NORMAL,lava_verify(IsLocalVar(induct)););
  Kill(induct);

  itr->Move();

  { // enter into loop body
    LoopScope scope(this,itr->pc(),itr->pc());

    while(itr->HasNext()) {
      if(itr->opcode() == BC_FEND1 || itr->opcode() == BC_FEND2 ||
                                      itr->opcode() == BC_FEEND)
        break;
      else
        if(!BuildBytecode(itr)) break;
    }

    current_bb()->end   = itr->pc();
    current_loop()->end = itr->pc();

    lava_debug(NORMAL,
          if(itr->opcode() == BC_FEND1 || itr->opcode() == BC_FEND2 ||
                                          itr->opcode() == BC_FEEND) {
            itr->Move();
            lava_verify(itr->pc() == itr->OffsetAt(offset));
          }
        );
    /**
     * If bytecode is *not* FEND1 then we need to mark induction
     * variable as part of the phi list since it must be mutated
     * due to the bytecode here
     */
    if(itr->opcode() != BC_FEND1) {
      Kill(induct);
    }
    itr->BranchTo(offset);
  }
}

void BytecodeAnalyze::BuildForeverLoop( BytecodeIterator* itr ) {
  lava_debug(NORMAL,lava_verify(itr->opcode() == BC_FEVRSTART););

  itr->Move();

  { // enter into loop body
    LoopScope scope(this,itr->pc(),itr->pc());

    while(itr->HasNext()) {
      if(itr->opcode() == BC_FEVREND)
        break;
      else {
        if(!BuildBytecode(itr)) break;
      }
    }
    lava_debug(NORMAL,lava_verify(itr->opcode() == BC_FEVREND););

    current_bb()->end   = itr->pc();
    current_loop()->end = itr->pc();
  }
  itr->Move(); // skip the *last* FEVREND
}

bool BytecodeAnalyze::BuildBytecode( BytecodeIterator* itr ) {
  bool ret = true;
  const BytecodeUsage& bu = itr->usage();
  for( int i = 0 ; i < BytecodeUsage::kMaxBytecodeArgumentSize ; ++i ) {
    if(bu.GetArgument(i) == BytecodeUsage::OUTPUT) {
      std::uint32_t reg;
      itr->GetOperandByIndex(i,&reg);
      if(IsLocalVar(reg)) Kill(reg);
    }
  }

  // do a dispatch based on the bytecode type
  switch(itr->opcode()) {
    case BC_JMPF:      BuildIf(itr);          break;
    case BC_TERN:      BuildTernary(itr);     break;
    case BC_AND:       BuildLogic(itr);       break;
    case BC_OR:        BuildLogic(itr);       break;
    case BC_FSTART:    BuildLoop(itr);        break;
    case BC_FESTART:   BuildLoop(itr);        break;
    case BC_FEVRSTART: BuildForeverLoop(itr); break;
    // upvalue set
    case BC_UVSET:
      if(current_loop()) {
        std::uint8_t a1,a2; itr->GetOperand(&a1,&a2);
        current_loop()->phi.uv[a1] = true;
      }
      itr->Move();
      break;
    // global variables
    case BC_GSET: case BC_GSETSSO:
      if(current_loop()) {
        std::uint8_t a1;
        std::uint16_t a2;
        itr->GetOperand(&a1,&a2);
        Str key;
        if(itr->opcode() == BC_GSET) {
          auto s = proto_->GetString(a2);
          key.data   = s->data();
          key.length = s->size();
        } else {
          auto s = proto_->GetSSO   (a2);
          key.data   = s->sso->data();
          key.length = s->sso->size();
        }

        { // insert the globals if not existed
          auto &g = current_loop()->phi.glb;
          if(std::find(g.begin(),g.end(),key) == g.end()) {
            g.push_back(key);
          }
        }
      }
      itr->Move();
      break;
    // bytecode that gonna terminate current basic block
    case BC_CONT: case BC_BRK: case BC_RET: case BC_RETNULL:
      itr->Move();
      ret = false;
      break;
    default:
      itr->Move();
      break;
  }

  return ret;
}

void BytecodeAnalyze::Dump( DumpWriter* writer ) const {
  const std::uint32_t* start = proto_->code_buffer();

  writer->WriteL("***************************************");
  writer->WriteL("        Bytecode Analysis              ");
  writer->WriteL("***************************************");
  writer->WriteL("Bytecode Start:%p",proto_->code_buffer());

  {
    DumpWriter::Section header(writer,"Basic Block Information");
    for( auto & e : basic_block_variable_ ) {
      DumpWriter::Section item(writer,"Start:%p,End:%p|Offset:%d",e.second.start,
                                                                  e.second.end,
                                                                 (e.second.start-start));
      for( std::size_t i = 0 ; i < max_local_var_size_ ; ++i ) {
        if(e.second.variable[i]) {
          writer->WriteL("Register Alive: %zu",i);
        }
      }
    }
  }

  {
    DumpWriter::Section(writer,"Loop Information");
    for( auto &e : loop_header_info_ ) {
      DumpWriter::Section item(writer,"Start:%p,End:%p|Offset:%d",e.second.start,
                                                                  e.second.end,
                                                                 (e.second.start-start));
      for( std::size_t i = 0 ; i < interpreter::kRegisterSize ; ++i ) {
        if(e.second.phi.var[i]) {
          writer->WriteL("LocalVar: %zu",i);
        }
      }

      for( std::size_t i = 0 ; i < interpreter::kMaxUpValueSize; ++i ) {
        if(e.second.phi.uv[i]) {
          writer->WriteL("UpValue: %zu",i);
        }
      }

      for( auto &v : e.second.phi.glb ) {
        writer->WriteL("Global: %s",Str::ToStdString(v).c_str());
      }
    }
  }
}

BytecodeAnalyze::BytecodeAnalyze( const Handle<Prototype>& proto ):
  proto_               (proto),
  max_local_var_size_  (proto->max_local_var_size()),
  loop_header_info_    (),
  basic_block_variable_(),
  loop_stack_          (),
  basic_block_stack_   ()
{
  BytecodeIterator itr(proto->GetBytecodeIterator());
  BuildBasicBlock(&itr);

  lava_debug(NORMAL,lava_verify(loop_stack_.empty()););
  lava_debug(NORMAL,lava_verify(basic_block_stack_.empty()););
}

BytecodeAnalyze::BytecodeAnalyze( BytecodeAnalyze&& that ):
  proto_               (that.proto_),
  max_local_var_size_  (that.proto_->max_local_var_size()),
  loop_header_info_    (std::move(that.loop_header_info_)),
  basic_block_variable_(std::move(that.basic_block_variable_)),
  loop_stack_          (std::move(that.loop_stack_)),
  basic_block_stack_   (std::move(that.basic_block_stack_))
{}

} // namespace cbase
} // namespace lavascript
