#include "graph.h"

#include "dest/interpreter/bytecode.h"
#include "dest/interpreter/bytecode-iterator.h"

#include <vector>
#include <set>
#include <unordered_map>

namespace lavascript {
namespace cbase {
using namespace ::lavascript::interpreter;

namespace {
class GraphBuilder;

/**
 * Bytecode liveness analyze.
 *
 * We will do a simple bytecode analyze before we start do the IR graph
 * construction due to the fact we need to let this analyze to figure out
 * the local variable / register slot assignment for each bytecode. This
 * is used when constructing PHI node inside of the loop
 */

class BytecodeLiveness {
 public:
  BytecodeLiveness( const Handle<Prototype>& proto ) :
    map_(),
    proto_(proto),
    max_local_var_size_(proto_->max_local_var_size())
  {}

 public:
  // Construct the bytecode lieveness with certain iterator
  void BuildLiveness();
 private:
  // build the liveness for a single bytecode; bytecode cannot be control
  // flow transfer
  void BuildBytecode( BytecodeIterator* , LivenessSet* );
  void BuildBlock   ( BytecodeIterator* , const LivenessSet* prev );


  void BuildUntilJump( BytecodeIterator* itr , const std::uint32_t* pc );

  void BuildBranch  ( BytecodeIterator* , const LivenessSet* prev );
  void BuildLogic   ( BytecodeIterator* , const LivenessSet* prev );
  void BuildLoop    ( BytecodeIterator* , const LivenessSet* prev );

  bool IsLocalVar   ( std::uint8_t reg ) const {
    return reg < max_local_var_size_;
  }

  LivenessMap* AddNewScope( const void* pc , const LivenessSet* prev ) {
    std::pair<LivenessMap::iterator,bool> ret =
      map_.insert( std::make_pair(pc,LivenessMap(prev)) );
    lava_debug(NORMAL,lava_verify(ret.second););
    return &(ret.first.second);
  }
 private:
  struct LivenessSet {
    void Add( std::uint8_t reg ) { register_set.insert(reg); }
    bool IsAlive( std::uint8_t reg );

    // instead of duplicate all the liveness information into the
    // nested scope, we just chain them based on this *pointer*
    const LivenessSet* prev;
    const std::uint32_t*   end;
    std::set<std::uint8_t> register_set;

    LivenessSet( const LivenessSet* p ):
      prev(p),
      end (NULL),
      register_set()
    {}
  };

  // bytecode address to LivenessSet mapping. This map only contains
  // top level function's liveness information for each bytecode and
  // also it is definitly not the most coarsed one since we have no
  // CFG and we cannot do a backward analyze
  //
  // The pc is the jump that starts a new basic block. NOTES: there're
  // no CFG that is established
  typedef std::unordered_map<const void*,LivenessSet> LivenessMap;
  LivenessMap map_;

  Handle<Prototype> proto_;
  std::uint8_t max_local_var_size_;
};

// build the liveness against basic block
void BytecodeLiveness::BuildBlock( BytecodeIterator* itr , const LivenessSet* prev ) {
  LivenessSet* set = AddNewScope(itr->pc(),prev);

  for( ; itr->HasNext() ; itr->Next() ) {
    switch(itr->opcode()) {
      /** all are control flow instruction/bytecode **/
      case BC_JMPF: BuildBranch(itr,set); break;
      case BC_AND:  BuildLogic (itr,set); break;
      case BC_OR:   BuildLogic (itr,set); break;
      case BC_FEVRSTART:
      case BC_FESTART:
      case BC_FSTART:
        BuildForeverLoop(itr);
        break;

      case BC_CONT:
      case BC_BRK:
      case BC_RET:
      case BC_RETNULL:
        break; /* end of the basic block */
      default:
        BuildBytecode(itr,set);
    }
  }
}

void BytecodeLiveness::BuildBranch( BytecodeIterator* itr ) {
}

void BytecodeLiveness::BuildBytecode( BytecodeIterator* itr , LivenessSet* set ) {
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
        if(IsLocalVar(a1)) set->Add(a1);
      }
      break;
    case BC_NEGATE: case BC_NOT: case BC_MOVE:
      {
        std::uint8_t a1,a2;
        itr->GetOperand(&a1,&a2);
        if(IsLocalVar(a1)) set->Add(a1);
      }
      break;
    case BC_LOAD0: case BC_LOAD1: case BC_LOADN1:
    case BC_LOADTRUE: case BC_LOADFALSE: case BC_LOADNULL:
      {
        std::uint8_t a1; itr->GetOperand(&a1);
        if(IsLocalVar(a1)) set->Add(a1);
      }
      break;
    case BC_LOADR: case BC_LOADSTR:
      {
        std::uint8_t a1,a2; itr->GetOperand(&a1,&a2);
        if(IsLocalVar(a1)) set->Add(a1);
      }
      break;
    case BC_LOADLIST0: case BC_LOADOBJ0:
      {
        std::uint8_t a1; itr->GetOperand(&a1);
        if(IsLocalVar(a1)) set->Add(a1);
      }
      break;
    case BC_LOADLIST1:
      {
        std::uint8_t a1,a2;
        itr->GetOperand(&a1,&a2);
        if(IsLocalVar(a1)) set->Add(a1);
      }
      break;
    case BC_LOADLIST2: case BC_LOADOBJ1:
      {
        std::uint8_t a1,a2,a3;
        itr->GetOperand(&a1,&a2,&a3);
        if(IsLocalVar(a1)) set->Add(a1);
      }
      break;
    case BC_NEWLIST: case BC_NEWOBJ:
      {
        std::uint8_t a1; std::uint16_t a2;
        itr->GetOperand(&a1,&a2);
        if(IsLocalVar(a1)) set->Add(a1);
      }
      break;
    case BC_LOADCLS:
      {
        std::uint8_t a1; std::uint16_t a2;
        itr->GetOperand(&a1,&a2);
        if(IsLocalVar(a1)) set->Add(a1);
      }
      break;
    case BC_PROPGET:
    case BC_PROPGETSSO:
    case BC_IDXGET:
    case BC_IDXGETI:
      {
        std::uint8_t a1,a2,a3;
        itr->GetOperand(&a1,&a2,&a3);
        if(IsLocalVar(a1)) set->Add(a1);
      }
      break;

    case BC_UVGET: case BC_GGET:
      {
        std::uint8_t a1; std::uint16_t a2;
        itr->GetOperand(&a1,&a2);
        if(IsLocalVar(a1)) set->Add(a1);
      }
      break;

    case BC_GGETSSO:
      {
        std::uint8_t a1 , a2;
        itr->GetOperand(&a1,&a2);
        if(IsLocalVar(a1)) set->Add(a1);
      }
      break;

    case BC_IDREF:
      {
        std::uint8_t a1 , a2, a3;
        itr->GetOperand(&a1,&a2);
        if(IsLocalVar(a2)) set->Add(a2);
        if(IsLocalVar(a3)) set->Add(a3);
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

    default:
      lava_unreachF("cannot reach here, bytecode %s",itr->opcode_name());
      break;
  }
}



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
  bool gen_phi;
  std::vector<LoopInfo> loop_info;

 public:
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

  bool gen_phi() const {
    return func_info().gen_phi;
  }

  void set_gen_phi( bool b ) {
    func_info().gen_phi = b;
  }

 private: // Constant handling
  ir::Expr* NewConstNumber( std::int32_t );
  ir::Expr* NewNumber( std::uint8_t ref );
  ir::Expr* NewString( std::uint8_t ref );
  ir::Expr* NewSSO   ( std::uint8_t ref );

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

  StopReason BuildBlock( BytecodeIterator* itr , const std::uint32_t* end_pc = NULL );

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
  if(gen_phi()) {
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
  } else {
    stack_[StackIndex(index)] = node;
  }
}

GraphBuilder::StopReason GraphBuilder::BuildLogic( BytecodeIterator* itr ) {
  lava_debug(NORMAL,lava_verify(itr->opcode() == BC_AND || itr->opcode() == BC_OR););
  bool op_and = itr->opcode() == BC_AND ? true : false;
  std::uint8_t reg; std::uint16_t offset;
  itr->GetOperand(&reg,&offset);

  // where we should end for the other part of the logical cominator
  const std::uint32_t* end_pc = itr->code_buffer() + pc;

  // save the fallthrough value for PHI node later on
  ir::Expr* saved = StackGet(reg);
  ir::Expr* false_node , *true_node;

  lava_debug(NORMAL,StackReset(reg););

  ir::Region* fallthrough_region = ir::Region::New(node_factory_);

  if(!op_and) {
    ir::Region* false_region = ir::Region::New(node_factory_,region());

    lava_verify(itr->Next());
    StopReason reason = BuildBlock(itr,end_pc);
    lava_verify(reason == STOP_END);

    // note for the false and true region order
    fallthrough_region->AddBackwardEdge(false_region);
    fallthrough_region->AddBackwardEdge(region());

    false_node = saved;
    true_node  = StackGet(reg);
  } else {
    ir::Region* true_region = ir::Region::New(node_factory_,region());

    lava_verify(itr->Next());
    StopReason reason = BuildBlock(itr,end_pc);
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

  if(itr->opcode() == BC_FEVRSTART)
    return BuildForeverLoop(itr);

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
  } else {
    std::uint8_t a1;
    itr->GetArgument(&a1,&after_pc);
    // create ir ItrNew which basically initialize the itr and also do
    // a test against the iterator to see whether it is workable
    ir::ItrNew* inew = ir::ItrNew::New(node_factory_,StackGet(a1),
                                                     GetBytecode(pc->itr()));
    loop_header = ir::If::New(node_factory_,inew,region());
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
    func_info().set_gen_phi(true);

    StopReason reason = BuildLoopBlock(itr);
    lava_debug(NORMAL, lava_verify(reason == STOP_SUCCESS || reason == STOP_JUMP););

    func_info().set_gen_phi(false);

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

GraphBuilder::StopReason BuildBranchBlock( BytecodeIterator* itr ,
                                           const std::uint32_t* pc ) {
  for( ; itr->HasNext(); itr->Next() ) {
    // check whether we reache end of PC where we suppose to stop
    if(pc == itr->pc()) return STOP_END;

    // check whether we have a unconditional jump or not
    if(itr->opcode() == BC_JUMP) return STOP_JUMP;

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
    set_gen_phi(true);

    StopReason reason = BuildBranchBlock(itr,false_pc);
    lava_debug(NORMAL,
        if(reason == STOP_END)
          lava_verify(false_pc == itr->pc()););

    // disable PHI insertion
    set_gen_phi(false);

    // We may have new true_region since BuildBlock can add new
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
GraphBuilder::BuildBlock( BytecodeIterator* itr , const std::uint32_t* end_pc ) {
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
