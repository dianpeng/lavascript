#include "graph.h"

#include "dest/interpreter/bytecode.h"
#include "dest/interpreter/bytecode-iterator.h"

#include <vector>

namespace lavascript {
namespace cbase {
using namespace ::lavascript::interpreter;

namespace {

// Used to simulate multiple branch/target and loop
struct GraphBuilderContext {
  std::vector<ir::Node*> stack;
  std::uint32_t base;
  ir::Node* region;
};

// Temporarily change GraphBuilderContext inside of GraphBuilder due
// to we need to d a branch scope IR building. This context adding is
// temporarily and will be merged back inside of its destructor
class BranchContext {
 public:
  BranchContext( GraphBuilder* );
  ~BranchContext();

 private:
  GraphBuilderContext* context_;
  GraphBuilder* builder_;
};

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
                                   const Handle<Script> & script  ,
                                   const std::uint32_t* osr ,
                                   std::uint32_t stack_base = 0 ):
    zone_        (zone),
    closure_     (closure),
    script_      (script),
    osr_         (osr),
    start_       (NULL),
    end_         (NULL),
    stack_base_  () ,
    cur_ctl_     ()
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

  void StackSet( std::uint32_t index , ir::Node* value ) {
    return current_context().stack[ current_context().base + index ] = value;
  }

  ir::Node* StackGet( std::uint32_t index ) {
    return &(current_context().stack[ current_context().base + index ]);
  }

  std::uint32_t StackIndex( std::uint32_t index ) const {
    return index + current_context().base;
  }

 private: // Region
  ir::Node* region() { return current_context().region; }

 private: // Constant handling
  ir::Node* NewConstNumber( std::int32_t );

  ir::Node* NewNumber( std::uint8_t ref );
  ir::Node* NewString( std::uint8_t ref );
  ir::Node* NewSSO   ( std::uint8_t ref );

 private:
  bool BuildBlock();

 private:
  zone::Zone* zone_;
  Handle<Closure> closure_;
  Handle<Script>  script_;
  const std::uint32_t* osr_;
  ir::ConstantFactory* const_factory_;
  ir::Start* start_;
  ir::End*   end_;

  // Working set data , used when doing inline and other stuff
  std::vector<std::uint32_t> stack_base_;
  std::vector<GraphBuilderContext> context_;
};


bool GraphBuilder::BuildBlock() {
  Handle<Prototype> proto(closure_->prototype());
  BytecodeIterator itr( proto->code_buffer() , proto->code_buffer_size() );
  for( ; itr.HasNext() ; itr.Next() ) {
    switch(itr.opcode()) {
      /* binary arithmetic/comparison */
      case BC_ADDRV: case BC_SUBRV: case BC_MULRV: case BC_DIVRV: case BC_MODRV: case BC_POWRV:
      case BC_LTRV:  case BC_LERV : case BC_GTRV : case BC_GERV : case BC_EQRV : case BC_NERV :
        {
          std::uint8_t dest , a1, a2;
          itr.GetOperand(&dest,&a1,&a2);
          StackSet(dest,ir::Binary::New(
                zone_,NewNumber(a1),StackGet(a2),ir::Binary::BytecodeToOperator(itr.opcode()),
                                                 ir::BytecodeInfo(StackIndex(dest),itr.code_position())));
        }
        break;
      case BC_ADDVR: case BC_SUBVR: case BC_MULVR: case BC_DIVVR: case BC_MODVR: case BC_POWVR:
      case BC_LTVR : case BC_LEVR : case BC_GTVR : case BC_GEVR : case BC_EQVR : case BC_NEVR :
        {
          std::uint8_t dest , a1, a2;
          itr.GetOperand(&dest,&a1,&a2);
          StackSet(dest,ir::Binary::New(
                zone_,StackGet(a1),NewNode(a2),ir::Binary::BytecodeToOperator(itr.opcode()),
                                               ir::BytecodeInfo(StackIndex(dest),itr.code_position())));
        }
        break;
      case BC_ADDVV: case BC_SUBVV: case BC_MULVV: case BC_DIVVV: case BC_MODVV: case BC_POWVV:
      case BC_LTVV : case BC_LEVV : case BC_GTVV : case BC_GEVV : case BC_EQVV : case BC_NEVV :
        {
          std::uint8_t dest, a1, a2;
          itr.GetOperand(&dest,&a1,&a2);
          StackSet(dest,ir::Binary::New(
                zone_,StackGet(a1),StackGet(a2),ir::Binary::BytecodeToOperator(itr.opcode()),
                                                ir::BytecodeInfo(StackIndex(dest),itr.code_position())));
        }
        break;
      case BC_EQSV: case BC_NESV:
        {
          std::uint8_t dest, a1, a2;
          itr.GetOperand(&dest,&a1,&a2);
          StackSet(dest,ir::Binary::New(
                zone_,NewString(a1),StackGet(a2),ir::Binary::BytecodeToOperator(itr.opcode()),
                                                 ir::BytecodeInfo(StackIndex(dest),itr.code_position())));
        }
        break;
      case BC_EQVS: case BC_NEVS:
        {
          std::uint8_t dest, a1, a2;
          itr.GetOperand(&dest,&a1,&a2);
          StackSet(dest,ir::Binary::New(
                zone_,StackGet(a1),NewString(a2),ir::Binary::BytecodeToOperator(itr.opcode()),
                                                 ir::BytecodeInfo(StackIndex(dest),itr.code_position())));
        }
        break;
      /* unary operation */
      case BC_NEGATE: case BC_NOT:
        {
          std::uint8_t dest, src;
          itr.GetOperand(&dest,&src);
          StackSet(dest,
              ir::Unary::New(zone_,StackGet(src),ir::Unary::BytecodeToOperator(itr.opcode()),
                ir::BytecodeInfo(StackIndex(dest),itr.code_position())));
        }
        break;
      /* move */
      case BC_MOVE:
        {
          std::uint8_t desst,src;
          itr.GetOperand(&dest,&src);
          StackSet(dest,StackGet(src));
        }
        break;
      /* loading */
      case BC_LOAD0: case BC_LOAD1: case BC_LOADN1:
        {
          std::uint8_t dest;
          itr.GetOperand(&dest);
          std::int32_t num = 0;
          if(itr.opcode() == BC_LOAD1)
            num = 1;
          else if(itr.opcode() == BC_LOADN1)
            num = -1;
          StackSet(dest,NewConstNumber(num));
        }
        break;
      case BC_LOADR:
        {
          std::uint8_t dest,src;
          itr.GetOperand(&dest,&src);
          StackSet(dest,NewNumber(src));
        }
        break;
      case BC_LOADSTR:
        {
          std::uint8_t dest,src;
          itr.GetOperand(&dest,&src);
          StackSet(dest,NewString(src));
        }
        break;
      case BC_LOADTRUE: case BC_LOADFALSE:
        {
          std::uint8_t dest;
          itr.GetOperand(&dest);
          StackSet(dest,NewBoolean(itr.opcode() == BC_LOADTRUE));
        }
        break;
  }
}


} // namespace
} // namespace cbase
} // namespace lavascript