#include "graph.h"

#include "dest/interpreter/bytecode.h"
#include "dest/interpreter/bytecode-iterator.h"

#include <vector>
#include <set>

namespace lavascript {
namespace cbase {
using namespace ::lavascript::interpreter;

namespace {
class GraphBuilder;

// Loop info encapsulate the information needed to construct
// IR related to loop , like continue jump and break jump
struct LoopJump {
  ir::Jump* node;
#if LAVASCRIPT_DEBUG_LEVEL >= 1
  std::uint16_t pc;  // record where the jump goes to, used for debugging purpose
#endif // LAVASCRIPT_DEBUG_LEVEL >= 1
  LoopJump( ir::Jump* n , std::uint16_t t ) : node(n), pc(t) {}
};

// Hold information related IR graph during a loop is constructed. This is
// created on demand and pop out when loop is constructed
struct LoopInfo {
  std::vector<LoopJump> pending_break;
  std::vector<LoopJump*> pending_continue;

  void AddBreak( ir::Jump* node , std::uint16_t target ) {
    pending_break.push_back(LoopJump(node,target));
  }
  void AddContinue( ir::Jump* node , std::uint16_t target ) {
    pending_continue.push_back(LoopJump(node,target));
  }
};

class LoopScope {
 public:
  LoopScope( GraphBuilder* gb );
  ~LoopScope();

 private:
  GraphBuilder* gb_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(LoopScope)
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
  bool rhs_branch;
  std::vector<LoopInfo> loop_info;

  bool IsLocalVar( std::uint8_t slot ) const {
    return slot < max_local_var_size;
  }

 public: // Loop related stuff
  // Add a new loop
  void EnterLoop() { loop_info.push_back(LoopInfo()); ++nested_loop_size; }
  void LeaveLoop() { loop_info.pop_back(); }

  // check whether we have loop currently
  bool has_loop() const { return !loop_info.empty(); }

  // get the current loop's LoopInfo structure
  LoopInfo& cur_loop() { return loop_info.back(); }

};

class GraphBuilder {
 public:
  GraphBuilder( zone::Zone* zone , const Handle<Closure>& closure ,
                                   const std::uint32_t* osr ,
                                   std::uint32_t stack_base = 0 ):
    zone_        (zone),
    script_      (script),
    osr_         (osr),
    start_       (NULL),
    end_         (NULL),
    stack_base_  () ,
    stack_       () ,
    branch_      ()
  {
    stack_base_.push_back(stack_base);
  }

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

  bool rhs_branch() const {
    return func_info().rhs_branch;
  }

  void set_rhs_branch( bool b ) {
    func_info().rhs_branch = b;
  }

 private: // Constant handling
  ir::Node* NewConstNumber( std::int32_t );
  ir::Node* NewNumber( std::uint8_t ref );
  ir::Node* NewString( std::uint8_t ref );
  ir::Node* NewSSO   ( std::uint8_t ref );

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
  void BuildBytecode   ( BytecodeIterator* itr );

  StopReason BuildUntil( BytecodeIterator* itr , const std::uint32_t* end_pc = NULL );

  // Build branch IR graph
  StopReason BuildBranch( BytecodeIterator* itr );
  StopReason BuildBranchBlock( BytecodeIterator* , const std::uint32_t* );

  // Build logical IR graph
  StopReason BuildLogic ( BytecodeIterator* itr );

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
  zone::Zone* zone_;
  ir::NodeFactory* node_factory_;
  ir::Start* start_;
  ir::End*   end_;

  const std::uint32_t* osr_;

  // Working set data , used when doing inline and other stuff
  std::vector<ir::Expr*> stack_;

  std::vector<FuncInfo> func_info_;
};

LoopScope::LoopScope( GraphBuilder* gb ) : gb_(gb) {
  gb->func_info().EnterLoop();
}

LoopScope::~LoopScope() {
  gb_->func_info().LeaveLoop();
}

inline GraphBuilder::StackSet( std::uint32_t index , ir::Expr* node , const std::uint32_t* pc ) {
  if(rhs_branch()) {
    /**
     * This right hand side branch so we need to check wether we need to
     * insert a PHI node here or not
     */
    ir::Expr* old = stack_[StackIndex(index)];
    if(old && old != node && func_info().IsLocalVar(index)) {
      stack_[StackIndex(index)] =
        ir::Phi::New(node_factory_,old,node,GetBytecodeInfo(StackIndex(index),pc));
    } else {
      stack_[StackIndex(index)] = node;
    }
  }
}

GraphBuilder::StopReason GraphBuilder::BuildLogic( BytecodeIterator* itr ) {
  lava_debug(NORMAL,lava_verify(itr->opcode() == BC_AND || itr->opcode() == BC_OR););
  bool op_and = itr->opcode() == BC_AND ? true : false;
  std::uint8_t reg; std::uint16_t offset;
  itr->GetOperand(&reg,&offset);
  const std::uint32_t* end_pc = itr->code_buffer() + pc;

  // save the fallthrough value for PHI node later on
  ir::Expr* saved = StackGet(reg);
  ir::Expr* false_node , *true_node;

  lava_debug(NORMAL,StackReset(reg););

  ir::Region* fallthrough_region = ir::Region::New(node_factory_);

  if(!op_and) {
    ir::Region* false_region = ir::Region::New(node_factory_,region());

    lava_verify(itr->Next());
    StopReason reason = BuildUntil(itr,end_pc);
    lava_verify(reason == STOP_END);

    // note for the false and true region order
    fallthrough_region->AddBackwardEdge(false_region);
    fallthrough_region->AddBackwardEdge(region());

    false_node = saved;
    true_node  = StackGet(reg);
  } else {
    ir::Region* true_region = ir::Region::New(node_factory_,region());

    lava_verify(itr->Next());
    StopReason reason = BuildUntil(itr,end_pc);
    lava_verify(reason == STOP_END);

    fallthrough_region->AddBackwardEdge(region());
    fallthrough_region->AddBackwardEdge(true_region);

    false_node = StackGet(reg);
    true_node  = saved;
  }

  region()->AddBackwardEdge(fallthrough_region);
  set_region( fallthrough_region );

  lava_debug(NORMAL,lava_verify(itr->pc() == end_pc););
  lava_debug(NORMAL,lava_verify(StackGet(reg) != NULL););


  // Inser the PHI node , note for the order of false node and true node
  StackSet(reg,ir::Phi::New(node_factory_,false_node,true_node,
                                                     GetBytecodeInfo(reg,itr->pc())),itr->pc());
  return STOP_SUCCESS;
}

GraphBuilder::StopReason
GraphBuilder::BuildLoopBlock( BytecodeIterator* itr ) {
  for( ; itr->HasNext(); itr->Next() ) {
    switch(itr->opcode()) {
      case BC_FEEND:
      case BC_FEND1:
      case BC_FEND2:
      case BC_FEVREND:
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

  if(itr->opcode() == BC_FEVRSTART)
    return BuildForeverLoop(itr);

  std::uint16_t jump;
  ir::If* loop_header = NULL;
  ir::Loop* body = NULL;
  ir::LoopExit* exit = NULL;
  ir::Merge*  after  = NULL; // the region right after the loop body

  // 1. construct the loop's first branch. all loop here are automatically
  //    inversed
  if(itr->opcode() == BC_FSTART) {
    std::uint8_t a1;
    itr->GetArgument(&a1,&jump);
    loop_header = ir::If::New(node_factory_,StackGet(a1),region());
  } else {
    std::uint8_t a1;
    itr->GetArgument(&a1,&jump);
    // create ir ItrNew which basically initialize the itr and also do
    // a test against the iterator to see whether it is workable
    ir::ItrNew* inew = ir::ItrNew::New(node_factory_,StackGet(a1),
                                                     GetBytecode(pc->itr()));
    loop_header = ir::If::New(node_factory_,inew,region());
  }
  itr->Next();

  set_region(loop_header); // new region

  // 2. do the false branch of each loop_header
  {
    std::size_t old_cursor = itr->cursor();
    itr->BranchTo(jump);

    // initialize after region since we are gonna evaluate Bytecode that is
    // *after* the loop body
    after = ir::Merge::New(node_factory_,region());
    set_region(after);

    StopReason reason = BuildUntil(itr);
    lava_debug(NORMAL,lava_verify(reason == STOP_EOF););

    itr->BranchTo(old_cursor); // resume to previous position
  }

  // 3. enter into the loop's body
  {
    LoopScope lscope(this);

    // create new loop body node
    body = ir::Loop::New(node_factory_,region());

    // set it as the current region node
    set_region(body);

    // the current bytecode iterator should points into the
    // very first bytecode inside of the loop's body
    lava_verify(BuildLoopBlock(itr) == STOP_SUCCESS);

    // now we should stop at the FEND1/FEND2/FEEND instruction
    switch(itr->opcode()) {
      case BC_FEND1: case BC_FEND2:
        {
          std::uint8_t a1,a2,a3,a4;
          itr->GetOperand(&a1,&a2,&a3,&a4);

          ir::Binary* comparison;

          if(itr->opcode() == ) {
            comparison = ir::Binary::New(node_factory_,StackGet(a1),
                                                       StackGet(a2),
                                                       ir::Binary::LT,
                                                       GetBytecode(itr->pc()));
          } else {
            // |a1| + |a3| < |a2|
            ir::Binary* addition = ir::Binary::New(node_factory_,StackGet(a1),
                                                                 StackGet(a3),
                                                                 ir::Binary::ADD,
                                                                 GetBytecode(itr->pc()));

            comparison = ir::Binary::New(node_factory_,addition,StackGet(a2),
                                                                ir::Binary::LT,
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

    // patch all the pending continue and break node
  }
}

GraphBuilder::StopReason BuildBranchBlock( BytecodeIterator* itr ,
                                           const std::uint32_t* pc ) {
  for( ; itr->HasNext(); itr->Next() ) {
    if(pc == itr->pc()) return STOP_END;

    if(itr->opcode() == BC_JUMP)
      return STOP_JUMP;

    BuildBytecode(itr);
  }
  return STOP_EOF;
}

GraphBuilder::StopReason
GraphBuilder::BuildBranch( BytecodeIterator* itr ) {
  lava_debug(NORMAL,lava_verify(itr->opcode() == BC_JMPF););

  // skip the BC_JMPF
  lava_verify(itr->Next(););

  std::uint8_t cond; std::uint16_t dest;
  itr->GetOperand(&cond,&dest);

  // create the leading If node
  ir::If* if_region = ir::Region::New(node_factory_,StackGet(cond),region());

  // switch to the if_region
  set_region(if_region);

  // merged control flow node
  ir::Region* false_region;
  const std::uint32_t* false_pc;
  bool need_merge = false;
  std::size_t final_cursor;

  // 1. Build code inside of the *false* branch at first
  {
    std::size_t old_cursor = itr->cursor();
    itr->BranchTo(dest);   // branch to the *false* branch
    false_pc = itr->pc();  // false branch pc start

    ir::ControlFlow* old_region = region();
    false_region = ir::Region::New(node_factory_,old_region);
    set_region( false_region );

    StopReason reason = BuildBranchBlock(itr,NULL);
    need_merge = (reason == STOP_JUMP);

    final_cursor = itr->cursor();
    itr->BranchTo(old_cursor);
    set_region(old_region);
  }

  // 2. Build code inside of the *true* branch
  ir::Region* true_region;
  {
    true_region = ir::Region::New(node_factory_,region());
    set_region(true_region);

    // enable PHI insertion
    set_rhs_branch(true);

    StopReason reason = BuildBranchBlock(itr,false_pc);
    lava_debug(NORMAL,
        if(reason == STOP_END)
          lava_verify(false_pc == itr->pc()););

    // disable PHI insertion
    set_rhs_branch(false);

    // We may have new true_region since BuildUntil can add new
    // region on the fly due to loop/continue/break
    true_region = region();
  }

  // 3. create merge node if we need to
  if(need_merge) {
    set_region(ir::Region::New(node_factory_,true_region,false_region));
  } else {
    false_region->AddBackwardEdge(zone_,true_region);
    set_region(false_region);
  }

  itr->BranchTo(final_cursor);
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
      lava_debug(NORMAL,lava_verify(func_info().has_loop());); // Must be inside of a loop body
      {
        std::uint16_t pc;
        itr.GetArgument(&pc);

        ir::Jump* jump = ir::Jump::New(node_factory_,region(),GetBytecodeInfo(itr->pc()));

        // modify the current region
        set_region(jump);

        if(itr.opcode() == BC_BRK)
          func_info().cur_loop().AddBreak(jump,pc);
        else
          func_info().cur_loop().AddContinue(jump,pc);
      }
      break;
  }
}

GraphBuilder::StopReason
GraphBuilder::BuildUntil( BytecodeIterator* itr , const std::uint32_t* end_pc ) {
  for( ; itr->HasNext() ; itr->Next() ) {
    if(itr->code_position() == end_pc)
      return STOP_END;
    StopReason reason = BuildBytecode(itr);
  }
}


} // namespace
} // namespace cbase
} // namespace lavascript
