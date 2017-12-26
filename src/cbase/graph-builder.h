#ifndef CBASE_GRAPH_H_
#define CBASE_GRAPH_H_
#include "ir.h"

#include "src/objects.h"
#include "src/zone.h"

namespace lavascript {
namespace cbase {
namespace ir    {

// A graph builder. It is responsible for building :
//   1) normal function with main entry
//   2) function with OSR entry , this will just compile code in that nested loop tree
class GraphBuilder {
  class  FuncInfo;
  struct LoopInfo;
  struct LoopJump;
 public:
  typedef std::vector<Expr*> ValueStack;

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
  inline void StackSet( std::uint32_t index , Expr* value );

  void StackReset( std::uint32_t index ) {
    stack_[StackIndex(index)] = NULL;
  }

  Expr* StackGet( std::uint32_t index ) {
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

  ControlFlow* region() const {
    return func_info().region;
  }

  void set_region( ControlFlow* new_region ) {
    func_info().region = new_region;
  }

 private: // Constant handling
  Expr* NewConstNumber( std::int32_t , const std::uint32_t* pc );
  Expr* NewNumber     ( std::uint8_t ref  , const std::uint32_t* pc );
  Expr* NewString     ( std::uint8_t ref  , const std::uint32_t* pc );
  Expr* NewSSO        ( std::uint8_t ref  , const std::uint32_t* pc );
  Expr* NewBoolean    ( bool , const std::uint32_t* pc );

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
  StopReason GotoIfEnd( BytecodeIterator* , const std::uint32_t* );
  StopReason BuildIf( BytecodeIterator* itr );
  StopReason BuildIfBlock( BytecodeIterator* , const std::uint32_t* );

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
  void GenerateLoopPhi();
  void PatchLoopPhi();

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

  IRInfo* NewIRInfo( const std::uint32_t* pc );

 private:
  zone::Zone* zone_;
  Graph*      graph_;
  Start*      start_;
  End*        end_;
  // Working set data , used when doing inline and other stuff
  ValueStack* stack_;
  std::vector<FuncInfo> func_info_;

  class LoopScope;
  class BasicBlockScope;
  class BackupStack;
  friend class LoopScope;
  friend class BackupStack;
};

inline GraphBuilder::StackSet( std::uint32_t index , Expr* node ) {
  stack_[StackIndex(index)] = node;
}

} // namespace ir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_GRAPH_H_
