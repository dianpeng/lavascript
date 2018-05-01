#ifndef CBASE_GRAPH_BUILDER_H_
#define CBASE_GRAPH_BUILDER_H_
#include "hir.h"
#include "effect.h"
#include "inliner.h"

#include "src/util.h"
#include "src/interpreter/intrinsic-call.h"
#include "src/objects.h"
#include "src/zone/zone.h"
#include "src/runtime-trace.h"

#include <cstdint>
#include <vector>
#include <map>
#include <string>
#include <memory>

namespace lavascript {
namespace cbase {
namespace hir    {

typedef std::vector<Expr*> ValueStack;

// -------------------------------------------------------------------------------------
// This is a HIR/MIR graph consturction , the LIR is essentially a traditional CFG
// The graph is a sea of nodes style and it is responsible for all optimization before
// scheduling
// The builder can build 1) normal function call 2) OSR style function IR
class GraphBuilder {
 public:
  // all intenral class forward declaration
  struct FuncInfo;

  // Environment --------------------------------------------------------------
  //
  // Track *all* program states in each lexical scope and created on the fly when
  // we enter into a new scope and merge it back when we exit the lexical scope.
  class Environment {
   public:
    struct GlobalVar {
      Str name; Expr* value;
      GlobalVar( const void* k , std::size_t l , Expr* v ): name(k,l), value(v) {}
      GlobalVar( const Str& k , Expr* v ) : name(k) , value(v) {}
      bool operator == ( const Str& k ) const { return Str::Cmp(name,k) == 0; }
    };
    typedef std::vector<GlobalVar>     GlobalMap;
    typedef std::vector<Expr*>         UpValueVector;
    typedef std::function<Expr* ()>    KeyProvider;
   public:
    Environment( GraphBuilder* );
    Environment( const Environment& );
   public:
    // init environment object from prototype object
    void EnterFunctionScope  ( const FuncInfo& );
    void PopulateArgument    ( const FuncInfo& );
    void ExitFunctionScope   ( const FuncInfo& );
    // getter/setter
    Expr*  GetUpValue( std::uint8_t );
    Expr*  GetGlobal ( const void* , std::size_t , const KeyProvider& );
    void   SetUpValue( std::uint8_t , Expr* );
    void   SetGlobal ( const void* , std::size_t , const KeyProvider& , Expr* );
    // accessor
    ValueStack*      stack()    { return &stack_;  }
    UpValueVector*   upvalue()  { return &upvalue_;}
    GlobalMap*       global()   { return &global_; }
    // effect group list
    Effect*          effect()   { return effect_.ptr(); }
    Checkpoint*      state () const { return state_;   }
    void UpdateState( Checkpoint* cp ) { state_ = cp; }
   private:
    ValueStack    stack_;     // register stack
    UpValueVector upvalue_;   // upvalue's effect group
    GlobalMap     global_;    // global's effect group
    GraphBuilder* gb_;        // graph builder
    CheckedLazyInstance<Effect> effect_; // a list of tracked effect group
    Checkpoint*   state_;     // current frame state for this environment

    friend class GraphBuilder;
    LAVA_DISALLOW_ASSIGN(Environment);
  };

  // Data structure record the pending jump that happened inside of the loop
  // body. It is typically break and continue keyword , since they cause a
  // unconditional jump happened
  struct UnconditionalJump {
    Jump* node;                     // node that is created when break/continue jump happened
    const std::uint32_t* pc;        // address of this unconditional jump
    Environment         env;        // environment for this unconditional jump
    UnconditionalJump( Jump* n , const std::uint32_t* p , const Environment& e ):node(n),pc(p),env(e){}
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
    // type of the PhiVar
    enum PhiVarType { GLOBAL , UPVALUE , VAR };
    // pending PHIs in this loop's body
    struct PhiVar {
      PhiVarType    type; // type of the var
      std::uint32_t idx;  // register index
      Phi*         phi;   // phi node
      PhiVar( PhiVarType t , std::uint32_t i , Phi* p ): type(t), idx(i), phi(p) {}
    };
    std::vector<PhiVar> phi_list;
   public:
    bool HasParentLoop() const { return loop_header_info->prev != NULL; }
    bool HasJump() const { return !pending_break.empty() || !pending_continue.empty(); }
    void AddBreak( Jump* node , const std::uint32_t* target , const Environment& e ) {
      pending_break.push_back(UnconditionalJump(node,target,e));
    }
    void AddContinue( Jump* node , const std::uint32_t* target , const Environment& e ) {
      pending_continue.push_back(UnconditionalJump(node,target,e));
    }
    void AddPhi( PhiVarType type , std::uint32_t index , Phi* phi ) {
      phi_list.push_back(PhiVar(type,index,phi));
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
    Handle<Prototype>         prototype; // cached for faster access
    ControlFlow*              region;
    std::uint32_t             base;
    std::uint8_t              max_local_var_size;
    std::vector<LoopInfo>     loop_info;
    std::vector<ControlFlow*> return_list;
    std::vector<Guard*>       guard_list ;
    BytecodeAnalyze           bc_analyze;
    const std::uint32_t*      osr_start;

   public:
    inline FuncInfo( const Handle<Prototype>& , ControlFlow* , std::uint32_t );
    inline FuncInfo( const Handle<Prototype>& , ControlFlow* , const std::uint32_t* );
    inline FuncInfo( FuncInfo&& );

    bool IsOSR                        () const { return osr_start != NULL; }
    bool IsLocalVar( std::uint8_t slot ) const { return slot < max_local_var_size; }
    // check whether we have loop currently
    bool HasLoop                      () const { return !loop_info.empty(); }
    // get the current loop
    GraphBuilder::LoopInfo& current_loop()     { return loop_info.back(); }
    const GraphBuilder::LoopInfo& current_loop() const { return loop_info.back(); }
    const BytecodeAnalyze::LoopHeaderInfo* current_loop_header() const {
      return current_loop().loop_header_info;
    }
   public: // Loop related stuff
    // Enter into a new loop scope, the corresponding basic block
    // information will be added into stack as part of the loop scope
    inline void EnterLoop( const std::uint32_t* pc );
    void        LeaveLoop() { loop_info.pop_back(); }
  };

 public:
  // Build routine's return status code
  enum StopReason {
    STOP_BAILOUT = -1,   // the target is too complicated to be jitted
    STOP_JUMP,
    STOP_EOF ,
    STOP_END ,
    STOP_SUCCESS
  };

 public:
  inline GraphBuilder( const Handle<Script>& , const RuntimeTrace& );
  // Build a normal function's IR graph
  bool Build( const Handle<Prototype>& , Graph* );
  // Build a function's graph assume OSR
  bool BuildOSR( const Handle<Prototype>& , const std::uint32_t* , Graph* );

 public: // Stack accessing
  std::uint32_t StackIndex( std::uint32_t index ) const      { return func_info().base+index; }
  void StackSet( std::uint32_t index , Expr* value )         { vstk()->at(StackIndex(index)) = value; }
  void StackReset( std::uint32_t index )                     { vstk()->at(StackIndex(index)) = NULL; }
  Expr* StackGet( std::uint32_t index )                      { return vstk()->at(StackIndex(index)); }
  Expr* StackGet( std::uint32_t index , std::uint32_t base ) { return vstk()->at(base+index); }

 public: // Current FuncInfo
  std::uint32_t method_index()         const { return static_cast<std::uint32_t>(func_info_.size()); }
  FuncInfo& func_info()                      { return func_info_.back(); }
  const FuncInfo& func_info()          const { return func_info_.back(); }
  bool IsTopFunction()                 const { return func_info_.size() == 1; }
  const Handle<Prototype>& prototype() const { return func_info().prototype; }
  std::uint32_t base()                 const { return func_info().base; }
  ControlFlow* region()                const { return func_info().region; }
  void set_region( ControlFlow* new_region ) { func_info().region = new_region; }
  Environment* env()                   const { return env_; }
  ValueStack*  vstk()                  const { return env_->stack(); }
  ValueStack*  upval()                 const { return env_->upvalue();}

 public:
  Graph*                    graph()     const { return graph_; }
  ::lavascript::zone::Zone* temp_zone()       { return &temp_zone_; }
  ::lavascript::zone::Zone* zone     () const { return zone_; }
  // input argument size , the input argument size is the argument size that
  // is belong to the top most function since this function's input argument
  // remains as input argument, rest of the nested inlined function's input
  // argument is not argument but just local variables
  std::size_t input_argument_size()    const { return func_info_.front().prototype->argument_size(); }
 private: // Constant handling
  Expr* NewConstNumber( std::int32_t );
  Expr* NewNumber     ( std::uint8_t );
  Expr* NewString     ( std::uint8_t );
  Expr* NewString     ( const Str&   );
  Str   NewStr        ( std::uint32_t ref , bool sso );
  Expr* NewSSO        ( std::uint8_t );
  Expr* NewBoolean    ( bool         );

 private: // IRList/IRObject
  IRList*   NewIRList   ( std::size_t );
  IRObject* NewIRObject ( std::size_t );

 private: // Guard handling
  // Add a type feedback with TypeKind into the stack slot pointed by index
  Expr* AddTypeFeedbackIfNeed( Expr* , TypeKind     , const interpreter::BytecodeLocation& );
  Expr* AddTypeFeedbackIfNeed( Expr* , const Value& , const interpreter::BytecodeLocation& );
  // Create a guard node and linked it back to the current graph
  Guard* NewGuard            ( Test* , Checkpoint* );
 private: // Arithmetic
  // Unary
  Expr* NewUnary            ( Expr* , Unary::Operator , const interpreter::BytecodeLocation& );
  Expr* TrySpeculativeUnary ( Expr* , Unary::Operator , const interpreter::BytecodeLocation& );
  Expr* NewUnaryFallback    ( Expr* , Unary::Operator );

  // Binary
  Expr* NewBinary           ( Expr* , Expr* , Binary::Operator ,
                                                                    const interpreter::BytecodeLocation& );
  Expr* TrySpecialTestBinary( Expr* , Expr* , Binary::Operator ,
                                                                    const interpreter::BytecodeLocation& );
  Expr* TrySpeculativeBinary( Expr* , Expr* , Binary::Operator ,
                                                                    const interpreter::BytecodeLocation& );
  Expr* NewBinaryFallback   ( Expr* , Expr* , Binary::Operator );
  // Ternary
  Expr* NewTernary( Expr* , Expr* , Expr* , const interpreter::BytecodeLocation& );
  // Intrinsic
  Expr* NewICall  ( std::uint8_t ,std::uint8_t ,std::uint8_t ,bool , const interpreter::BytecodeLocation& );
  Expr* LowerICall( ICall* , const interpreter::BytecodeLocation& );
 private: // Property/Index Get/Set
  Expr* NewPSet( Expr* , Expr* , Expr* , const interpreter::BytecodeLocation& );
  Expr* NewPGet( Expr* , Expr* , const interpreter::BytecodeLocation& );
  Expr* NewISet( Expr* , Expr* , Expr* , const interpreter::BytecodeLocation& );
  Expr* NewIGet( Expr* , Expr* , const interpreter::BytecodeLocation& );

 private: // Global variables
  void NewGGet( std::uint8_t , std::uint8_t , bool sso );
  void NewGSet( std::uint8_t , std::uint8_t , bool sso );

 private: // Upvalue
  void NewUGet( std::uint8_t , std::uint8_t );
  void NewUSet( std::uint8_t , std::uint8_t );

 private: // Checkpoint generation
  // Create a checkpoint with snapshot of current stack status
  Checkpoint* GenerateCheckpoint( const interpreter::BytecodeLocation& );
  // Create a eager checkpoint at the new *merge* region node if needed
  void GenerateMergeCheckpoint  ( const Environment& , const interpreter::BytecodeLocation& );
  // Get a init checkpoint , checkpoint that has IRInfo points to the first of the bytecode ,
  // of the top most inlined function.
  Checkpoint* InitCheckpoint ();
 private: // Helper for type trace
  // Check whether the traced type for this bytecode is the expected one wrt the index's value
  bool IsTraceTypeSame( TypeKind , std::size_t index , const interpreter::BytecodeLocation& );
 private: // Phi
  Expr* NewPhi( Expr* , Expr* , ControlFlow* );
 private: // Misc
  // Helper function to generate exit Phi node and also link the return and guard nodes to the
  // success and fail node of the HIR graph
  void PatchExitNode( ControlFlow* , ControlFlow* );
 private:
  // ----------------------------------------------------------------------------------
  // OSR compilation
  // ----------------------------------------------------------------------------------
  StopReason BuildOSRStart( const Handle<Prototype>& , const std::uint32_t* , Graph* );
  void BuildOSRLocalVariable();
  void SetupOSRLoopCondition( interpreter::BytecodeIterator* );
  StopReason BuildOSRLoop   ( interpreter::BytecodeIterator* );
  StopReason PeelOSRLoop    ( interpreter::BytecodeIterator* );
  // Build a block as OSR loop body
  StopReason GotoOSRBlockEnd( interpreter::BytecodeIterator* , const std::uint32_t* );

 private: // Build bytecode
  // Just build *one* BC isntruction , this will not build certain type of BCs
  // since it is expected other routine to consume those BCs
  StopReason BuildBytecode  ( interpreter::BytecodeIterator* itr );
  StopReason BuildBasicBlock( interpreter::BytecodeIterator* itr , const std::uint32_t* end_pc = NULL );

  // Build branch IR graph
  void InsertPhi( Environment* , Environment* , ControlFlow* );
  void GeneratePhi( ValueStack* , const ValueStack& , const ValueStack& , std::size_t , ControlFlow* );

  StopReason GotoIfEnd   ( interpreter::BytecodeIterator* , const std::uint32_t* );
  StopReason BuildIf     ( interpreter::BytecodeIterator* itr );
  StopReason BuildIfBlock( interpreter::BytecodeIterator* , const std::uint32_t* );
  // Build logical IR graph
  StopReason BuildLogic  ( interpreter::BytecodeIterator* itr );
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
  StopReason GotoLoopEnd     ( interpreter::BytecodeIterator* itr );
  Expr* BuildLoopEndCondition( interpreter::BytecodeIterator* itr , ControlFlow* );
  StopReason BuildLoop       ( interpreter::BytecodeIterator* itr );
  StopReason BuildLoopBody   ( interpreter::BytecodeIterator* itr , ControlFlow* );
  StopReason BuildForeverLoop( interpreter::BytecodeIterator* itr );
  void GenerateLoopPhi       ();
  void PatchLoopPhi          ();

  // Iterate until we see FEEND/FEND1/FEND2/FEVREND
  StopReason BuildLoopBlock  ( interpreter::BytecodeIterator* itr );
  void PatchUnconditionalJump( UnconditionalJumpList* , ControlFlow* , const interpreter::BytecodeLocation& );
 private:
  // Zone owned by the Graph object, and it is supposed to be stay around while the
  // optimization happenened
  zone::Zone*              zone_;
  Handle<Script>           script_;
  Graph*                   graph_;
  // Working set data , used when doing inline and other stuff
  Environment*             env_;
  std::vector<FuncInfo>    func_info_;
  // Type trace for speculative operation generation
  const RuntimeTrace&      runtime_trace_;
  // This zone is used for other transient memory costs during graph construction
  zone::Zone               temp_zone_;
  // Inliner , checks for inline operation
  std::unique_ptr<Inliner> inliner_;
 private:
  class OSRScope ;
  class FuncScope;
  class LoopScope;
  class BackupEnvironment;

  friend class OSRScope ;
  friend class FuncScope;
  friend class LoopScope;
  friend class BackupEnvironment;
  friend class Environment;
};

// --------------------------------------------------------------------------
//
// Inline
//
// --------------------------------------------------------------------------
inline GraphBuilder::FuncInfo::FuncInfo( const Handle<Prototype>& proto , ControlFlow* start_region ,
                                                                          std::uint32_t b ):
  prototype         (proto),
  region            (start_region),
  base              (b),
  max_local_var_size(proto->max_local_var_size()),
  loop_info         (),
  return_list       (),
  guard_list        (),
  bc_analyze        (proto),
  osr_start         (NULL)
{}

inline GraphBuilder::FuncInfo::FuncInfo( const Handle<Prototype>& proto , ControlFlow* start_region ,
                                                                          const std::uint32_t* ostart ):
  prototype         (proto),
  region            (start_region),
  base              (0),
  max_local_var_size(proto->max_local_var_size()),
  loop_info         (),
  return_list       (),
  guard_list        (),
  bc_analyze        (proto),
  osr_start         (ostart)
{}

inline GraphBuilder::FuncInfo::FuncInfo( FuncInfo&& that ):
  prototype         (that.prototype),
  region            (that.region),
  base              (that.base),
  max_local_var_size(that.max_local_var_size),
  loop_info         (std::move(that.loop_info)),
  return_list       (std::move(that.return_list)),
  guard_list        (std::move(that.guard_list)),
  bc_analyze        (std::move(that.bc_analyze)),
  osr_start         (that.osr_start)
{}

inline GraphBuilder::GraphBuilder( const Handle<Script>& script , const RuntimeTrace& tt ):
  zone_             (NULL),
  script_           (script),
  graph_            (NULL),
  func_info_        (),
  runtime_trace_    (tt),
  temp_zone_        (),
  // TODO:: change inliner to runtime construction based on configuration
  inliner_          ( new StaticInliner() )
{}

inline void GraphBuilder::FuncInfo::EnterLoop( const std::uint32_t* pc ) {
  const BytecodeAnalyze::LoopHeaderInfo* info = bc_analyze.LookUpLoopHeader(pc);
  lava_debug(NORMAL,lava_verify(info););
  loop_info.push_back(LoopInfo(info));
}

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_GRAPH_BUILDER_H_
