#include "graph.h"

#include "dest/interpreter/bytecode.h"
#include "dest/interpreter/bytecode-iterator.h"

#include <vector>
#include <set>

namespace lavascript {
namespace cbase {
using namespace ::lavascript::interpreter;

namespace {

// Temporarily change GraphBuilderContext inside of GraphBuilder due
// to *inline* The GraphBuilderContext will be poped out inside of
// the destructor
class InlineContext {
 public:
  BranchContext( GraphBuilder* );
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
  enum StopReason {
    STOP_JUMP,
    STOP_EOF ,
    STOP_END ,
    STOP_FAIL,
    STOP_DONE
  };

  template< bool STOP_AT_JUMP = false >
  StopReason BuildBlock( BytecodeIterator* itr , const std::uint32_t* end_pc = NULL );

  // Building branch IR graph
  StopReason BuildBranch( BytecodeIterator* itr );

  // Building logical IR graph
  StopReason BuildLogic ( BytecodeIterator* itr );

 private:
  zone::Zone* zone_;
  ir::NodeFactory* node_factory_;
  ir::Start* start_;
  ir::End*   end_;

  const std::uint32_t* osr_;

  // Working set data , used when doing inline and other stuff
  std::vector<ir::Expr*> stack_;

  struct FuncInfo {
    Handle<Closure> closure;
    ir::ControlFlow* region;
    std::uint32_t base;
    std::uint8_t  max_local_var_size;
    bool rhs_branch;

    bool IsLocalVar( std::uint8_t slot ) const {
      return slot < max_local_var_size;
    }
  };
  std::vector<FuncInfo> func_info_;
};

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

GraphBuilder::StopReason
GraphBuilder::BuildBranch( BytecodeIterator* itr ) {
  lava_debug(NORMAL,lava_verify(itr->opcode() == BC_JMPF););

  // skip the BC_JMPF
  lava_verify(itr->Next(););

  std::uint8_t cond; std::uint16_t dest;
  itr->GetOperand(&cond,&dest);

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

    StopReason reason = BuildBlock<true>(itr,NULL);
    if(reason == STOP_JUMP) {
      need_merge = true;
    }

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

    StopReason reason = BuildBlock<true>(itr,false_pc);
    lava_debug(NORMAL,
        if(reason == STOP_END)
          lava_verify(false_pc == itr->pc()););

    // disable PHI insertion
    set_rhs_branch(false);
  }

  // 3. create merge node if we need to
  if(need_merge) {
    set_region(ir::Region::New(node_factory_,true_region,false_region));
  } else {
    false_region->backedge()->Add(zone_,true_region);
    set_region(false_region);
  }

  itr->BranchTo(final_cursor);
  return STOP_DONE;
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
    StopReason reason = BuildBlock<false>(itr,end_pc);
    lava_verify(reason == STOP_END);

    // note for the false and true region order
    fallthrough_region->AddBackwardEdge(false_region);
    fallthrough_region->AddBackwardEdge(region());

    false_node = saved;
    true_node  = StackGet(reg);
  } else {
    ir::Region* true_region = ir::Region::New(node_factory_,region());

    lava_verify(itr->Next());
    StopReason reason = BuildBlock<false>(itr,end_pc);
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
  return STOP_DONE;
}

template< bool STOP_AT_JUMP >
GraphBuilder::StopReason
GraphBuilder::BuildBlock( BytecodeIterator* itr , const std::uint32_t* end_pc ) {
  for( ; itr->HasNext() ; itr->Next() ) {

    if(itr->code_position() == end_pc)
      return STOP_END;

    switch(itr->opcode()) {
      /* binary arithmetic/comparison */
      case BC_ADDRV: case BC_SUBRV: case BC_MULRV: case BC_DIVRV: case BC_MODRV: case BC_POWRV:
      case BC_LTRV:  case BC_LERV : case BC_GTRV : case BC_GERV : case BC_EQRV : case BC_NERV :
        {
          std::uint8_t dest , a1, a2;
          itr->GetOperand(&dest,&a1,&a2);
          StackSet(dest,ir::Binary::New(node_factory_,NewNumber(a1),
                                                      StackGet(a2),
                                                      ir::Binary::BytecodeToOperator(itr->opcode()),
                                                      GetBytecodeInfo(dest,itr->pc())),
                                                      itr->pc());
        }
        break;
      case BC_ADDVR: case BC_SUBVR: case BC_MULVR: case BC_DIVVR: case BC_MODVR: case BC_POWVR:
      case BC_LTVR : case BC_LEVR : case BC_GTVR : case BC_GEVR : case BC_EQVR : case BC_NEVR :
        {
          std::uint8_t dest , a1, a2;
          itr->GetOperand(&dest,&a1,&a2);
          StackSet(dest,ir::Binary::New(node_factory_,StackGet(a1),
                                                      NewNode(a2),
                                                      ir::Binary::BytecodeToOperator(itr->opcode()),
                                                      GetBytecodeInfo(dest,itr->pc())),
                                                      itr->pc());
        }
        break;
      case BC_ADDVV: case BC_SUBVV: case BC_MULVV: case BC_DIVVV: case BC_MODVV: case BC_POWVV:
      case BC_LTVV : case BC_LEVV : case BC_GTVV : case BC_GEVV : case BC_EQVV : case BC_NEVV :
        {
          std::uint8_t dest, a1, a2;
          itr->GetOperand(&dest,&a1,&a2);
          StackSet(dest,ir::Binary::New(node_factory_,StackGet(a1),
                                                      StackGet(a2),
                                                      ir::Binary::BytecodeToOperator(itr->opcode()),
                                                      GetBytecodeInfo(dest,itr->pc())),
                                                      itr->pc());
        }
        break;
      case BC_EQSV: case BC_NESV:
        {
          std::uint8_t dest, a1, a2;
          itr->GetOperand(&dest,&a1,&a2);
          StackSet(dest,ir::Binary::New(node_factory_,NewString(a1),
                                                      StackGet(a2),
                                                      ir::Binary::BytecodeToOperator(itr->opcode()),
                                                      GetBytecodeInfo(dest,itr->pc())),
                                                      itr->pc());
        }
        break;
      case BC_EQVS: case BC_NEVS:
        {
          std::uint8_t dest, a1, a2;
          itr->GetOperand(&dest,&a1,&a2);
          StackSet(dest,ir::Binary::New(node_factory_,StackGet(a1),
                                                      NewString(a2),
                                                      ir::Binary::BytecodeToOperator(itr->opcode()),
                                                      GetBytecodeInfo(dest,itr->pc())),
                                                      itr->pc());
        }
        break;
      /* unary operation */
      case BC_NEGATE: case BC_NOT:
        {
          std::uint8_t dest, src;
          itr->GetOperand(&dest,&src);
          StackSet(dest,ir::Unary::New(node_factory_,StackGet(src),
                                                     ir::Unary::BytecodeToOperator(itr->opcode()),
                                                     GetBytecodeInfo(dest,itr->pc())),
                                                     itr->pc());
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
      case BC_JMP:
        if(STOP_AT_JUMP) return STOP_JUMP;

        {
          std::uint8_t dest;
          itr->GetOperand(&dest);
          itr->BranchTo(dest);
          ir::Region* region = ir::Region::New(node_factory_,region(),GetBytecodeInfo(dest,itr));
          func_info().region = region; // set it to new region
        }
        break;
    }
}


} // namespace
} // namespace cbase
} // namespace lavascript
