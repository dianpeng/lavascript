#include "graph.h"

#include "dest/interpreter/bytecode.h"
#include "dest/interpreter/bytecode-iterator.h"

#include <vector>
#include <set>
#include <map>

namespace lavascript {
namespace cbase {
using namespace ::lavascript::interpreter;

namespace {
class GraphBuilder;

/**
 * Simple bytecode analysis to help build Graph. This pass will figure out
 * several information :
 *
 * 1) at the start of each basic block, the local variable mapping
 *
 * 2) different loop header and also which variable has been modified inside
 *    of it since we need to insert PHI (this doesn't include variable bounded
 *    inside of the loop)
 *
 * 3) nested loop information , which allow us to do loop peeling when do OSR
 *    compilation
 */

class BytecodeAnalysis {
 public:
  // For loop scope, just need to use this class to push the scope, it will push
  // basic block scope and loop scope
  class LoopScope;

  // For none loop scope, use this class to push the scope since it will only push
  // basic block scope
  class BasicBlockScope;

  BytecodeAnalysis( const Handle<Prototype>& proto );
  void DoAnalysis();

  struct BasicBlockVariable {
    const BasicBlockVariable* prev;  // parent scope
    std::set<std::uint8_t> variable;
    const std::uint32_t* start;
    const std::uint32_t* end  ;      // this is end of the BB it stops when a jump/return happened
    BasicBlockVariable(): prev(NULL), variable(), start(NULL), end(NULL) {}

    // Whether a register slot is alive at this BB , excludes any nested BB inside of this BB
    bool IsAlive( std::uint8_t ) const;
  };


  // LoopHeaderInfo captures the loop's internal body information and its nested
  // information
  struct LoopHeaderInfo {
    const LoopHeaderInfo* prev ;  // this pointer points to its parental loop if it has one
    const BasicBlockVariable* bb; // corresponding basic block
    const std::uint32_t* start ;  // start of the bytecode
    const std::uint32_t* end   ;  // end   of the bytecode
    std::set<std::uint8_t> phi;   // variables that have been modified and need to insert PHI
                                  // ahead of the loop
    LoopHeaderInfo(): prev(NULL), bb(NULL), start(NULL), end(NULL) , phi() {}

    const BasicBlockVariable* enclosed_bb() const {
      return bb->prev;
    }
  };

  typedef std::map<const std::uint32_t*,LoopHeaderInfo> LoopHeaderInfoMap;
  typedef std::map<const std::uint32_t*,BasicBlockVariable> BasicBlockVariableMap;

 private:
  // build the liveness for a single bytecode; bytecode cannot be control
  // flow transfer
  bool BuildBytecode( BytecodeIterator* );
  void BuildBasicBlock   ( BytecodeIterator* );

  void BuildBranch  ( BytecodeIterator* );
  void BuildLogic   ( BytecodeIterator* );
  void BuildTernary ( BytecodeIterator* );
  void BuildLoop    ( BytecodeIterator* );

  bool IsLocalVar   ( std::uint8_t reg ) const {
    return reg < max_local_var_size_;
  }

  // Properly handle register kill event
  void Kill( std::uint8_t reg );

 private:
  BasicBlockVariable* NewBasicBlockVar( const std::uint32_t* start ) {
    std::pair<BasicBlockVariableMap::iterator,bool>
      ret = basic_block_variable_.insert(std::make_pair(start,BasicBlockVariable()));
    lava_debug(NORMAL,lava_verify(ret.second););
    BasicBlockVariable* node = &(ret.first->second);
    node->prev   = basic_block_stack_.empty() ? NULL : basic_block_stack_.back();
    node->start  = start;
    return node;
  }

  LoopHeaderInfo* NewLoopHeaderInfo( const BasicBlockVariable* bb ,
                                     const std::uint32_t* start ) {
    std::pair<LoopHeaderInfoMap::iterator,bool>
      ret = loop_header_info_.insert(std::make_pair(start,LoopHeaderInfoMap()));
    lava_debug(NORMAL,lava_verify(ret.second););
    LoopHeaderInfo* node = &(ret.first->second);
    node->prev = loop_stack_.empty() ? NULL : loop_stack_.back();
    node->start= start;
    node->bb   = bb;
    return node;
  }

  LoopHeaderInfo* current_loop() { return loop_stack_.empty() ? NULL : loop_satck_.back(); }

  BasicBlockVariable* current_bb() { return basic_block_stack_.back(); }

 private:
  Handle<Prototype> proto_;
  std::uint8_t max_local_var_size_;

  // loop header information , pay attention that all the memory is
  // owned by the std::map
  LoopHeaderInfoMap loop_header_info_;

  // basic block variable map information
  BasicBlockVariableMap basic_block_variable_;

  // loop stack , context/state information
  std::vector<LoopHeaderInfo*> loop_stack_;

  // basic block stack, context/state information
  std::vector<BasicBlockVariable*> basic_block_stack_;

  friend class LoopScope;
  friend class BasicBlockScope;
};

class BytecodeAnalysis::LoopScope {
 public:
  LoopScope( BytecodeAnalysis* ba , const std::uint32_t* bb_start ,
                                    const std::uint32_t* loop_start ):
    ba_(ba)
  {
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
  BytecodeAnalysis* ba_;
};

class BytecodeAnalysis::BasicBlockScope {
 public:
  BasicBlockScope( BytecodeAnalysis* ba , const std::uint32_t* pc ):
    ba_(ba)
  {
    ba->basic_block_stack_.push_back(ba->NewBasicBlockVar(pc));
  }

  ~BasicBlockScope() {
    ba_->basic_block_stack_.pop_back();
  }
 private:
  BytecodeAnalysis* ba_;
};

bool BytecodeAnalysis::BasicBlockVariable::IsAlive( std::uint8_t reg ) const {
  BasicBlockVariable* scope = this;
  do {
    if(scope->variable.find(reg) != scope->variable.end())
      return true;
    scope = scope->prev;
  } while(scope);
  return false;
}

void BytecodeAnalysis::Kill( std::uint8_t reg ) {
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
      current_loop()->phi.insert(reg);
    }
  }
}

// build the liveness against basic block
void BytecodeAnalysis::BuildBasicBlock( BytecodeIterator* itr ) {
  BasicBlockScope scope(this,itr->pc());
  for( ; itr->HasNext() && BuildBytecode(itr) ; itr->Next() )
    ;
  current_bb()->end = itr->pc();
}

void BytecodeAnalysis::BuildBranch( BytecodeIterator* itr ) {
  lava_debug(NORMAL,lava_verify(itr->bytecode() == BC_JMPF););
  std::uint8_t a1; std::uint16_t a2;
  itr->GetOperand(&a1,&a2);
  const std::uint32_t* false_pc = itr->OffsetAt(a2);

  // true branch
  itr->Next();
  {
    BasicBlockScope scope(this,itr->pc()); // add new basic block
    for( ; itr->HasNext() ; itr->Next() ) {
      if(itr->pc() == false_pc) break;
      if(itr->opcode() == BC_JMP) {
        lava_debug(NORMAL,lava_verify(itr->pc()+1 == false_pc););
        break;
      }
      if(!BuildBytecode(itr)) {
        itr->BranchTo(a2);  // set the itr to the start of next bb
        break;
      }
    }
  }

  // false branch
  lava_debug(NORMAL,lava_verify(itr->pc() == false_pc););
  {
    BasicBlockScope scope(this,itr->pc());
    BuildBasicBlock(itr);
  }
}

void BytecodeAnalysis::BuildLogic( BytecodeIterator* itr ) {
  lava_debug(NORMAL,lava_verify(itr->opcode() == BC_OR || itr->opcode() == BC_AND););
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

void BytecodeAnalysis::BuildTernary( BytecodeIterator* itr ) {
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

void BytecodeAnalysis::BuildLoop( BytecodeIterator* itr ) {
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
      switch(itr->opcode()) {
        case BC_FEND1: case BC_FEND2: case BC_FEEND:
          /** end of the loop */
          goto done;
        default:
          if(!BuildBytecode(itr)) goto done;
          break;
      }
    }
done:
    current_loop()->end = itr->pc();
    lava_debug(NORMAL,
          if(itr->opcode() == BC_FEND1 ||
                       itr->opcode() == BC_FEND2 ||
                       itr->opcode() == BC_FEEND) {
            itr->Next(); lava_verify(itr->pc() == itr->OffsetAt(offset));
          }
        );
    itr->BranchTo(offset);
  }
}

void BytecodeAnalysis::BuildForeverLoop( BytecodeIterator* itr ) {
  lava_debug(NORMAL,lava_verify(itr->opcode() == BC_FEVRSTART););
  std::uint16_t offset;
  const std::uint32_t* loop_start_pc = itr->pc();

  itr->GetOperand(&offset);
  itr->Next();

  { // enter into loop body
    LoopScope scope(this,itr->pc(),loop_start_pc);

    for( ; itr->HasNext() ; itr->Next()) {
      switch(itr->opcode()) {
        case BC_FEVREND: goto done;
        default:
          if(!BuildBytecode(itr)) goto done;
          break;
      }
    }

done:
    current_loop()->end = itr->pc();
    lava_debug(NORMAL,
        if(itr->opcode() == BC_FEVREND) {
          itr->Next();
          lava_verify(itr->pc() == itr->OffsetAt(offset));
        }
      );
  }
  itr->BranchTo(offset);
}

bool BytecodeAnalysis::BuildBytecode( BytecodeIterator* itr ) {
  switch(itr->opcode()) {
    case BC_ADDRV: case BC_ADDVR: case BC_ADDVV:
    case BC_SUBRV: case BC_SUBVR: case BC_SUBVV:
    case BC_MULRV: case BC_MULVR: case BC_MULVV:
    case BC_DIVRV: case BC_DIVVR: case BC_DIVVV:
    case BC_MODRV: case BC_MODVR: case BC_MODVV:
    case BC_POWRV: case BC_POWVR: case BC_POWVV:
    case BC_LTRV : case BC_LTVR : case BC_LTVV :
    case BC_LERV : case BC_LEVR : case BC_LEVV :
    case BC_GTRV : case BC_GTVR : case BC_GTVV :
    case BC_GERV : case BC_GEVR : case BC_GEVV :
    case BC_EQRV : case BC_EQVR : case BC_EQSV : case BC_EQVS: case BC_EQVV:
    case BC_NERV : case BC_NEVR : case BC_NESV : case BC_NEVS: case BC_NEVV:
      {
        std::uint8_t a1,a2,a3;
        itr->GetOperand(&a1,&a2,&a3);
        if(IsLocalVar(a1)) Kill(a1);
      }
      break;
    case BC_NEGATE: case BC_NOT: case BC_MOVE:
      {
        std::uint8_t a1,a2;
        itr->GetOperand(&a1,&a2);
        if(IsLocalVar(a1)) Kill(a1);
      }
      break;
    case BC_LOAD0: case BC_LOAD1: case BC_LOADN1:
    case BC_LOADTRUE: case BC_LOADFALSE: case BC_LOADNULL:
      {
        std::uint8_t a1; itr->GetOperand(&a1);
        if(IsLocalVar(a1)) Kill(a1);
      }
      break;
    case BC_LOADR: case BC_LOADSTR:
      {
        std::uint8_t a1,a2; itr->GetOperand(&a1,&a2);
        if(IsLocalVar(a1)) Kill(a1);
      }
      break;
    case BC_LOADLIST0: case BC_LOADOBJ0:
      {
        std::uint8_t a1; itr->GetOperand(&a1);
        if(IsLocalVar(a1)) Kill(a1);
      }
      break;
    case BC_LOADLIST1:
      {
        std::uint8_t a1,a2;
        itr->GetOperand(&a1,&a2);
        if(IsLocalVar(a1)) Kill(a1);
      }
      break;
    case BC_LOADLIST2: case BC_LOADOBJ1:
      {
        std::uint8_t a1,a2,a3;
        itr->GetOperand(&a1,&a2,&a3);
        if(IsLocalVar(a1)) Kill(a1);
      }
      break;
    case BC_NEWLIST: case BC_NEWOBJ:
      {
        std::uint8_t a1; std::uint16_t a2;
        itr->GetOperand(&a1,&a2);
        if(IsLocalVar(a1)) Kill(a1);
      }
      break;
    case BC_LOADCLS:
      {
        std::uint8_t a1; std::uint16_t a2;
        itr->GetOperand(&a1,&a2);
        if(IsLocalVar(a1)) Kill(a1);
      }
      break;
    case BC_PROPGET:
    case BC_PROPGETSSO:
    case BC_IDXGET:
    case BC_IDXGETI:
      {
        std::uint8_t a1,a2,a3;
        itr->GetOperand(&a1,&a2,&a3);
        if(IsLocalVar(a1)) Kill(a1);
      }
      break;

    case BC_UVGET: case BC_GGET:
      {
        std::uint8_t a1; std::uint16_t a2;
        itr->GetOperand(&a1,&a2);
        if(IsLocalVar(a1)) Kill(a1);
      }
      break;

    case BC_GGETSSO:
      {
        std::uint8_t a1 , a2;
        itr->GetOperand(&a1,&a2);
        if(IsLocalVar(a1)) current-bb()->Add(a1);
      }
      break;

    case BC_IDREF:
      {
        std::uint8_t a1 , a2, a3;
        itr->GetOperand(&a1,&a2);
        if(IsLocalVar(a2)) Kill(a2);
        if(IsLocalVar(a3)) Kill(a3);
      }
      break;

    /** these bytecodes just need to sink , no special operations */
    case BC_ADDLIST:
    case BC_ADDOBJ:
    case BC_INITCLS:

    case BC_PROPSET:
    case BC_PROPSETSSO:
    case BC_IDXSET:
    case BC_IDXSETI:
    case BC_UVSET:
    case BC_GSET:
    case BC_GSETSSO:

    case BC_CALL:
    case BC_TCALL:
      break;

    /** all are control flow instruction/bytecode **/
    case BC_JMPF:
      BuildBranch(itr); break;
    case BC_AND: case BC_OR:
      BuildLogic (itr);
      break;
    case BC_TERN:
      BuildTernary(itr);
      break;
    case BC_FEVRSTART:
      BuildForeverLoop(itr); break;
    case BC_FESTART: case BC_FSTART:
      BuildLoop(itr); break;

    /** terminated Bytecode which abort this basic block **/
    case BC_CONT:
    case BC_BRK:
    case BC_RET:
    case BC_RETNULL:
      return false;

    default:
      lava_unreachF("cannot reach here, bytecode %s",itr->opcode_name());
      break;
  }
}

/* ====================================================================
 *
 * Graph Builder
 *
 * ===================================================================*/

// Data structure record the pending jump that happened inside of the loop
// body. It is typically break and continue keyword , since they cause a
// unconditional jump happened
struct LoopJump {
  ir::Jump* node;    // node that is created when break/continue jump happened
  std::uint16_t pc;  // record where the jump goes to, used for debugging purpose
  LoopJump( ir::Jump* n , std::uint16_t t ) : node(n), pc(t) {}
};

// Hold information related IR graph during a loop is constructed. This is
// created on demand and pop out when loop is constructed
struct LoopInfo {
  // all pending break happened inside of this loop
  std::vector<LoopJump> pending_break;

  // all pending continue happened inside of this loop
  std::vector<LoopJump> pending_continue;

  // a pointer points to a LoopHeaderInfo object
  BytecodeAnalysis::LoopHeaderInfo* loop_header_info;

  void AddBreak( ir::Jump* node , std::uint16_t target ) {
    pending_break.push_back(LoopJump(node,target));
  }
  void AddContinue( ir::Jump* node , std::uint16_t target ) {
    pending_continue.push_back(LoopJump(node,target));
  }
};

// Structure to record function level information when we do IR construction.
// Once a inline happened, then we will push a new FuncInfo object into func_info
// stack/vector
struct FuncInfo {
  Handle<Closure> closure;
  ir::ControlFlow* region;
  std::uint32_t base;
  std::uint8_t  max_local_var_size;
  std::uint16_t  nested_loop_size;
  std::vector<LoopInfo> loop_info;
  BytecodeAnalysis analysis; // information for bytecode analysis

  bool IsLocalVar( std::uint8_t slot ) const {
    return slot < max_local_var_size;
  }

 public: // Loop related stuff
  // Add a new loop
  void EnterLoop() { loop_info.push_back(LoopInfo()); ++nested_loop_size; }
  void LeaveLoop() { loop_info.pop_back(); }

  // check whether we have loop currently
  bool HasLoop() const { return !loop_info.empty(); }

  // get the current loop's LoopInfo structure
  LoopInfo& CurrentLoop() { return loop_info.back(); }

};


// A graph builder. It is responsible for building :
//   1) normal function with main entry
//   2) function with OSR entry , this will just compile code in that nested loop tree
class GraphBuilder {
 public:
  class LoopScope;
  class BackupStack;
  typedef std::vector<ir::Expr*> ValueStack;

 public:
  GraphBuilder( zone::Zone* zone , const Handle<Closure>& closure ,
                                   const std::uint32_t* osr ):
    zone_        (zone),
    script_      (script),
    osr_         (osr),
    start_       (NULL),
    end_         (NULL),
    stack_       () ,
    func_info_   ()
  {}

  bool Build();

 private: // Context managment
  GraphBuilderContext& current_context() {
    return context_.back();
  }

  void push_context( const GraphBuilderContext& ctx ) {
    context_.push_back(ctx);
  }

  GrpahBuilderContext& pop_context() {
    context_.pop();
    return context_.back();
  }

 private: // Stack accessing

  inline void StackSet( std::uint32_t index , ir::Expr* value );

  void StackReset( std::uint32_t index ) {
    stack_[StackIndex(index)] = NULL;
  }

  ir::Expr* StackGet( std::uint32_t index ) {
    return stack_[index+stack_base_.back()];
  }

  std::uint32_t StackIndex( std::uint32_t index ) const {
    return stack_base_.back()+index;
  }

 private: // Current FuncInfo
  FuncInfo& func_info() {
    return func_info_.back();
  }

  bool IsTopFunction() const { return func_info_.size() == 1; }

  const Handle<Closure>& closure() const {
    return func_info().closure;
  }

  const Handle<Prototype>& prototype() const {
    return closure().prototype();
  }

  std::uint32_t base() const {
    return func_info().base;
  }

  ir::ControlFlow* region() const {
    return func_info().region;
  }

  void set_region( ir::ControlFlow* new_region ) {
    func_info().region = new_region;
  }

 private: // Constant handling
  ir::Expr* NewConstNumber( std::int32_t , const std::uint32_t* pc );
  ir::Expr* NewNumber( std::uint8_t ref  , const std::uint32_t* pc );
  ir::Expr* NewString( std::uint8_t ref  , const std::uint32_t* pc );
  ir::Expr* NewSSO   ( std::uint8_t ref  , const std::uint32_t* pc );
  ir::Expr* NewBoolean( bool , const std::uint32_t* pc );

  // Helper function for constructing Bytecode

 private:
  // Build routine's return status code
  enum StopReason {
    STOP_JUMP,
    STOP_EOF ,
    STOP_END ,
    STOP_FAIL,
    STOP_SUCCESS
  };

  // Just build *one* BC isntruction , this will not build certain type of BCs
  // since it is expected other routine to consume those BCs
  void BuildBytecode( BytecodeIterator* itr );

  StopReason BuildBasicBlock( BytecodeIterator* itr , const std::uint32_t* end_pc = NULL );

  // Build branch IR graph
  StopReason GotoBranchEnd( BytecodeIterator* , const std::uint32_t* );
  StopReason BuildBranch( BytecodeIterator* itr );
  StopReason BuildBranchBlock( BytecodeIterator* , const std::uint32_t* );

  // Build logical IR graph
  StopReason BuildLogic ( BytecodeIterator* itr );

  // Build ternary IR graph
  StopReason BuildTernary( BytecodeIterator* itr );

  // Build loop IR graph
  //
  // Loop IR graph construction is little bit tricky because of the cycle. However due
  // to our bytecode , it is not very complicated compare to other vm's bytecode. The
  // reason is because we have a way to tell whether a register index is for a variable
  // or just a intermediate value. We don't insert PHI to intermediate value but only
  // to variable. So in general it is similar to how we do the branch
  StopReason BuildLoop  ( BytecodeIterator* itr );
  StopReason BuildForeverLoop( BytecodeIterator* itr );

  // Iterate until we see FEEND/FEND1/FEND2/FEVREND
  StopReason BuildLoopBlock( BytecodeIterator* itr );

 private:
  void InsertPhi( ValueStack* dest , const ValueStack& false_stack ,
                                     const ValueStack& true_stack ,
                                     const std::uint32_t* );

  void InsertPhi( const ValueStack& false_stack , const ValueStack& true_stack ,
                                                  const std::uint32_t* pc ) {
    InsertPhi(stack_,false_stack,true_stack,pc);
  }

 private:
  zone::Zone* zone_;
  ir::NodeFactory* node_factory_;
  ir::Start* start_;
  ir::End*   end_;
  const std::uint32_t* osr_;
  // Working set data , used when doing inline and other stuff
  ValueStack* stack_;
  std::vector<FuncInfo> func_info_;

  friend class LoopScope;
  friend class BackupStack;
};

class GraphBuilder::LoopScope {
 public:
  LoopScope( GraphBuilder* gb ) : gb_(gb) {
    gb->func_info().EnterLoop();
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

void GraphBuilder::InsertPhi( GraphBuilder::ValueStack* dest , const GraphBuilder::ValueStack& false_stack ,
                                                               const GraphBuilder::ValueStack& true_stack ,
                                                               const std::uint32_t* pc ) {
  lava_debug(NORMAL,lava_verify(false_stack.size() == true_stack.size()););

  for( std::size_t i = 0 ; i < false_stack.size() ; ++i ) {
    ir::Expr* lhs = false_stack[i];
    ir::Expr* rhs = true_stack [i];
    /**
     * if one of lhs and rhs is NULL, it basically means some lexical scope bounded
     * variable is mutated and obviously not variable that needs a PHI, so we just
     * need to skip these type of variable entirely
     */
    if(lhs && rhs) {
      if(lhs != rhs)
        dest[i] = ir::Phi::New(node_factory_,lhs,rhs,GetBytecodeInfo(pc));
      else
        dest[i] = lhs;
    }
  }
}

inline GraphBuilder::StackSet( std::uint32_t index , ir::Expr* node , const std::uint32_t* pc ) {
  stack_[StackIndex(index)] = node;
}

GraphBuilder::StopReason GraphBuilder::BuildLogic( BytecodeIterator* itr ) {
  lava_debug(NORMAL,lava_verify(itr->opcode() == BC_AND || itr->opcode() == BC_OR););
  bool op_and = itr->opcode() == BC_AND ? true : false;
  std::uint8_t reg; std::uint16_t offset;
  itr->GetOperand(&reg,&offset);

  // where we should end for the other part of the logical cominator
  const std::uint32_t* end_pc = itr->OffsetAt(offset);

  // save the fallthrough value for PHI node later on
  ir::Expr* lhs = StackGet(reg);

  lava_debug(NORMAL,StackReset(reg););

  { // evaluate the rhs
    itr->Next();
    StopReason reason = BuildBasicBlock(itr,end_pc);
    lava_verify(reason == STOP_END);
  }

  lava_debug(NORMAL,lava_verify(StackGet(reg)););

  if(op_and)
    StackSet(reg,ir::And(node_factory_,lhs,StackGet(reg),GetBytecodeInfo(itr->pc())),itr->pc());
  else
    StackSet(reg,ir::Or (node_factory_,lhs,StackGet(reg),GetBytecodeInfo(itr->pc())),itr->pc());

  return STOP_SUCCESS;
}

GraphBuilder::StopReason GraphBuilder::BuildTernary( BytecodeIterator* itr ) {
  lava_debug(NORMAL,lava_verify(itr->opcode() == BC_TERN););
  std::uint8_t cond , result , dummy;
  std::uint32_t offset;
  std::uint16_t final_cursor;
  ir::Expr* lhs, *rhs;

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

  StackSet(i, ir::Ternary::New(node_factory_,StackGet(cond),lhs,rhs,GetBytecodeInfo(itr->pc())));

  return STOP_SUCCESS;
}

GraphBuilder::StopReason GraphBuilder::GotoBranchEnd( BytecodeIterator* itr,
                                                      const std::uint32_t* pc ) {
  for( ; itr->HasNext() ; itr->Next() ) {
    if(itr->pc() == pc) return STOP_END;
    if(itr->opcode() == BC_JUMP) return STOP_JUMP;
  }
  lava_unreachF("cannot reach here since it is end of the stream %p:%p",itr->pc(),pc);
  return STOP_EOF;
}

GraphBuilder::StopReason GraphBuilder::BuildBranchBlock( BytecodeIterator* itr ,
                                                         const std::uint32_t* pc ) {
  for( ; itr->HasNext(); itr->Next() ) {
    // check whether we reache end of PC where we suppose to stop
    if(pc == itr->pc()) return STOP_END;

    // check whether we have a unconditional jump or not
    if(itr->opcode() == BC_JUMP) return STOP_JUMP;
    switch(itr->opcode()) {
      case BC_JUMP: return STOP_JUMP;
      case BC_CONT: case BC_BRK:
        BuildBytecode(itr); itr->Next();
        return GotoBranchEnd(itr,pc);
      default:
        BuildBytecode(itr);
        break;
    }
  }
  lava_unreachF("cannot reach here since it is end of the stream %p:%p",itr->pc(),pc);
  return STOP_EOF;
}

GraphBuilder::StopReason
GraphBuilder::BuildBranch( BytecodeIterator* itr ) {
  lava_debug(NORMAL,lava_verify(itr->opcode() == BC_JMPF););

  // skip the BC_JMPF
  lava_verify(itr->Next(););

  std::uint8_t cond; std::uint16_t offset;
  itr->GetOperand(&cond,&offset);

  // create the leading If node
  ir::If* if_region = ir::Region::New(node_factory_,StackGet(cond),region());
  ir::Region* false_region = ir::Region::New(node_factory_,if_region);
  ir::Region* true_region  = ir::Region::New(node_factory_,if_region);
  ir::Merge * merge = ir::Merge::New(node_factory_,false_region,true_region);
  ValueStack true_stack;

  const std::uint32_t* false_pc;
  std::uint16_t final_cursor;
  bool have_false_branch;

  // 1. Build code inside of the *true* branch and it will also help us to identify whether we
  //    have dangling elif/else branch
  {
    itr->Next(); // skip the BC_JMPF

    BackupStack(&true_Stack); // back up the old stack and use new stack

    set_region(true_region);  // switch to true region

    StopReason reason = BuildBranchBlock(itr,itr->OffsetAt(offset));

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

  // 2. Build code inside of the *true* branch
  if(have_false_branch) {

    set_region(false_region);

    itr->BranchTo(offset); // go to the false branch

    StopReason reason = BuildBranch(itr,itr->OffsetAt(final_cursor));
  } else {
    final_cursor = offset; // we don't have a else/elif branch
  }

  // 3. handle PHI node
  InsertPhi(*stack_ /* false stack */, true_stack /* true stack */ , itr->OffsetAt(final_cursor));

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
 *   the loop based on the information provided by the BytecodeAnalysis since it will
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
 *   So we will first generate loop body for "loop C" since it is OSR entry , including loop D
 *   will be generated. Then we will generate IR for the peeling part which is whatever that
 *   is left after the loop C finish its body. And then go back to the header of loop B to generate
 *   whatever block that is before the loop C. Same style goes to the loop A. Basically for all
 *   enclosed loop we gonna do a simple peeling ; for whatever that is enclosed/nested, normal way.
 *
 *   After these nested loop cluster been generated, our code generation will be done here. In our
 *   case is when we exit loop A there will be a deoptimization happened and we will fallback to
 *   the interpreter.
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

GraphBuilder::StopReason
GraphBuilder::BuildLoop( BytecodeIterator* itr ) {
  lava_debug(NORMAL,lava_verify(itr->opcode() == BC_FSTART ||
                                itr->opcode() == BC_FESTART||
                                itr->opcode() == BC_FEVRSTART););

  std::uint16_t after_pc , exit_pc;
  ir::If*       loop_header = NULL;
  ir::Loop*     body = NULL;
  ir::LoopExit* exit = NULL;
  ir::Merge*    after  = NULL; // the region right after the loop body

  // 1. construct the loop's first branch. all loop here are automatically
  //    inversed
  if(itr->opcode() == BC_FSTART) {
    std::uint8_t a1;
    itr->GetArgument(&a1,&after_pc);
    loop_header = ir::If::New(node_factory_,StackGet(a1),region());
  } else if(itr->opcode() == BC_FESTART) {
    std::uint8_t a1;
    itr->GetArgument(&a1,&after_pc);
    // create ir ItrNew which basically initialize the itr and also do
    // a test against the iterator to see whether it is workable
    ir::ItrNew* inew = ir::ItrNew::New(node_factory_,StackGet(a1),
                                                     GetBytecode(pc->itr()));
    loop_header = ir::If::New(node_factory_,inew,region());
  } else {
    lava_debug(NORMAL,lava_verify(itr->opcode() == BC_FEVRSTART););
    /**
     * for forever loop, we still build the structure of inverse loop, but just
     * mark the condition to be true. later pass for eliminating branch will take
     * care of this false inversed loop if
     */
    loop_header = ir::If::New(node_factory_,NewBoolean(true,itr->pc()),region());
  }

  itr->Next();

  set_region(loop_header);

  // reserve the after region and iterate its bytecode later on
  after = ir::Merge::New(node_factory_,loop_header);

  // 2. enter into the loop's body
  {
    LoopScope lscope(this);

    // create new loop body node
    body = ir::Loop::New(node_factory_,region());

    // set it as the current region node
    set_region(body);

    // iterate all the BCs inside of the loop body and also mark to generate PHI
    StopReason reason = BuildLoopBlock(itr);
    lava_debug(NORMAL, lava_verify(reason == STOP_SUCCESS || reason == STOP_JUMP););

    // now we should stop at the FEND1/FEND2/FEEND instruction
    switch(itr->opcode()) {
      case BC_FEND1: case BC_FEND2:
        {
          std::uint8_t a1,a2,a3,a4;
          itr->GetOperand(&a1,&a2,&a3,&a4);

          ir::Binary* comparison;

          if(itr->opcode() == BC_FEEND) {
            comparison = ir::Binary::New(node_factory_,StackGet(a1), StackGet(a2), ir::Binary::LT,
                                                                                   GetBytecode(itr->pc()));
          } else {
            // |a1| + |a3| < |a2| , here we should take care of the PHI node insertion
            ir::Binary* loop_induction = ir::Phi::New(node_factory_,GetBytecode(itr->pc()));

            // left handside of the PHI are original a1
            loop_induction->AddDef( StackGet(a1) );

            // the addition node will use the PHI node as its left hand side
            ir::Binary* addition = ir::Binary::New(node_factory_,loop_induction,StackGet(a3),
                                                                                ir::Binary::ADD,
                                                                                GetBytecode(itr->pc()));
            // finish the PHI node construction
            loop_induction->AddDef(addition);

            // store the PHI node back to the slot
            StackSet(a1,loop_induction,itr->pc());

            // construct comparison node
            comparison = ir::Binary::New(node_factory_,addition,StackGet(a2), ir::Binary::LT,
                                                                              GetBytecode(itr->pc()));
          }

          // exit points back to the body of the loop
          lava_debug(NORMAL,lava_verify(exit == NULL););

          // create the loop exit node
          exit = ir::LoopExit::New(node_factory_,comparison,after,body);
        }
        break;
      default: /* BC_FEEND */
        {
          std::uint8_t a1; std::uint16_t pc;
          itr->GetOperand(&a1,&pc);

          ir::ItrNext* comparison =
            ir::ItrNext::New(node_factory_,StackGet(a1),GetBytecode(itr->pc()));

          exit = ir::LoopExit::New(node_factory_,comparison,after,body);
        }
        break;
    }

    // what is the loop exit block's PC
    exit_pc = itr->pc();

    // skip the last end instruction
    itr->Next();

    // patch all the pending continue and break node
    for( auto &e : func_info().CurrentLoop().pending_break ) {
      lava_debug(NORMAL,lava_verify(e.pc == after_pc););
      after->AddBackwardEdge(e.node);
    }

    for( auto &e : func_info().CurrentLoop().pending_continue ) {
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
        ir::Binary* node =
          ir::Binary::New(node_factory_,NewNumber(a1),StackGet(a2),
                                                      ir::Binary::BytecodeToOperator(itr->opcode()),
                                                      GetBytecodeInfo(dest,itr->pc()));
        StackSet(dest,node,itr->pc());
      }
      break;
    case BC_ADDVR: case BC_SUBVR: case BC_MULVR: case BC_DIVVR: case BC_MODVR: case BC_POWVR:
    case BC_LTVR : case BC_LEVR : case BC_GTVR : case BC_GEVR : case BC_EQVR : case BC_NEVR :
      {
        std::uint8_t dest , a1, a2;
        itr->GetOperand(&dest,&a1,&a2);
        ir::Binary* node =
          ir::Binary::New(node_factory_,StackGet(a1),NewNumber(a2),
                                                      ir::Binary::BytecodeToOperator(itr->opcode()),
                                                      GetBytecodeInfo(dest,itr->pc()));
        StackSet(dest,node,itr->pc());
      }
      break;
    case BC_ADDVV: case BC_SUBVV: case BC_MULVV: case BC_DIVVV: case BC_MODVV: case BC_POWVV:
    case BC_LTVV : case BC_LEVV : case BC_GTVV : case BC_GEVV : case BC_EQVV : case BC_NEVV :
      {
        std::uint8_t dest, a1, a2;
        itr->GetOperand(&dest,&a1,&a2);

        ir::Binary* node =
          ir::Binary::New(node_factory_,StackGet(a1),StackGet(a2),
                                                      ir::Binary::BytecodeToOperator(itr->opcode()),
                                                      GetBytecodeInfo(dest,itr->pc()));
        StackSet(dest,node,itr->pc());
      }
      break;
    case BC_EQSV: case BC_NESV:
      {
        std::uint8_t dest, a1, a2;
        itr->GetOperand(&dest,&a1,&a2);

        ir::Binary* node =
          ir::Binary::New(node_factory_,NewString(a1),StackGet(a2),
                                                      ir::Binary::BytecodeToOperator(itr->opcode()),
                                                      GetBytecodeInfo(dest,itr->pc()));
        StackSet(dest,node,itr->pc());
      }
      break;
    case BC_EQVS: case BC_NEVS:
      {
        std::uint8_t dest, a1, a2;
        itr->GetOperand(&dest,&a1,&a2);

        ir::Binary* node =
          ir::Binary::New(node_factory_,StackGet(a1),NewString(a2),
                                                      ir::Binary::BytecodeToOperator(itr->opcode()),
                                                      GetBytecodeInfo(dest,itr->pc()));
        StackSet(dest,node,itr->pc());
      }
      break;
    /* unary operation */
    case BC_NEGATE: case BC_NOT:
      {
        std::uint8_t dest, src;
        itr->GetOperand(&dest,&src);
        ir::Unary* node = ir::Unary::New( node_factory_,StackGet(src),
                                          ir::Unary::BytecodeToOperator(itr->opcode()),
                                          GetBytecodeInfo(dest,itr->pc()));

        StackSet(dest,node,itr->pc());
      }
      break;
    /* move */
    case BC_MOVE:
      {
        std::uint8_t desst,src;
        itr->GetOperand(&dest,&src);
        StackSet(dest,StackGet(src),itr->pc());
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
        StackSet(dest,NewConstNumber(num),itr->pc());
      }
      break;
    case BC_LOADR:
      {
        std::uint8_t dest,src;
        itr->GetOperand(&dest,&src);
        StackSet(dest,NewNumber(src),itr->pc());
      }
      break;
    case BC_LOADSTR:
      {
        std::uint8_t dest,src;
        itr->GetOperand(&dest,&src);
        StackSet(dest,NewString(src),itr->pc());
      }
      break;
    case BC_LOADTRUE: case BC_LOADFALSE:
      {
        std::uint8_t dest;
        itr->GetOperand(&dest);
        StackSet(dest,NewBoolean(itr->opcode() == BC_LOADTRUE),itr->pc());
      }
      break;

    /* jumps */
    case BC_BRK: case BC_CONT:
      lava_debug(NORMAL,lava_verify(func_info().HasLoop());); // Must be inside of a loop body
      {
        std::uint16_t pc;
        itr.GetArgument(&pc);

        ir::Jump* jump = ir::Jump::New(node_factory_,region(),GetBytecodeInfo(itr->pc()));

        // modify the current region
        set_region(jump);

        if(itr.opcode() == BC_BRK)
          func_info().CurrentLoop().AddBreak(jump,pc);
        else
          func_info().CurrentLoop().AddContinue(jump,pc);
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


} // namespace
} // namespace cbase
} // namespace lavascript
