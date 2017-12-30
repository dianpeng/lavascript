#ifndef CBASE_GRAPH_BUILDER_H_
#define CBASE_GRAPH_BUILDER_H_
#include "ir.h"
#include "src/objects.h"
#include "src/zone/zone.h"

#include <cstdint>
#include <vector>

namespace lavascript {
namespace cbase {
namespace ir    {

typedef std::vector<Expr*> ValueStack;

// -------------------------------------------------------------------------------------
// This is a HIR/MIR graph consturction , the LIR is essentially a traditionaly CFG
// The graph is a sea of nodes style and it is responsible for all optimization before
// scheduling
//
// The builder can build 1) normal function call 2) OSR style function IR
class GraphBuilder {
 public:
  // Stack to simulate all the register status while building the IR graph

 private:
  // Data structure record the pending jump that happened inside of the loop
  // body. It is typically break and continue keyword , since they cause a
  // unconditional jump happened
  struct UnconditionalJump {
    Jump* node;                     // node that is created when break/continue jump happened
    const std::uint32_t* pc;        // address of this unconditional jump
    ValueStack stack;               // stack results for this branch , used to generate PHI node
    UnconditionalJump( Jump* n , const std::uint32_t* p , const ValueStack& stk ):
      node(n),
      pc  (p),
      stack(stk)
    {}
  };

  typedef std::vector<UnconditionalJump> UnconditionalJumpList;

  // Hold information related IR graph during a loop is constructed. This is
  // created on demand and pop out when loop is constructed
  struct LoopInfo {
    // all pending break happened inside of this loop
    UnconditionalJumpList pending_break;

    // all pending continue happened inside of this loop
    UnconditionalJumpList pending_continue;

    // a pointer points to a LoopHeaderInfo object
    const BytecodeAnalyze::LoopHeaderInfo* loop_header_info;

    // pending PHIs in this loop's body
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
    void AddBreak( Jump* node , const std::uint32_t* target , const ValueStack& stk ) {
      pending_break.push_back(UnconditionalJump(node,target,stk));
    }

    void AddContinue( Jump* node , const std::uint32_t* target , const ValueStack& stk ) {
      pending_continue.push_back(UnconditionalJump(node,target,stk));
    }

    void AddPhi( std::uint8_t index , Phi* phi ) {
      phi_list.push_back(PhiVar(index,phi));
    }

    LoopInfo( const BytecodeAnalyze::LoopHeaderInfo* info ):
      pending_break(),
      pending_continue(),
      loop_header_info(info),
      phi_list()
    {}
  };

  // Structure to record function level information when we do IR construction.
  // Once a inline happened, then we will push a new FuncInfo object into func_info
  // stack/vector
  struct FuncInfo {
    /** member field **/
    Handle<Closure> closure;
    Handle<Prototype> prototype; // cached for faster access
    ControlFlow* region;
    std::uint32_t base;
    std::uint8_t  max_local_var_size;
    std::vector<LoopInfo> loop_info;
    std::vector<Return* > return_list;
    BytecodeAnalyze bc_analyze;

   public:
    inline FuncInfo( const Handle<Closure>& , ControlFlow* , std::uint32_t );
    inline FuncInfo( FuncInfo&& );

    bool IsLocalVar( std::uint8_t slot ) const {
      return slot < max_local_var_size;
    }

    // check whether we have loop currently
    bool HasLoop() const { return !loop_info.empty(); }

    // get the current loop
    GraphBuilder::LoopInfo& current_loop() {
      return loop_info.back();
    }

    const GraphBuilder::LoopInfo& current_loop() const {
      return loop_info.back();
    }

    const BytecodeAnalyze::LoopHeaderInfo* current_loop_header() const {
      return current_loop().loop_header_info;
    }

   public: // Loop related stuff

    // Enter into a new loop scope, the corresponding basic block
    // information will be added into stack as part of the loop scope
    inline void EnterLoop( const std::uint32_t* pc );
    void LeaveLoop() { loop_info.pop_back(); }
  };

 public:
  GraphBuilder( zone::Zone* zone , const Handle<Script>& script ):
    zone_        (zone),
    script_      (script),
    graph_       (NULL),
    stack_       () ,
    func_info_   ()
  {}

  // Build a normal function's IR graph
  bool Build( const Handle<Closure>& , Graph* );

 private: // Stack accessing
  std::uint32_t StackIndex( std::uint32_t index ) const {
    return func_info().base+index;
  }

  void StackSet( std::uint32_t index , Expr* value ) {
    stack_->at(StackIndex(index)) = value;
  }

  void StackReset( std::uint32_t index ) {
    stack_->at(StackIndex(index)) = NULL;
  }

  Expr* StackGet( std::uint32_t index ) {
    return stack_->at(StackIndex(index));
  }
 private: // Current FuncInfo
  FuncInfo& func_info() { return func_info_.back(); }
  const FuncInfo& func_info() const { return func_info_.back(); }

  bool IsTopFunction() const { return func_info_.size() == 1; }

  const Handle<Closure>& closure() const {
    return func_info().closure;
  }

  const Handle<Prototype>& prototype() const {
    return func_info().prototype;
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
  Expr* NewConstNumber( std::int32_t , const interpreter::BytecodeLocation& );
  Expr* NewConstNumber( std::int32_t );

  Expr* NewNumber     ( std::uint8_t , IRInfo* );
  Expr* NewNumber     ( std::uint8_t , const interpreter::BytecodeLocation& );
  Expr* NewNumber     ( std::uint8_t );

  Expr* NewString     ( std::uint8_t , IRInfo* );
  Expr* NewString     ( std::uint8_t , const interpreter::BytecodeLocation& );
  Expr* NewString     ( std::uint8_t );

  Expr* NewSSO        ( std::uint8_t , const interpreter::BytecodeLocation& );
  Expr* NewSSO        ( std::uint8_t );

  Expr* NewBoolean    ( bool         , const interpreter::BytecodeLocation& );
  Expr* NewBoolean    ( bool );

 private:
  // Build routine's return status code
  enum StopReason {
    STOP_BAILOUT = -1,   // the target is too complicated to be jitted
    STOP_JUMP,
    STOP_EOF ,
    STOP_END ,
    STOP_SUCCESS
  };

  // Just build *one* BC isntruction , this will not build certain type of BCs
  // since it is expected other routine to consume those BCs
  StopReason BuildBytecode( interpreter::BytecodeIterator* itr );

  StopReason BuildBasicBlock( interpreter::BytecodeIterator* itr , const std::uint32_t* end_pc = NULL );

  // Build branch IR graph
  void InsertIfPhi( ValueStack* dest , const ValueStack& false_stack ,
                                       const ValueStack& true_stack ,
                                       ControlFlow* ,
                                       const interpreter::BytecodeLocation& pc );

  StopReason GotoIfEnd( interpreter::BytecodeIterator* , const std::uint32_t* );
  StopReason BuildIf( interpreter::BytecodeIterator* itr );
  StopReason BuildIfBlock( interpreter::BytecodeIterator* , const std::uint32_t* );

  // Build logical IR graph
  StopReason BuildLogic ( interpreter::BytecodeIterator* itr );

  // Build ternary IR graph
  StopReason BuildTernary( interpreter::BytecodeIterator* itr );

  // Build loop IR graph
  //
  // The core/tricky part of the loop IR is where to insert PHI due to we cannot see
  // a variable when we insert PHI in loop body. We know this information by using
  // information from BytecodeAnalyze. It will tell us which list of variables are
  // modified inside of the loop and these variables are not local variable inside of
  // the loop. So we just insert PHI before hand and at last we patch the PHI to correct
  // its *second input operand* since the modification comes later in the loop.
  StopReason BuildLoop       ( interpreter::BytecodeIterator* itr );
  StopReason BuildForeverLoop( interpreter::BytecodeIterator* itr );
  void GenerateLoopPhi       ( const interpreter::BytecodeLocation& );
  void PatchLoopPhi          ();
  // Iterate until we see FEEND/FEND1/FEND2/FEVREND
  StopReason BuildLoopBlock( interpreter::BytecodeIterator* itr );

  void InsertUnconditionalJumpPhi( const ValueStack& , ControlFlow* ,
      const interpreter::BytecodeLocation& );

  void PatchUnconditionalJump    ( UnconditionalJumpList* , ControlFlow* ,
      const interpreter::BytecodeLocation& );


 private:
  IRInfo* NewIRInfo( const interpreter::BytecodeLocation& loc );

 private:
  zone::Zone*           zone_;
  Handle<Script>        script_;
  Graph*                graph_;
  // Working set data , used when doing inline and other stuff
  ValueStack*           stack_;
  std::vector<FuncInfo> func_info_;

 private:
  class FuncScope;
  class LoopScope;
  class BackupStack;

  friend class FuncScope;
  friend class LoopScope;
  friend class BackupStack;
};

inline GraphBuilder::FuncInfo::FuncInfo( const Handle<Closure>& cls , ControlFlow* start_region ,
                                                                      std::uint32_t b ):
  closure           (cls),
  prototype         (cls->prototype()),
  region            (start_region),
  base              (b),
  max_local_var_size(cls->prototype()->max_local_var_size()),
  loop_info         (),
  return_list       (),
  bc_analyze        (cls->prototype())
{}

inline GraphBuilder::FuncInfo::FuncInfo( FuncInfo&& that ):
  closure            (that.closure),
  prototype          (that.prototype),
  region             (that.region),
  base               (that.base),
  max_local_var_size (that.max_local_var_size),
  loop_info          (std::move(that.loop_info)),
  return_list        (std::move(that.return_list)),
  bc_analyze         (std::move(that.bc_analyze))
{}

inline void GraphBuilder::FuncInfo::EnterLoop( const std::uint32_t* pc ) {
  {
    const BytecodeAnalyze::LoopHeaderInfo* info = bc_analyze.LookUpLoopHeader(pc);
    lava_debug(NORMAL,lava_verify(info););
    loop_info.push_back(LoopInfo(info));
  }
}

} // namespace ir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_GRAPH_BUILDER_H_
