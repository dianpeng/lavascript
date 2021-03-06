#include "graph-builder.h"
#include "hir.h"
#include "effect.h"
#include "inliner.h"
#include "type-inference.h"

#include "fold/folder.h"
#include "fold/fold-box.h"

#include "src/util.h"
#include "src/objects.h"
#include "src/runtime-trace.h"
#include "src/interpreter/intrinsic-call.h"
#include "src/interpreter/bytecode.h"
#include "src/interpreter/bytecode-iterator.h"

#include "src/zone/zone.h"
#include "src/zone/stl.h"

#include <cstdint>
#include <string>
#include <memory>
#include <cmath>
#include <vector>
#include <set>
#include <map>

namespace lavascript {
namespace cbase      {
namespace hir        {
namespace            {

typedef zone::stl::ZoneVector<Expr*> ValueStack;
using namespace ::lavascript::interpreter;

// This is a HIR/MIR graph consturction , the LIR is essentially a traditional CFG
// The graph is a sea of nodes style and it is responsible for all optimization before
// scheduling
// The builder can build 1) normal function call 2) OSR style function IR
class GraphBuilder {
 public:
  // all intenral class forward declaration
  struct FuncInfo;

  // Environment
  // Track *all* program states in each lexical scope and created on the fly when
  // we enter into a new scope and merge it back when we exit the lexical scope.
  class Environment {
   public:
    // Help to mark which scope this envrionment is in. Used to help setup different
    // kinds of the effect node to track side effect.
    enum { LEXICAL_SCOPE , LOOP_SCOPE };

    struct GlobalVar {
      Str name; Expr* value;
      GlobalVar( const void* k , std::size_t l , Expr* v ): name(k,l), value(v) {}
      GlobalVar( const Str&  k , Expr* v                 ): name(k)  , value(v) {}
      bool operator == ( const Str& k ) const { return Str::Cmp(name,k) == 0; }
    };
    typedef zone::stl::ZoneVector<GlobalVar>     GlobalMap;
    typedef zone::stl::ZoneVector<Expr*>         UpValueVector;
    typedef zone::stl::ZoneVector<UpValueVector> UpValueVectorStack;
    typedef std::function<Expr* ()>              KeyProvider;
   public:
    Environment( zone::Zone* , GraphBuilder* );
    Environment( const Environment& , int    );
    Environment( const Environment& );

    void InsertBranchStartEffect( ControlFlow* );
   public:
    // init environment object from prototype object
    void EnterFunctionScope( const FuncInfo& );
    void PopulateArgument  ( const FuncInfo& );
    void ExitFunctionScope ( const FuncInfo& );

    // getter/setter
    Expr*  GetUpValue      ( std::uint8_t );
    Expr*  GetGlobal       ( const void* , std::size_t , const KeyProvider& );

    void   SetUpValue      ( std::uint8_t , Expr* );
    void   SetGlobal       ( const void* , std::size_t , const KeyProvider& , Expr* );
    // accessor
    ValueStack*      stack()    { return &stack_;          }
    UpValueVector*   upvalue()  { return &upval_stk_.back(); }
    GlobalMap*       global()   { return &global_;         }
    void set_global  ( GlobalMap&& gm ) { global_ = std::move(gm); }

    const ValueStack*      stack() const { return &stack_; }
    const UpValueVector* upvalue() const { return &upval_stk_.back(); }
    const GlobalMap*     global () const { return &global_; }

    // effect group list
    Effect*          effect()          { return effect_.ptr(); }
    const Effect*    effect() const    { return effect_.ptr(); }
    Checkpoint*      cp    () const    { return cp_;  }
    zone::Zone*      zone  () const    { return zone_ ;  }
    void UpdateCP   ( Checkpoint* cp ) { cp_ = cp; }

    LoopEffectStart*   AsLoopEffectStart() { return effect()->write_effect()->As<LoopEffectStart>(); }
   private:
    zone::Zone*        zone_;            // zone allocator
    ValueStack         stack_;           // register stack
    UpValueVectorStack upval_stk_;       // upvalue's value stack
    GlobalMap          global_;          // global's effect group
    GraphBuilder*      gb_;              // graph builder
    CheckedLazyInstance<Effect> effect_; // a list of tracked effect group
    Checkpoint*        cp_;              // current frame state for this environment

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
      PhiBase*     phi;  // phi node
      PhiVar( PhiVarType t , std::uint32_t i , PhiBase* p ): type(t), idx(i), phi(p) {}
    };
    std::vector<PhiVar> phi_list;
    // loop effect phi node , used to prevent memory operation forwarding cross loop
    // carry dependency boundary
    LoopEffectStart* loop_effect_phi;
   public:
    bool HasParentLoop() const { return loop_header_info->prev != NULL; }
    bool HasJump() const { return !pending_break.empty() || !pending_continue.empty(); }
    void AddBreak( Jump* node , const std::uint32_t* target , const Environment& e ) {
      pending_break.push_back(UnconditionalJump(node,target,e));
    }
    void AddContinue( Jump* node , const std::uint32_t* target , const Environment& e ) {
      pending_continue.push_back(UnconditionalJump(node,target,e));
    }
    void AddPhi( PhiVarType type , std::uint32_t index , PhiBase* phi ) {
      phi_list.push_back(PhiVar(type,index,phi));
    }
    LoopInfo( const BytecodeAnalyze::LoopHeaderInfo* info ):
      pending_break(),
      pending_continue(),
      loop_header_info(info),
      phi_list(),
      loop_effect_phi(NULL)
    {}
  };

  // Structure to record function level information when we do IR construction.
  // Once a inline happened, then we will push a new FuncInfo object into func_info
  // stack/vector
  struct FuncInfo {
    /** member field **/
    Handle<Prototype>           prototype; // cached for faster access
    ControlFlow*                region;
    std::uint32_t               base;
    std::vector<LoopInfo>       loop_info;
    std::vector<JumpWithValue*> return_list;
    std::vector<Guard*>         guard_list ;
    BytecodeAnalyze             bc_analyze;
    const std::uint32_t*        osr_start;
    const std::uint32_t*        return_addr;
    std::uint8_t                max_local_var_size;
    bool                        tcall;     // whether it is a tail call

   public:
    inline FuncInfo( const Handle<Prototype>& , ControlFlow* , std::uint32_t , const std::uint32_t* ,
                                                                               bool tcall = false );
    inline FuncInfo( const Handle<Prototype>& , ControlFlow* , const std::uint32_t* , bool tcall = false );
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
    STOP_SUCCESS,
    STOP_NA
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
  FuncInfo& top_func ()                      { return func_info_.front();}
  const FuncInfo& func_info()          const { return func_info_.back(); }
  bool IsTopFunction()                 const { return func_info_.size() == 1; }
  const Handle<Prototype>& prototype() const { return func_info().prototype; }
  std::uint32_t base()                 const { return func_info().base; }
  ControlFlow* region()                const { lava_debug(NORMAL,lava_verify(region_);); return region_; }
  void set_region( ControlFlow* new_region ) { region_ = new_region; }
  Environment* env()                   const { return env_; }
  ValueStack*  vstk()                  const { return env_->stack(); }
  Environment::UpValueVector*  upval() const { return env_->upvalue();}

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
  Expr* AddTypeFeedbackIfNeed( Expr* , TypeKind     , const BytecodeLocation& );
  Expr* AddTypeFeedbackIfNeed( Expr* , const Value& , const BytecodeLocation& );
  // Create a guard node and linked it back to the current graph
  Guard* NewGuard            ( Test* );
  // Check whether a node's type is the needed type
  bool CheckType( Expr** , TypeKind     , std::size_t , const BytecodeLocation& );
 private: // Arithmetic
  // Unary
  Expr* NewUnary            ( Expr* , Unary::Operator , const BytecodeLocation& );
  Expr* TrySpeculativeUnary ( Expr* , Unary::Operator , const BytecodeLocation& );
  Expr* NewUnaryFallback    ( Expr* , Unary::Operator );
  // Binary
  Expr* NewBinary           ( Expr* , Expr* , Binary::Operator , const BytecodeLocation& );
  Expr* TrySpeculativeBinary( Expr* , Expr* , Binary::Operator , const BytecodeLocation& );
  Expr* NewBinaryFallback   ( Expr* , Expr* , Binary::Operator );
  // Ternary
  Expr* NewTernary( Expr* , Expr* , Expr* , const BytecodeLocation& );

 private: // Intrinsic
  Expr* NewICall  ( std::uint8_t ,std::uint8_t ,std::uint8_t ,bool , const BytecodeLocation& );

  // Lower intrinsic call into internal node
  Expr* LowerICall( ICall* , const BytecodeLocation& );

  // Math intrinsic function lowering
  Expr* LowerICallMin   ( ICall* , const BytecodeLocation& );
  Expr* LowerICallMax   ( ICall* , const BytecodeLocation& );
  Expr* LowerICallAbs   ( ICall* , const BytecodeLocation& );

  // Bit operations
  Expr* LowerICallLShift( ICall* , const BytecodeLocation& );
  Expr* LowerICallRShift( ICall* , const BytecodeLocation& );
  Expr* LowerICallLRo   ( ICall* , const BytecodeLocation& );
  Expr* LowerICallRRo   ( ICall* , const BytecodeLocation& );
  Expr* LowerICallBAnd  ( ICall* , const BytecodeLocation& );
  Expr* LowerICallBOr   ( ICall* , const BytecodeLocation& );

  Expr* LowerICallUpdate( ICall* , const BytecodeLocation& );
  Expr* LowerICallSet   ( ICall* , const BytecodeLocation& );
  Expr* LowerICallGet   ( ICall* , const BytecodeLocation& );
  Expr* LowerICallPut   ( ICall* , const BytecodeLocation& );
  Expr* LowerICallIter  ( ICall* , const BytecodeLocation& );

 private: // Property/Index Get/Set
  Expr* TrySpeculativePSet( Expr* , Expr* , Expr* , const BytecodeLocation& );
  Expr* TryFoldTypedPSet  ( Expr* , Expr* , Expr* , const BytecodeLocation& );
  Expr* NewPSetFallback   ( Expr* , Expr* , Expr* , const BytecodeLocation& );
  Expr* NewPSet           ( Expr* , Expr* , Expr* , const BytecodeLocation& );

  Expr* TrySpeculativePGet( Expr* , Expr* , const BytecodeLocation& );
  Expr* TryFoldTypedPGet  ( Expr* , Expr* , const BytecodeLocation& );
  Expr* NewPGetFallback   ( Expr* , Expr* , const BytecodeLocation& );
  Expr* NewPGet           ( Expr* , Expr* , const BytecodeLocation& );

  Expr* GenerateRawIndex  ( Expr* , const BytecodeLocation& );

  Expr* TrySpeculativeISet( Expr* , Expr* , Expr* , const BytecodeLocation& );
  Expr* TryFoldTypedISet  ( Expr* , Expr* , Expr* , const BytecodeLocation& );
  Expr* NewISetFallback   ( Expr* , Expr* , Expr* , const BytecodeLocation& );
  Expr* NewISet           ( Expr* , Expr* , Expr* , const BytecodeLocation& );

  Expr* TrySpeculativeIGet( Expr* , Expr* , const BytecodeLocation& );
  Expr* TryFoldTypedIGet  ( Expr* , Expr* , const BytecodeLocation& );
  Expr* NewIGetFallback   ( Expr* , Expr* , const BytecodeLocation& );
  Expr* NewIGet           ( Expr* , Expr* , const BytecodeLocation& );

 private: // Global variables
  void NewGGet( std::uint8_t , std::uint16_t , bool sso );
  void NewGSet( std::uint16_t , std::uint8_t , bool sso );

 private: // Upvalue
  void NewUGet( std::uint8_t , std::uint8_t );
  void NewUSet( std::uint8_t , std::uint8_t );

 private: // Checkpoint generation
  // Create a checkpoint with snapshot of current stack status
  Checkpoint* GenerateCheckpoint( const BytecodeLocation& );
  // Create a eager checkpoint at the new *merge* region node if needed
  void GenerateMergeCheckpoint  ( const Environment& , const BytecodeLocation& );
  // Get a init checkpoint , checkpoint that has IRInfo points to the first of the bytecode ,
  // of the top most inlined function.
  Checkpoint* InitCheckpoint ();

 private: // Phi
  Expr* NewPhi( Expr* , Expr* , ControlFlow* );

 private: // Iterator
  Expr* NewItrNew  ( Expr* , const BytecodeLocation& );
  Expr* NewItrNext ( Expr* , const BytecodeLocation& );
  Expr* NewItrTest ( Expr* , const BytecodeLocation& );
  Expr* NewItrDeref( Expr* , const BytecodeLocation& );

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
  void SetupOSRLoopCondition( BytecodeIterator* );
  StopReason BuildOSRLoop   ( BytecodeIterator* );
  StopReason PeelOSRLoop    ( BytecodeIterator* );
 private: // Build bytecode
  // Just build *one* BC isntruction , this will not build certain type of BCs
  // since it is expected other routine to consume those BCs
  StopReason BuildBytecode  ( BytecodeIterator* );
  StopReason BuildBasicBlock( BytecodeIterator* , const std::uint32_t* end_pc = NULL );

  // Build branch IR graph
  void InsertPhi  ( Environment* , Environment*      , ControlFlow* );
  void GeneratePhi( ValueStack*  , const ValueStack& , const ValueStack& ,std::size_t ,
                                                                          std::size_t ,
                                                                          ControlFlow* );
  StopReason BuildIf     ( BytecodeIterator* );
  StopReason BuildIfBlock( BytecodeIterator* , const std::uint32_t* );
  // Build logical IR graph
  StopReason BuildLogic  ( BytecodeIterator* );
  // Build ternary IR graph
  StopReason BuildTernary( BytecodeIterator* );
  // Build loop IR graph
  //
  // The core/tricky part of the loop IR is where to insert PHI due to we cannot see
  // a variable when we insert PHI in loop body. We know this information by using
  // information from BytecodeAnalyze. It will tell us which list of variables are
  // modified inside of the loop and these variables are not local variable inside of
  // the loop. So we just insert PHI before hand and at last we patch the PHI to correct
  // its *second input operand* since the modification comes later in the loop.
  Expr* BuildLoopEndCondition( BytecodeIterator* , ControlFlow* );

  StopReason BuildLoop       ( BytecodeIterator* );
  StopReason BuildLoopBody   ( BytecodeIterator* , ControlFlow* );
  StopReason BuildForeverLoop( BytecodeIterator* );
  void GenerateLoopInductionVariable( Loop* );
  void PatchLoopInductionVariable   ();

  // Iterate until we see FEEND/FEND1/FEND2/FEVREND
  StopReason BuildLoopBlock  ( BytecodeIterator* );
  void PatchUnconditionalJump( UnconditionalJumpList* , ControlFlow* , const BytecodeLocation& );
 private:
  // ----------------------------------------------------------------------------------
  // Inline
  // ----------------------------------------------------------------------------------
  bool IsInlineFrame() const { return func_info_.size() > 1; }
  // try to do an inline, if we cannot inline it, it will fallback to generate a call node.
  // otherwise, it will inline the function into the graph directly
  bool NewCall           ( BytecodeIterator* );
  bool NewCallFallback   ( BytecodeIterator* );
  bool SpeculativeInline ( const Handle<Prototype>& , BytecodeIterator* );
  bool DoInline          ( const Handle<Prototype>& , std::uint8_t , bool , const std::uint32_t* );

 private:
  // Zone owned by the Graph object, and it is supposed to be stay around while the
  // optimization happenened
  zone::Zone*                         zone_;
  zone::Zone                          temp_zone_;
  ControlFlow*                        region_;
  // Folder to perform on the fly expression optimization
  std::unique_ptr<FolderChain>        folder_chain_;
  Handle<Script>                      script_;
  Graph*                              graph_;
  // Working set data , used when doing inline and other stuff
  Environment*                        env_;
  // Type trace for speculative operation generation
  const RuntimeTrace&                 runtime_trace_;

  // Inliner , checks for inline operation
  std::unique_ptr<Inliner>            inliner_;

  // Tracked information
  zone::stl::ZoneVector<FuncInfo>     func_info_;
  zone::stl::ZoneVector<ControlFlow*> trap_list_;

  // init barrier and checkpoint
  Checkpoint*    init_checkpoint_;
  InitBarrier*   init_barrier_;

  class OSRScope ;
  class InlineScope;
  class FuncScope;
  class LoopScope;
  class BackupEnvironment;

  friend class OSRScope ;
  friend class InlineScope;
  friend class FuncScope;
  friend class LoopScope;
  friend class BackupEnvironment;
  friend class Environment;

  LAVA_DISALLOW_COPY_AND_ASSIGN(GraphBuilder)
};

// --------------------------------------------------------------------------
//
// Inline
//
// --------------------------------------------------------------------------
inline GraphBuilder::FuncInfo::FuncInfo( const Handle<Prototype>& proto ,
                                         ControlFlow*             start_region ,
                                         std::uint32_t            b ,
                                         const std::uint32_t*     raddr ,
                                         bool                     tcall ):
  prototype         (proto),
  region            (start_region),
  base              (b),
  loop_info         (),
  return_list       (),
  guard_list        (),
  bc_analyze        (proto),
  osr_start         (NULL),
  return_addr       (raddr),
  max_local_var_size(proto->max_local_var_size()),
  tcall             (tcall)
{}

inline GraphBuilder::FuncInfo::FuncInfo( const Handle<Prototype>& proto ,
                                         ControlFlow*             start_region ,
                                         const std::uint32_t*     ostart ,
                                         bool                     tcall ):
  prototype         (proto),
  region            (start_region),
  base              (0),
  loop_info         (),
  return_list       (),
  guard_list        (),
  bc_analyze        (proto),
  osr_start         (ostart),
  return_addr       (NULL),
  max_local_var_size(proto->max_local_var_size()),
  tcall             (tcall)
{}

inline GraphBuilder::FuncInfo::FuncInfo( FuncInfo&& that ):
  prototype         (that.prototype),
  region            (that.region),
  base              (that.base),
  loop_info         (std::move(that.loop_info)),
  return_list       (std::move(that.return_list)),
  guard_list        (std::move(that.guard_list)),
  bc_analyze        (std::move(that.bc_analyze)),
  osr_start         (that.osr_start),
  return_addr       (that.return_addr),
  max_local_var_size(that.max_local_var_size),
  tcall             (that.tcall)
{}

inline GraphBuilder::GraphBuilder( const Handle<Script>& script , const RuntimeTrace& tt ):
  zone_             (NULL),
  temp_zone_        (),
  region_           (),
  folder_chain_     (),
  script_           (script),
  graph_            (NULL),
  runtime_trace_    (tt),
  // TODO:: change inliner to runtime construction based on configuration
  inliner_          (new StaticInliner()),
  func_info_        (&temp_zone_),
  trap_list_        (&temp_zone_),
  init_checkpoint_  (NULL),
  init_barrier_     (NULL)
{
  folder_chain_ = std::make_unique<FolderChain>(&temp_zone_);
}

inline void GraphBuilder::FuncInfo::EnterLoop( const std::uint32_t* pc ) {
  const BytecodeAnalyze::LoopHeaderInfo* info = bc_analyze.LookUpLoopHeader(pc);
  lava_debug(NORMAL,lava_verify(info););
  loop_info.push_back(LoopInfo(info));
}

GraphBuilder::Environment::Environment( zone::Zone* zone , GraphBuilder* gb ):
  zone_     (zone),
  stack_    (zone),
  upval_stk_(zone),
  global_   (zone),
  gb_       (gb),
  effect_   (),
  cp_       (gb->InitCheckpoint())
{
  auto ib = InitBarrier::New(gb->graph());
  effect_.Init(ib);
  lava_debug(NORMAL,lava_verify(!gb->init_barrier_ && !gb->init_checkpoint_););

  gb->init_barrier_    = ib;
  gb->init_checkpoint_ = cp_;
}

GraphBuilder::Environment::Environment( const Environment& env , int type ):
  zone_   (env.zone_ ),
  stack_  (env.stack_),
  upval_stk_(env.upval_stk_),
  global_ (env.global_),
  gb_     (env.gb_),
  effect_ (),
  cp_     (env.cp_)
{
  WriteEffect* effect = NULL;
  if(type == LOOP_SCOPE) {
    // create a loop effect phi node for loop scope
    effect = LoopEffectStart::New( gb_->graph() , env.effect()->write_effect() );
  } else {
    effect = env.effect()->write_effect();
  }
  // initialize effect object inside of this environment
  effect_.Init(effect);
}

GraphBuilder::Environment::Environment( const Environment& env ):
  zone_   (env.zone_ ),
  stack_  (env.stack_),
  upval_stk_(env.upval_stk_),
  global_ (env.global_),
  gb_     (env.gb_),
  effect_ (),
  cp_     (env.cp_)
{
  effect_.Init(*env.effect_);
}

void GraphBuilder::Environment::InsertBranchStartEffect( ControlFlow* region ) {
  auto se = BranchStartEffect::New(gb_->graph());
  effect()->UpdateWriteEffect(se);

  lava_debug(NORMAL,lava_verify(region->Is<IfTrue>() || region->Is<IfFalse>()););
  if(region->Is<IfTrue>()) {
    region->As<IfTrue>()->set_branch_start_effect(se);
  } else {
    region->As<IfFalse>()->set_branch_start_effect(se);
  }
}

void GraphBuilder::Environment::EnterFunctionScope( const FuncInfo& func ) {
  // resize the stack to have 256 slots for new prototype
  stack_.resize       ( stack_.size() + kRegisterSize );
  // resize the stack to have enough slots for the upvalues
  upval_stk_.push_back( UpValueVector(zone_) );
  upvalue()->resize   ( func.prototype->upvalue_size() );
}

void GraphBuilder::Environment::PopulateArgument ( const FuncInfo& func ) {
  auto sz = func.prototype->argument_size();
  for( std::size_t i = 0 ; i < sz ; ++i ) {
    stack_[i] = Arg::New(gb_->graph_,static_cast<std::uint32_t>(i));
  }
}

void GraphBuilder::Environment::ExitFunctionScope( const FuncInfo& func ) {
  stack_.resize( func.base + kRegisterSize );
  upval_stk_.pop_back();
}

Expr* GraphBuilder::Environment::GetUpValue( std::uint8_t index ) {
  lava_debug(NORMAL,lava_verify(index < upvalue()->size()););
  if( auto v = upvalue()->at(index); v ) {
    return v;
  } else {
    auto uget = UGet::New(gb_->graph_,index,gb_->method_index());
    effect()->AddReadEffect(uget);
    upvalue()->at(index) = uget;
    return uget;
  }
}

void GraphBuilder::Environment::SetUpValue( std::uint8_t index , Expr* value ) {
  auto uset = USet::New(gb_->graph_,index,gb_->method_index(),value);
  effect()->UpdateWriteEffect(uset);
  upvalue()->at(index) = value;
}

Expr* GraphBuilder::Environment::GetGlobal ( const void* key , std::size_t length ,
                                                               const KeyProvider& key_provider ) {
  auto itr = std::find(global_.begin(),global_.end(),Str(key,length));
  if(itr != global_.end()) {
    return itr->value;
  } else {
    auto gget = GGet::New(gb_->graph_,key_provider());
    effect()->AddReadEffect(gget);
    global_.push_back(GlobalVar(key,length,gget));
    return gget;
  }
}

void GraphBuilder::Environment::SetGlobal( const void* key , std::size_t length ,
                                                             const KeyProvider& key_provider ,
                                                             Expr* value ) {
  auto gset = GSet::New(gb_->graph_,key_provider(),value);
  effect()->UpdateWriteEffect(gset);

  auto itr = std::find(global_.begin(),global_.end(),Str(key,length));
  if(itr == global_.end()) {
    global_.push_back(GlobalVar(key,length,value));
  } else {
    itr->value = value;
  }
}

/* -------------------------------------------------------------
 * RAII objects to handle different type of scopes when iterate
 * different bytecode along the way
 * -----------------------------------------------------------*/
class GraphBuilder::OSRScope {
 public:
  OSRScope( GraphBuilder* , const Handle<Prototype>& , ControlFlow* , const std::uint32_t* );
  ~OSRScope() {
    gb_->env()->ExitFunctionScope(gb_->func_info_.back());
    gb_->func_info_.pop_back();
  }
 private:
  GraphBuilder* gb_;
};

class GraphBuilder::FuncScope {
 public:
  FuncScope( GraphBuilder* , const Handle<Prototype>& , ControlFlow* );
  ~FuncScope() {
    gb_->env()->ExitFunctionScope(gb_->func_info_.back());
    gb_->func_info_.pop_back();
  }
 private:
  GraphBuilder* gb_;
};

class GraphBuilder::InlineScope {
 public:
  InlineScope( GraphBuilder* , const Handle<Prototype>& , std::size_t           ,
                                                          bool                  ,
                                                          ControlFlow*          ,
                                                          const std::uint32_t*  );
  ~InlineScope();
 private:
  GraphBuilder* gb_;
};

class GraphBuilder::LoopScope {
 public:
  LoopScope( GraphBuilder* gb , const std::uint32_t* pc ) :
    gb_(gb) { gb->func_info().EnterLoop(pc); }
  ~LoopScope() { gb_->func_info().LeaveLoop(); }
 private:
  GraphBuilder* gb_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(LoopScope)
};

class GraphBuilder::BackupEnvironment {
 public:
  BackupEnvironment( GraphBuilder::Environment* new_env , GraphBuilder* gb ):
    old_env_(gb->env_), gb_(gb) { gb->env_ = new_env; }

  ~BackupEnvironment() { gb_->env_ = old_env_; }
 private:
  Environment* old_env_;
  GraphBuilder* gb_;
};

// A nicer way to use backup environment object to do backup
#define BACKUP_ENVIRONMENT(env,gb)                                    \
  if( auto __backup = BackupEnvironment((env),(gb)); true )

GraphBuilder::OSRScope::OSRScope( GraphBuilder* gb , const Handle<Prototype>& proto ,
                                                     ControlFlow* region ,
                                                     const std::uint32_t* osr_start ):
  gb_(gb) {
  // initialize FuncInfo for OSR compilation entry
  FuncInfo temp(proto,region,osr_start);
  // get the loop header information and recursively register all its needed
  // loop info object inside of the FuncInfo object
  auto loop_header = temp.bc_analyze.LookUpLoopHeader(osr_start);
  lava_debug(NORMAL,lava_verify(loop_header););
  // need to iterate the loop one bye one from the top most loop
  // inside of the nested loop cluster so we will use a queue
  {
    std::vector<const BytecodeAnalyze::LoopHeaderInfo*> queue;
    auto cur_header = loop_header->prev; // skip the OSR loop
    while(cur_header) {
      queue.push_back(cur_header);
      cur_header = cur_header->prev;
    }
    for( auto ritr = queue.rbegin() ; ritr != queue.rend(); ++ritr ) {
      temp.loop_info.push_back(LoopInfo(*ritr));
    }
  }
  // push current FuncInfo object to the trackings tack
  gb->func_info_.push_back(FuncInfo(std::move(temp)));
  // initialize environment object for this OSRScope
  gb->env()->EnterFunctionScope(gb->func_info());
  // initialize the function argument
  gb->env()->PopulateArgument  (gb->func_info());
}

GraphBuilder::FuncScope::FuncScope( GraphBuilder* gb , const Handle<Prototype>& proto ,
                                                       ControlFlow* region ):
  gb_(gb) {

  gb->func_info_.push_back(FuncInfo(proto,region,0u));
  gb->env()->EnterFunctionScope(gb->func_info());
  gb->env()->PopulateArgument(gb->func_info());
}

GraphBuilder::InlineScope::InlineScope( GraphBuilder*            gb,
                                        const Handle<Prototype>& proto,
                                        std::size_t              new_base ,
                                        bool                     tcall,
                                        ControlFlow*             region,
                                        const std::uint32_t*     raddr ):
  gb_(gb) {

  gb->func_info_.push_back(FuncInfo(proto,region,new_base,raddr,tcall));
  gb->env()->EnterFunctionScope(gb->func_info());
}

GraphBuilder::InlineScope::~InlineScope() {
  gb_->env()->ExitFunctionScope(gb_->func_info_.back());
  gb_->func_info_.pop_back();
}

// ===============================================================================
// Inline
// ===============================================================================
bool GraphBuilder::DoInline( const Handle<Prototype>& proto , std::uint8_t         base ,
                                                              bool                 tcall,
                                                              const std::uint32_t* raddr ) {
  Expr* ret    = NULL;
  auto new_itr = proto->GetBytecodeIterator();
  {
    // setup inline function lexical scope
    InlineScope scope(this,proto,func_info().base + base ,tcall,region(),raddr);
    // create the inline start node to mark the Graph
    auto istart = InlineStart::New(graph_,region());
    set_region(istart);
    // do the recursive graph construction
    if(BuildBasicBlock(&new_itr) == STOP_BAILOUT) return false;
    // setup the inline end block
    auto iend = InlineEnd::New(graph_,region());
    // handle return value of inline frame here
    if(func_info().tcall) {
      lava_verify(func_info().return_list.empty());
      // if it is a tail call inline frame then it doesn't generate anything
      // but we still need to put something as placeholder
      ret = Nil::New(graph_);
    } else {
      // generate return value
      if( func_info().return_list.size() == 1 ) {
        ret = func_info().return_list[0]->value();
      } else {
        // if the inlined function has multiple return/exit point then
        // we need to do a jump value merge node here to fan in all the
        // inlined functino conrol flow
        auto phi = Phi::New(graph_,iend);
        for( auto &e : func_info().return_list ) {
          iend->AddBackwardEdge(e);
          phi->AddOperand(e->value());
        }
        ret = phi;
      }
    }
    set_region(iend);
  }

  StackSet(kAccRegisterIndex,ret); // set the return value into the previous function's stack
  return true;                     // inline is done
}

bool GraphBuilder::SpeculativeInline( const Handle<Prototype>& proto , BytecodeIterator* itr ) {
  auto tcall = itr->opcode() == BC_TCALL;
  std::uint8_t func,base,arg;
  itr->GetOperand(&func,&base,&arg);

  // the callable node in IR graph
  auto f = StackGet(func);

  if(auto tp = GetTypeInference(f); tp != TPKIND_CLOSURE) {
    // generate a CondTrap here. we cannot simply do a Guard node since our Guard
    // node is an expression node instead of a control flow. The guard for this
    // inline function call must be done *before* the function body generated, so
    // we need to add a CondTrap which is a control flow node instead of expression.
    auto tt = TestType::New(graph_,TPKIND_CLOSURE,f);
    auto ct = CondTrap::New(graph_,tt,env()->cp(),region());
    auto rg = Region::New(graph_,ct);
    set_region(rg);
    trap_list_.push_back(ct);
  }

  // start the inline
  return DoInline(proto,base,tcall,itr->NextPC());
}

bool GraphBuilder::NewCall( BytecodeIterator* itr ) {
  lava_debug(NORMAL,lava_verify(itr->opcode() == BC_CALL || itr->opcode() == BC_TCALL););

  std::uint8_t func,base,arg;
  itr->GetOperand(&func,&base,&arg);
  auto f = StackGet(func);
  if(f->Is<Closure>()) {
    // okay this object/expression node is an closure node, then we could just
    // use this to do static inline without needing to know the type trace and
    // this is most useful since it helps do inline locally.
    auto proto = script_->GetFunction(f->As<Closure>()->ref()).prototype;
    return DoInline(proto,base,itr->opcode() == BC_TCALL,itr->NextPC());
  } else {
    // this is the general case inline when we don't know the type of our function.
    // it mostly happened at cross file/module function inline
    auto tt   = runtime_trace_.GetTrace( itr->pc() );
    if(tt) {
      auto &v = tt->data[0];
      if(v.IsClosure()) {
        auto proto = v.GetClosure()->prototype();
        if(inliner_->ShouldInline(func_info_.size(),proto)) {
          return SpeculativeInline(v.GetClosure()->prototype(),itr);
        }
      }
    }
  }

  // okay, we need a dynamic call site for performing the function call
  return NewCallFallback(itr);
}

bool GraphBuilder::NewCallFallback( BytecodeIterator* itr ) {
  (void)itr;
  // TODO:: add implementation
  lava_die();
  return false;
}

// ===============================================================================
// Constant
// ===============================================================================
Expr* GraphBuilder::NewConstNumber( std::int32_t ivalue ) {
  return Float64::New(graph_,static_cast<double>(ivalue));
}

Expr* GraphBuilder::NewNumber( std::uint8_t ref ) {
  double real = func_info().prototype->GetReal(ref);
  return Float64::New(graph_,real);
}

Expr* GraphBuilder::NewString( std::uint8_t ref ) {
  Handle<String> str(func_info().prototype->GetString(ref));
  if(str->IsSSO()) {
    return SString::New(graph_,str->sso());
  } else {
    return LString::New(graph_,str->long_string());
  }
}

Expr* GraphBuilder::NewString( const Str& str ) {
  return ::lavascript::cbase::hir::NewString(graph_,str.data,str.length);
}

Expr* GraphBuilder::NewSSO( std::uint8_t ref ) {
  const SSO& sso = *(func_info().prototype->GetSSO(ref)->sso);
  return SString::New(graph_,sso);
}

Str   GraphBuilder::NewStr( std::uint32_t ref , bool sso ) {
  if(sso) {
    auto sso = func_info().prototype->GetSSO(ref)->sso;
    return Str(sso->data(),sso->size());
  } else {
    auto str = func_info().prototype->GetString(ref);
    return Str(str->data(),str->size());
  }
}

Expr* GraphBuilder::NewBoolean( bool value ) {
  return Boolean::New(graph_,value);
}

IRList* GraphBuilder::NewIRList( std::size_t size ) {
  auto ret = IRList::New(graph_,size);
  env()->effect()->UpdateWriteEffect(ret);
  return ret;
}

IRObject* GraphBuilder::NewIRObject( std::size_t size ) {
  auto ret = IRObject::New(graph_,size);
  env()->effect()->UpdateWriteEffect(ret);
  return ret;
}

// Type Guard ---------------------------------------------------------------------
Guard* GraphBuilder::NewGuard( Test* test ) {
  // create a new guard
  auto guard = Guard::New(graph_,test);
  // add this guard back to the guard_list , later on we can patch the guard
  func_info().guard_list.push_back(guard);
  // return the newly created region node
  return guard;
}

Expr* GraphBuilder::AddTypeFeedbackIfNeed( Expr* node , const Value& value , const BytecodeLocation& pc ) {
  return AddTypeFeedbackIfNeed(node,MapValueToTypeKind(value),pc);
}

Expr* GraphBuilder::AddTypeFeedbackIfNeed( Expr* n , TypeKind tp , const BytecodeLocation& pc ) {
  (void)pc;
  lava_debug(NORMAL,lava_verify(TPKind::IsLeaf(tp)););

  auto stp = GetTypeInference(n);
  if(stp != TPKIND_UNKNOWN) {
    /**
     * This check means our static type inference should match the traced
     * type value here. If not match basically means our IR graph is wrong
     * fundementally so it is a *BUG* actually
     */
    lava_assertF(stp == tp,"This is a *SERIOUS BUG*, we get "
                           "type inference of value %s but "
                           "the traced type is actually %s!",
                           GetTypeKindName(stp), GetTypeKindName(tp));
    return n;
  }
  lava_debug(NORMAL,lava_verify(tp != TPKIND_UNKNOWN););
  // generate type test node
  auto tt  = TestType::New(graph_,tp,n);
  // generate guard node
  auto gd  = NewGuard(tt);
  // return the new guarded node
  return gd;
}

bool GraphBuilder::CheckType( Expr** object , TypeKind tp , std::size_t index , const BytecodeLocation& pc ) {
  lava_debug(CRAZY,lava_verify(tp != TPKIND_UNKNOWN););
  // 1. try static type checking
  if(auto t = GetTypeInference(*object); t != TPKIND_UNKNOWN) {
    auto res = false;
    lava_verify(TPKind::Contain(tp,t,&res));
    return res;
  }

  // 2. try type guard since we don't have a type guess
  if(auto tt = runtime_trace_.GetTrace(pc.address()); tt) {
    auto &v  = tt->data[index];
    auto vt  = MapValueToTypeKind(v);
    auto res = false;
    lava_verify(TPKind::Contain(tp,vt,&res));

    if(res) {
      auto tt_node = TestType::New(graph_,vt,*object);
      auto guard   = NewGuard(tt_node);
      *object      = guard; // overwrite the old node with newly guarded node
      return true;
    }
  }

  return false;
}

// ========================================================================
// Unary Node
// ========================================================================
Expr* GraphBuilder::NewUnary( Expr* node , Unary::Operator op , const BytecodeLocation& pc ) {
  // 1. try to do a constant folding
  if(auto new_node = folder_chain_->Fold(graph_,UnaryFolderData{op,node}); new_node)
    return new_node;
  // 2. now we know there's no way we can resolve the expression and
  //    we don't have any type information from static type inference.
  //    fallback to do speculative unary or dynamic dispatch if needed
  return TrySpeculativeUnary(node,op,pc);
}

Expr* GraphBuilder::TrySpeculativeUnary( Expr* node , Unary::Operator op , const BytecodeLocation& pc ) {
  // try to get the value feedback from type trace operations
  auto tt = runtime_trace_.GetTrace( pc.address() );
  if(tt) {
    auto v = tt->data[1]; // unary operation's 1st operand
    if(op == Unary::NOT) {
      // create a guard for this object's boolean value under boolean context
      node = AddTypeFeedbackIfNeed(node,v,pc);
      // do a static boolean inference here
      auto tp = GetTypeInference(node);
      bool bval;
      if(TPKind::ToBoolean(tp,&bval)) {
        return Boolean::New(graph_,!bval);
      } else if(tp == TPKIND_BOOLEAN) {
        // if the guarded type is boolean, then we could just use BooleanNot
        // node which has type information and enable the inference on it
        return NewBoxNode<BooleanNot>(graph_, TPKIND_BOOLEAN,NewUnboxNode(graph_,node,TPKIND_BOOLEAN));
      }
    } else {
      if(v.IsReal()) {
        // add the type feedback for this node
        node = AddTypeFeedbackIfNeed(node,TPKIND_FLOAT64,pc);
        // create a boxed node with gut of Float64Negate node
        return NewBoxNode<Float64Negate>(graph_, TPKIND_FLOAT64, NewUnboxNode(graph_,node,TPKIND_FLOAT64));
      }
    }
  }
  // Fallback:
  // okay, we are not able to get *any* types of type information, fallback to
  // generate a fully dynamic dispatch node
  return NewUnaryFallback(node,op);
}

Expr* GraphBuilder::NewUnaryFallback( Expr* node , Unary::Operator op ) {
  return Unary::New(graph_,node,op);
}

// ========================================================================
// Binary Node
// ========================================================================
Expr* GraphBuilder::NewBinary  ( Expr* lhs , Expr* rhs , Binary::Operator op , const BytecodeLocation& pc ) {
  if(auto new_node = folder_chain_->Fold(graph_,BinaryFolderData{op,lhs,rhs}); new_node)
    return new_node;
  // try speculative binary node
  if(auto new_node = TrySpeculativeBinary(lhs,rhs,op,pc); new_node) return new_node;
  // fallback to use normal binary dispatch
  return NewBinaryFallback(lhs,rhs,op);
}

Expr* GraphBuilder::TrySpeculativeBinary( Expr* lhs , Expr* rhs , Binary::Operator op,
                                                                  const BytecodeLocation& pc ) {
  auto tt = runtime_trace_.GetTrace(pc.address());
  if(tt) {
    auto lhs_val = tt->data[1];
    auto rhs_val = tt->data[2];
    switch(op) {
      case Binary::ADD:
      case Binary::SUB:
      case Binary::MUL:
      case Binary::DIV:
      case Binary::POW:
      case Binary::MOD:
        if(lhs_val.IsReal() && rhs_val.IsReal()) {
          lhs = AddTypeFeedbackIfNeed(lhs,TPKIND_FLOAT64,pc);
          rhs = AddTypeFeedbackIfNeed(rhs,TPKIND_FLOAT64,pc);
          {
            auto l = NewUnboxNode(graph_,lhs,TPKIND_FLOAT64);
            auto r = NewUnboxNode(graph_,rhs,TPKIND_FLOAT64);
            return NewBoxNode<Float64Arithmetic>(graph_,TPKIND_FLOAT64,l,r,op);
          }
        }
        break;
      case Binary::LT:
      case Binary::LE:
      case Binary::GT:
      case Binary::GE:
      case Binary::EQ:
      case Binary::NE:
        if(lhs_val.IsReal() && rhs_val.IsReal()) {
          lhs = AddTypeFeedbackIfNeed(lhs,TPKIND_FLOAT64,pc);
          rhs = AddTypeFeedbackIfNeed(rhs,TPKIND_FLOAT64,pc);
          {
            auto l = NewUnboxNode(graph_,lhs,TPKIND_FLOAT64);
            auto r = NewUnboxNode(graph_,rhs,TPKIND_FLOAT64);
            return NewBoxNode<Float64Compare>(graph_,TPKIND_BOOLEAN,l,r,op);
          }

        } else if(lhs_val.IsString() && rhs_val.IsString()) {
          if((lhs_val.IsSSO() && rhs_val.IsSSO()) && (op == Binary::EQ || op == Binary::NE)) {
            lhs = AddTypeFeedbackIfNeed(lhs,TPKIND_SMALL_STRING,pc);
            rhs = AddTypeFeedbackIfNeed(rhs,TPKIND_SMALL_STRING,pc);
            if(op == Binary::EQ) {
              auto l = NewUnboxNode(graph_,lhs,TPKIND_SMALL_STRING);
              auto r = NewUnboxNode(graph_,rhs,TPKIND_SMALL_STRING);
              return NewBoxNode<SStringEq>(graph_,TPKIND_BOOLEAN,l,r);
            } else {
              auto l = NewUnboxNode(graph_,lhs,TPKIND_SMALL_STRING);
              auto r = NewUnboxNode(graph_,rhs,TPKIND_SMALL_STRING);
              return NewBoxNode<SStringNe>(graph_,TPKIND_BOOLEAN,l,r);
            }
          } else {
            lhs = AddTypeFeedbackIfNeed(lhs,TPKIND_STRING,pc);
            rhs = AddTypeFeedbackIfNeed(rhs,TPKIND_STRING,pc);
            {
              auto l = NewUnboxNode(graph_,lhs,TPKIND_STRING);
              auto r = NewUnboxNode(graph_,rhs,TPKIND_STRING);
              return NewBoxNode<StringCompare>(graph_,TPKIND_BOOLEAN,l,r,op);
            }
          }
        }
        break;
      case Binary::AND:
      case Binary::OR :
        {
          lhs = AddTypeFeedbackIfNeed(lhs,lhs_val,pc);
          // simplify the logic expression if we can do so
          if(auto ret = folder_chain_->Fold(graph_,BinaryFolderData{op,lhs,rhs}); ret)
            return ret;
          rhs = AddTypeFeedbackIfNeed(rhs,rhs_val,pc);
          if(GetTypeInference(lhs) == TPKIND_BOOLEAN &&
             GetTypeInference(rhs) == TPKIND_BOOLEAN) {
            auto l = NewUnboxNode(graph_,lhs,TPKIND_BOOLEAN);
            auto r = NewUnboxNode(graph_,rhs,TPKIND_BOOLEAN);
            return NewBoxNode<BooleanLogic>(graph_, TPKIND_BOOLEAN, l, r, op);
          }
        }
        break;
      default:
        break;
    }
  }

  return NULL;
}

Expr* GraphBuilder::NewBinaryFallback( Expr* lhs , Expr* rhs , Binary::Operator op ) {
  WriteEffect* write_effect = NULL;
  if(Binary::IsArithmeticOperator(op)) {
    write_effect = Arithmetic::New(graph_,lhs,rhs,op);
  } else if(Binary::IsComparisonOperator(op)) {
    write_effect = Compare::New(graph_,lhs,rhs,op);
  } else if(Binary::IsLogicalOperator(op)) {
    return Logical::New(graph_,lhs,rhs,op);
  } else {
    lava_die();
    return NULL;
  }

  env()->effect()->UpdateWriteEffect(write_effect);
  return write_effect;
}

// ========================================================================
// Ternary Node
// ========================================================================
Expr* GraphBuilder::NewTernary ( Expr* cond , Expr* lhs, Expr* rhs, const BytecodeLocation& pc ) {
  if(auto new_node = folder_chain_->Fold(graph_,TernaryFolderData{cond,lhs,rhs}); new_node)
    return new_node;

  { // do a guess based on type trace
    auto tt = runtime_trace_.GetTrace(pc.address());
    if(tt) {
      auto a1 = tt->data[0]; // condition's value
      cond = AddTypeFeedbackIfNeed(cond,a1,pc);
      if( auto bval = false; TPKind::ToBoolean(MapValueToTypeKind(a1),&bval) )
        return bval ? lhs : rhs;
    }
  }
  // Fallback
  return Ternary::New(graph_,cond,lhs,rhs);
}

// ========================================================================
// Phi
// ========================================================================
Expr* GraphBuilder::NewPhi( Expr* lhs , Expr* rhs , ControlFlow* region ) {
  if( auto n = folder_chain_->Fold(graph_,PhiFolderData{lhs,rhs,region}); n)
    return n;
  else
    return Phi::New(graph_,lhs,rhs,region->As<Merge>());
}

// ========================================================================
// Iterator
// ========================================================================
Expr* GraphBuilder::NewItrNew( Expr* object , const BytecodeLocation& pc ) {
  (void)pc;

  auto inew = ItrNew::New(graph_,object);
  env()->effect()->UpdateWriteEffect(inew);
  return inew;
}

Expr* GraphBuilder::NewItrNext( Expr* object , const BytecodeLocation& pc ) {
  (void)pc;

  auto inext = ItrNext::New(graph_,object);
  env()->effect()->UpdateWriteEffect(inext);
  return inext;
}

Expr* GraphBuilder::NewItrTest( Expr* object , const BytecodeLocation& pc ) {
  (void)pc;

  auto itest = ItrTest::New(graph_,object);
  env()->effect()->UpdateWriteEffect(itest);
  return itest;
}

Expr* GraphBuilder::NewItrDeref( Expr* object , const BytecodeLocation& pc ) {
  (void)pc;

  auto ideref = ItrDeref::New(graph_,object);
  env()->effect()->UpdateWriteEffect(ideref);
  return ideref;
}

// ========================================================================
// Intrinsic Call Node
// ========================================================================
Expr* GraphBuilder::NewICall( std::uint8_t a1 , std::uint8_t a2 , std::uint8_t a3 ,
                                                                  bool tcall ,
                                                                  const BytecodeLocation& pc ) {
  IntrinsicCall ic = static_cast<IntrinsicCall>(a1);
  auto base = a2; // new base to get value from current stack
  auto node = ICall::New(graph_,ic,tcall);
  for( std::uint8_t i = 0 ; i < a3 ; ++i ) {
    node->AddArgument(StackGet(i,base));
  }
  lava_debug(NORMAL,lava_verify(GetIntrinsicCallArgumentSize(ic) == a3););

  if(auto ret = folder_chain_->Fold(graph_,ExprFolderData{node}); ret)
    return ret;
  else {
    if(auto ret = LowerICall(node,pc); ret)
      return ret;
    else
      return node;
  }
}

Expr* GraphBuilder::LowerICall( ICall* node , const BytecodeLocation& pc ) {
  return NULL;
}

// ====================================================================
// Property Get/Set
// ====================================================================
Expr* GraphBuilder::TryFoldTypedPGet ( Expr* object , Expr* key , const BytecodeLocation& bc ) {
  (void)bc;
  lava_debug(CRAZY,lava_verify(GetTypeInference(object) == TPKIND_OBJECT););

  // assumption is object is typped, regardless via a GUARD or naturally typped
  // try to fold the reference lookup here
  auto ref = folder_chain_->Fold(graph_,ObjectFindFolderData{object,key,env()->effect()->write_effect()});
  if(!ref) {
    // create a ObjectFind node
    auto of = ObjectFind::New(graph_,object,key);
    env()->effect()->AddReadEffect(of);
    // try to fold the newly created reference node
    ref = folder_chain_->Fold(graph_,ExprFolderData{of});
  }
  // try to fold the get operation
  if(auto new_ret = folder_chain_->Fold(graph_,
        ObjectRefGetFolderData{ref,env()->effect()->write_effect()}); new_ret) {
    return new_ret;
  } else {
    auto ret = ObjectRefGet::New(graph_,ref);
    env()->effect()->AddReadEffect(ret);
    return ret;
  }

  lava_die(); return NULL;
}

Expr* GraphBuilder::TrySpeculativePGet( Expr* object , Expr* key , const BytecodeLocation& bc ) {
  lava_debug(NORMAL,lava_verify(key->Is<StringNode>()););
  if(CheckType(&object,TPKIND_OBJECT,1,bc)) {
    return TryFoldTypedPGet(object,key,bc);
  }
  return NULL;
}

Expr* GraphBuilder::NewPGetFallback( Expr* object , Expr* key , const BytecodeLocation& bc ) {
  auto ret = PGet::New(graph_,object,key);
  auto cp = GenerateCheckpoint(bc);
  cp->AddOperand(ret);
  env()->effect()->UpdateWriteEffect(ret);
  return ret;
}

Expr* GraphBuilder::NewPGet( Expr* object , Expr* key , const BytecodeLocation& bc ) {
  if(auto ret = TrySpeculativePGet(object,key,bc); ret)
    return ret;
  else
    return NewPGetFallback(object,key,bc);
}

Expr* GraphBuilder::TryFoldTypedPSet ( Expr* object , Expr* key , Expr* value ,
                                                                   const BytecodeLocation& bc ) {
  (void)bc;
  lava_debug(CRAZY,lava_verify(GetTypeInference(object) == TPKIND_OBJECT););

  auto ref = folder_chain_->Fold(graph_,
      ObjectFindFolderData{object,key,env()->effect()->write_effect()});
  if(!ref) {
    auto of = ObjectFind::New(graph_,object,key);
    env()->effect()->AddReadEffect(of);
    ref = folder_chain_->Fold(graph_,ExprFolderData{of});
  }
  if(auto new_ret = folder_chain_->Fold(graph_,
        ObjectRefSetFolderData{ref,value,env()->effect()->write_effect()}); new_ret) {
    return new_ret; // store collapsing
  } else {
    auto ret = ObjectRefSet::New(graph_,ref,value);
    env()->effect()->UpdateWriteEffect(ret);
    return ret;
  }

  lava_die(); return NULL;
}

Expr* GraphBuilder::TrySpeculativePSet( Expr* object , Expr* key , Expr* value ,
                                                                   const BytecodeLocation& bc ) {
  lava_debug(NORMAL,lava_verify(key->Is<StringNode>()););
  if(CheckType(&object,TPKIND_OBJECT,0,bc)) {
    return TryFoldTypedPSet(object,key,value,bc);
  }
  return NULL;
}

Expr* GraphBuilder::NewPSetFallback( Expr* object , Expr* key , Expr* value ,
                                                                const BytecodeLocation& bc ) {
  auto ret = PSet::New(graph_,object,key,value);
  auto cp = GenerateCheckpoint(bc);
  cp->AddOperand(ret);
  env()->effect()->UpdateWriteEffect(ret);
  return ret;
}

Expr* GraphBuilder::NewPSet( Expr* object , Expr* key , Expr* value , const BytecodeLocation& bc ) {
  if(auto ret = TrySpeculativePSet(object,key,value,bc); ret)
    return ret;
  else
    return NewPSetFallback(object,key,value,bc);
}
// ====================================================================
// Index Get/Set
// ====================================================================

// This function is used to generate a raw unboxed index in int32 format. Noted, this
// is a signed integer. This integer will be used tfor ListIndex node for indicating
// the index of the node. And it also helps us to do double narrowing later on when
// inside of the loop.
Expr* GraphBuilder::GenerateRawIndex( Expr* index , const BytecodeLocation& bc ) {
  (void)bc;
  auto tp = GetTypeInference(index);
  lava_debug(CRAZY, auto ret = false;
    lava_verify(TPKind::Contain(TPKIND_NUMBER,tp,&ret));
    lava_verify(ret);
  );
  // unbox the value from number back to whatever it is
  auto ub   = NewUnboxNode( graph_ , index , tp );
  // cast it to the int64 value for indexing purpose
  if(tp == TPKIND_INT64) {
    return ub;
  } else {
    auto idx = Float64ToInt64::New(graph_,ub);
    if(auto folded_idx = folder_chain_->Fold(graph_,ExprFolderData{idx}); folded_idx)
      return folded_idx;
    else
      return idx;
  }
}

Expr* GraphBuilder::TryFoldTypedISet( Expr* object , Expr* index , Expr* value ,
                                                                   const BytecodeLocation& bc ) {
  (void)bc;
  lava_debug(CRAZY,lava_verify(GetTypeInference(object) == TPKIND_LIST););
  index = GenerateRawIndex(index,bc);
  auto ref = folder_chain_->Fold(graph_,
      ListIndexFolderData{object,index,env()->effect()->write_effect()});
  if(!ref) {
    auto of = ListIndex::New(graph_,object,index);
    env()->effect()->AddReadEffect(of);
    ref = folder_chain_->Fold(graph_,ExprFolderData{of});
  }
  if(auto new_ret = folder_chain_->Fold(graph_,
        ListRefSetFolderData{ref,value,env()->effect()->write_effect()}); new_ret) {
    return new_ret;
  } else {
    auto ret = ListRefSet::New(graph_,ref,value);
    env()->effect()->UpdateWriteEffect(ret);
    return ret;
  }
}

Expr* GraphBuilder::TrySpeculativeISet( Expr* object, Expr* index, Expr* value ,
                                                                   const BytecodeLocation& bc ) {
  if(CheckType(&object,TPKIND_LIST,0,bc)) {
    if(bc.opcode() != BC_IDXSETI) {
      if(!CheckType(&index,TPKIND_NUMBER,1,bc)) return NULL;
    }
    return TryFoldTypedISet(object,index,value,bc);
  }
  return NULL;
}

Expr* GraphBuilder::NewISetFallback( Expr* object, Expr* index, Expr* value , const BytecodeLocation& bc ) {
  auto ret = ISet::New(graph_,object,index,value);
  auto cp  = GenerateCheckpoint(bc);
  cp->AddOperand(ret);
  env()->effect()->UpdateWriteEffect(ret);
  return ret;
}

Expr* GraphBuilder::NewISet( Expr* object, Expr* index, Expr* value, const BytecodeLocation& bc ) {
  if(auto ret = TrySpeculativeISet(object,index,value,bc); ret)
    return ret;
  else
    return NewISetFallback(object,index,value,bc);
}


Expr* GraphBuilder::TryFoldTypedIGet( Expr* object , Expr* index , const BytecodeLocation& bc ) {
  (void)bc;
  lava_debug(CRAZY,lava_verify(GetTypeInference(object) == TPKIND_LIST););
  index = GenerateRawIndex(index,bc);
  auto ref = folder_chain_->Fold(graph_,
      ListIndexFolderData{object,index,env()->effect()->write_effect()});
  if(!ref) {
    auto of = ListIndex::New(graph_,object,index);
    env()->effect()->AddReadEffect(of);
    ref = folder_chain_->Fold(graph_,ExprFolderData{of});
  }

  if(auto new_ret = folder_chain_->Fold(graph_,
        ListRefGetFolderData{ref,env()->effect()->write_effect()}); new_ret) {
    return new_ret;
  } else {
    auto ret = ListRefGet::New(graph_,ref);
    env()->effect()->AddReadEffect(ret);
    return ret;
  }
}

Expr* GraphBuilder::TrySpeculativeIGet( Expr* object, Expr* index, const BytecodeLocation& bc ) {
  if(CheckType(&object,TPKIND_LIST  ,1,bc)) {
    if(bc.opcode() != BC_IDXGETI) {
      if(!CheckType(&index,TPKIND_NUMBER,2,bc)) return NULL;
    }
    return TryFoldTypedIGet(object,index,bc);
  }
  return NULL;
}

Expr* GraphBuilder::NewIGetFallback( Expr* object, Expr* index , const BytecodeLocation& bc ) {
  auto ret = IGet::New(graph_,object,index);
  auto cp  = GenerateCheckpoint(bc);
  cp->AddOperand(ret);
  env()->effect()->UpdateWriteEffect(ret);
  return ret;
}

Expr* GraphBuilder::NewIGet( Expr* object, Expr* index, const BytecodeLocation& bc ) {
  if(auto ret = TrySpeculativeIGet(object,index,bc); ret)
    return ret;
  else
    return NewIGetFallback(object,index,bc);
}

// ========================================================================
// Global Variable
// ========================================================================
void GraphBuilder::NewGGet( std::uint8_t a1 , std::uint16_t a2 , bool sso ) {
  auto str  = NewStr(a2,sso);
  auto node = env()->GetGlobal( str.data , str.length , [=]() { return sso ? NewSSO(a2) : NewString(a2); } );
  StackSet(a1,node);
}

void GraphBuilder::NewGSet( std::uint16_t a1 , std::uint8_t a2 , bool sso ) {
  auto str  = NewStr(a1,sso);
  env()->SetGlobal( str.data , str.length , [=]() { return sso ? NewSSO(a1) : NewString(a1); } , StackGet(a2) );
}

// ========================================================================
// UpValue
// ========================================================================
void GraphBuilder::NewUGet( std::uint8_t a1 , std::uint8_t a2 ) {
  auto node = env()->GetUpValue(a2);
  StackSet(a1,node);
}

void GraphBuilder::NewUSet( std::uint8_t a1, std::uint8_t a2 ) {
  env()->SetUpValue(a1,StackGet(a2));
}

// =========================================================================
// Checkpoint
// =========================================================================
Checkpoint* GraphBuilder::GenerateCheckpoint( const BytecodeLocation& pc ) {
  // create the checkpoint object
  auto cp = Checkpoint::New(graph_,graph_->zone()->New<IRInfo>(method_index(),pc));

  { // capture all stack recorded value
    const std::uint32_t* pc_start = func_info().prototype->code_buffer();
    std::uint32_t            diff = pc.address() - pc_start;
    std::uint8_t           offset = func_info().prototype->GetRegOffset(diff);
    const std::uint32_t stack_end = func_info().base + offset;
    // record all the stack value into the checkpoint object
    for( std::uint32_t i = 0 ; i < stack_end ; ++i ) {
      auto node = vstk()->at(i);
      if(node) cp->AddStackSlot(node,i);
    }
  }

  // update the checkpoint node from the environment object
  env()->UpdateCP(cp);
  return cp;
}

void GraphBuilder::GenerateMergeCheckpoint( const Environment& that ,
                                            const BytecodeLocation& bc ) {
  if(!that.cp()->IsIdentical(env()->cp())) {
    GenerateCheckpoint(bc);
  }
}

Checkpoint* GraphBuilder::InitCheckpoint() {
  return Checkpoint::New(graph_,graph_->zone()->New<IRInfo>(method_index(),BytecodeLocation()));
}


// ====================================================================
// Misc
// ====================================================================
void GraphBuilder::PatchExitNode( ControlFlow* succ , ControlFlow* fail ) {
  for( auto &e : func_info().return_list ) {
    succ->AddBackwardEdge(e);
  }

  // patch all the failed node
  for( auto &e : func_info().guard_list ) {
    fail->AddOperand(e);
  }
}

// ====================================================================
// Branch Phi
// ====================================================================

void GraphBuilder::GeneratePhi( ValueStack* dest , const ValueStack& lhs , const ValueStack& rhs ,
                                                                           std::size_t base ,
                                                                           std::size_t limit,
                                                                           ControlFlow* region ) {
  const std::size_t sz = std::min(lhs.size(),rhs.size());
  for( std::size_t i = base ; i < sz && limit != 0 ; ++i , --limit ) {
    Expr* l = lhs[i];
    Expr* r = rhs[i];
    if(l && r) dest->at(i) = NewPhi(l,r,region);
  }
}

// key function to merge the effect and value from 2 environments. called at every region merge
void GraphBuilder::InsertPhi( Environment* lhs , Environment* rhs , ControlFlow* region ) {
  // 1. generate merge for both region's root effect group
  Effect::MergeEffect(*lhs->effect(),*rhs->effect(),env()->effect(),graph_,region->As<EffectMergeRegion>());
  // 2. generate phi for all the stack value
  GeneratePhi(vstk(),*lhs->stack(),*rhs->stack(),func_info().base,
      func_info().prototype->max_local_var_size(),region);
  // 3. generate phi for all the upvalue
  GeneratePhi(upval(),*lhs->upvalue(),*rhs->upvalue(),0, kMaxUpValueSize , region);
  // 4. generate phi for all the global values
  {
    Environment::GlobalMap temp(temp_zone()); temp.reserve( lhs->global()->size() );
    // scanning the left hand side global variables
    for( auto &e : *lhs->global() ) {
      auto itr = std::find(rhs->global()->begin(),rhs->global()->end(),e.name);
      Expr* lnode = NULL;
      Expr* rnode = NULL;
      if(itr != rhs->global()->end()) {
        lnode = e.value;
        rnode = itr->value;
        // add it to the temporarily list
        temp.push_back(Environment::GlobalVar(e.name,NewPhi(lnode,rnode,region)));
      }
      // if this global variable is not found, then just do nothing since this
      // brings this global variable back to the original state which requires
      // a global variable read from memory
    }

    // for other globals that appear in rhs , we don't do anything for the new global
    // variable array, then all of them becomes unclear state again and require a
    // GGet node generated whenever a query/lookup for those global variable comes.
    // use the new global list
    env()->set_global(std::move(temp));
  }
}

void GraphBuilder::PatchUnconditionalJump( UnconditionalJumpList* jumps , ControlFlow* region ,
                                                                          const BytecodeLocation& pc ) {
  for( auto& e : *jumps ) {
    lava_debug(NORMAL,lava_verify(e.pc == pc.address()););
    lava_verify(e.node->TrySetTarget(pc.address(),region));
    region->AddBackwardEdge(e.node);
    InsertPhi(env(),&e.env,region);
  }
  jumps->clear();
}

GraphBuilder::StopReason GraphBuilder::BuildLogic( BytecodeIterator* itr ) {
  lava_debug(NORMAL,lava_verify(itr->opcode() == BC_AND || itr->opcode() == BC_OR););
  bool op_and = itr->opcode() == BC_AND ? true : false;
  std::uint8_t lhs,rhs,dummy;
  std::uint32_t pc;
  itr->GetOperand(&lhs,&rhs,&dummy,&pc);
  // where we should end for the other part of the logical cominator
  const std::uint32_t* end_pc = itr->OffsetAt(pc);
  Expr* lhs_expr = StackGet(lhs);
  lava_debug(NORMAL,lava_verify(lhs_expr););
  StackSet(rhs,lhs_expr);
  { // evaluate the rhs
    itr->Move();
    StopReason reason = BuildBasicBlock(itr,end_pc);
    lava_verify(reason == STOP_END);
  }
  Expr* rhs_expr = StackGet(rhs);
  lava_debug(NORMAL,lava_verify(rhs_expr););
  if(op_and)
    StackSet(rhs,NewBinary(lhs_expr,rhs_expr,Binary::AND,itr->bytecode_location()));
  else
    StackSet(rhs,NewBinary(lhs_expr,rhs_expr,Binary::OR ,itr->bytecode_location()));
  return STOP_SUCCESS;
}

GraphBuilder::StopReason GraphBuilder::BuildTernary( BytecodeIterator* itr ) {
  lava_debug(NORMAL,lava_verify(itr->opcode() == BC_TERN););
  std::uint8_t cond , result , dummy;
  std::uint32_t offset;
  std::uint16_t final_cursor;
  Expr* lhs, *rhs;

  itr->GetOperand(&cond,&result,&dummy,&offset);
  { // evaluate the fall through branch
    for( itr->Move() ; itr->HasNext() ; ) {
      if(itr->opcode() == BC_JMP) break; // end of the first ternary fall through branch
      if(BuildBytecode(itr) == STOP_BAILOUT) return STOP_BAILOUT;
    }

    lava_debug(NORMAL,lava_verify(itr->opcode() == BC_JMP););
    itr->GetOperand(&final_cursor);
    lhs = StackGet(result);
  }

  const std::uint32_t* end_pc = itr->OffsetAt(final_cursor);
  { // evaluate the jump branch
    lava_debug(NORMAL,StackReset(result););
    lava_debug(NORMAL,itr->Move();lava_verify(itr->pc() == itr->OffsetAt(offset)););

    while(itr->HasNext()) {
      if(itr->pc() == end_pc) break;
      if(BuildBytecode(itr) == STOP_BAILOUT) return STOP_BAILOUT;
    }

    rhs = StackGet(result);
    lava_debug(NORMAL,lava_verify(rhs););
  }

  StackSet(result, NewTernary(StackGet(cond),lhs,rhs,itr->bytecode_location()));
  return STOP_SUCCESS;
}

GraphBuilder::StopReason GraphBuilder::BuildIfBlock( BytecodeIterator* itr , const std::uint32_t* pc ) {
  while( itr->HasNext() ) {
    // check whether we reache end of PC where we suppose to stop
    if(pc == itr->pc())
      return STOP_END;
    else if(itr->opcode() == BC_JMP)
      return STOP_JUMP;
    else if(BuildBytecode(itr) == STOP_BAILOUT)
      return STOP_BAILOUT;
  }
  if(pc == itr->pc()) return STOP_END;

  lava_unreachF("cannot reach here since it is end of the stream %p:%p",itr->pc(),pc);
  return STOP_EOF;
}

GraphBuilder::StopReason
GraphBuilder::BuildIf( BytecodeIterator* itr ) {
  lava_debug(NORMAL,lava_verify(itr->opcode() == BC_JMPF););

  // do the normal branch
  std::uint8_t cond;    // condition's register
  std::uint16_t offset; // jump target when condition evaluated to be false
  itr->GetOperand(&cond,&offset);

  // create the leading If node
  If*      if_region    = If::New(graph_,StackGet(cond),region());
  IfFalse* false_region = IfFalse::New(graph_,if_region);
  IfTrue*  true_region  = IfTrue::New(graph_,if_region);
  ControlFlow* lhs      = NULL;
  ControlFlow* rhs      = NULL;
  IfMerge* merge        = IfMerge::New(graph_);
  if_region->set_merge(merge);

  // duplicate an environment for branching into the if_true branch
  Environment   true_env(*env(),Environment::LEXICAL_SCOPE);
  std::uint16_t final_cursor;
  bool          have_false_branch;

  // 1. Build code inside of the *true* branch and it will also help us to
  //    identify whether we have dangling elif/else branch
  itr->Move();

  // backup the old stack and use the new stack to do simulation
  BACKUP_ENVIRONMENT(&true_env,this) {
    // swith to a true region
    set_region(true_region);
    // generate a separator effect
    env()->InsertBranchStartEffect(true_region);
    {
      StopReason reason = BuildIfBlock(itr,itr->OffsetAt(offset));
      if(reason == STOP_BAILOUT) {
        return STOP_BAILOUT;
      } else if(reason == STOP_JUMP) {
        // we do have a none empty false_branch
        have_false_branch = true;
        lava_debug(NORMAL,lava_verify(itr->opcode() == BC_JMP););
        itr->GetOperand(&final_cursor);
      } else {
        lava_debug(NORMAL,lava_verify(reason == STOP_END););
        have_false_branch = false;
      }
    }
    rhs = region();
  }
  // 2. Build code inside of the *false* branch
  {
    // build the false branch regardless whether it has one or not
    if(have_false_branch) {
      set_region(false_region);
      // generate a sperator region
      env()->InsertBranchStartEffect(region());
      // goto false branch
      itr->BranchTo(offset);
      if(BuildIfBlock(itr,itr->OffsetAt(final_cursor)) == STOP_BAILOUT)
        return STOP_BAILOUT;
      lhs = region();
    } else {
      // reach here means we don't have a elif/else branch
      final_cursor = offset;
      lhs          = false_region;
      env()->InsertBranchStartEffect(false_region);
    }
  }
  // 3. set the merge backward edge
  merge->AddBackwardEdge(lhs);
  merge->AddBackwardEdge(rhs);
  itr->BranchTo(final_cursor);
  set_region(merge);
  // 4. handle PHI node
  InsertPhi(env(),&true_env,merge);
  // 5. generate new Checkpoint node eagerly after the merge if needed
  GenerateMergeCheckpoint(true_env,itr->bytecode_location());

  return STOP_SUCCESS;
}

GraphBuilder::StopReason
GraphBuilder::BuildLoopBlock( BytecodeIterator* itr ) {
  while(itr->HasNext()) {
    if(IsLoopEndBytecode(itr->opcode())) {
      return STOP_SUCCESS;
    } else if(BuildBytecode(itr) == STOP_BAILOUT) {
      return STOP_BAILOUT;
    }
  }

  lava_unreachF("%s","must be closed by BC_FEEND/BC_FEND1/BC_FEND2/BC_FEVREND");
  return STOP_BAILOUT;
}

// ===============================================================================
// Loop Phi
// ===============================================================================

void GraphBuilder::GenerateLoopInductionVariable( Loop* loop ) {
  // 1. stacked variables
  {
    const auto len = func_info().current_loop_header()->phi.var.size();
    for( std::size_t i = 0 ; i < len ; ++i ) {
      if(func_info().current_loop_header()->phi.var[i]) {
        Expr* old = StackGet(static_cast<std::uint32_t>(i));
        lava_debug(NORMAL,lava_verify(old););
        auto iv  = LoopIV::New(graph_,loop);
        iv->AddOperand(old);
        StackSet(static_cast<std::uint32_t>(i),iv);
        func_info().current_loop().AddPhi(LoopInfo::VAR,i,iv);
      }
    }
  }
  // 2. upvalue
  {
    const auto len = func_info().prototype->upvalue_size();
    for( std::size_t i = 0 ; i < len ; ++i ) {
      if(func_info().current_loop_header()->phi.uv[i]) {
        auto uget = env()->GetUpValue(static_cast<std::uint32_t>(i));
        auto phi  = Phi::New(graph_,loop);
        phi->AddOperand(uget);
        env()->upvalue()->at(i) = phi; // modify the old value to be new Phi node
        func_info().current_loop().AddPhi(LoopInfo::UPVALUE,i,phi);
      }
    }
  }
  // 3. globals
  {
    for( auto &e : func_info().current_loop_header()->phi.glb ) {
      auto gget = env()->GetGlobal(e.data,e.length,[=](){ return NewString(e); });
      auto phi  = Phi::New(graph_,loop);
      phi->AddOperand(gget);
      // find the exact places of the global variables in globs and then
      // insert the new phi into the places
      std::uint32_t index = 0;
      {
        auto itr = std::find(env()->global()->begin(),env()->global()->end(),e);
        lava_verify(itr != env()->global()->end());
        itr->value = phi;
        index = std::distance(env()->global()->begin(),itr);
      }
      // add it back to the tracking phi list for later patching
      func_info().current_loop().AddPhi(LoopInfo::GLOBAL,index,phi);
    }
  }
}

void GraphBuilder::PatchLoopInductionVariable() {
  for( auto & e: func_info().current_loop().phi_list ) {
    auto phi = e.phi;
    switch(e.type) {
      case LoopInfo::VAR:
        if(phi->IsUsed()) {
          auto node = StackGet(e.idx);
          lava_debug(NORMAL,lava_assert (phi != node,"What happened here may be the bytecode make "
                                                     "bytecode_analyzer.cc failed. It detects a node "
                                                     "that is modified inside of a loop but also in  "
                                                     "its parental lexical scope. However this is not "
                                                     "true since the variable is actually not referenced "
                                                     "inside of the loop nest"););
          phi->AddOperand(node);
          if(auto p = folder_chain_->Fold(graph_,ExprFolderData{phi}); p) phi->Replace(p);
        } else {
          PhiBase::RemovePhiFromRegion(phi);
        }
        break;
      case LoopInfo::GLOBAL:
        {
          auto v = env()->global()->at(e.idx).value;
          lava_debug(NORMAL,lava_verify(phi != v););
          phi->AddOperand(v);
          if(auto p = folder_chain_->Fold(graph_,ExprFolderData{phi}); p) phi->Replace(p);
        }
        break;
      default:
        {
          auto v = env()->upvalue()->at(e.idx);
          lava_debug(NORMAL,lava_verify(phi != v););
          phi->AddOperand(v);
          if(auto p = folder_chain_->Fold(graph_,ExprFolderData{phi}); p) phi->Replace(p);
        }
        break;
    }
  }
  func_info().current_loop().phi_list.clear();
}

// ============================================================
// Loop
// ============================================================
Expr* GraphBuilder::BuildLoopEndCondition( BytecodeIterator* itr , ControlFlow* body ) {
  (void)body;
  switch(itr->opcode()) {
    case BC_FEND1:
      {
        std::uint8_t a1,a2,a3; std::uint32_t a4;
        itr->GetOperand(&a1,&a2,&a3,&a4);
        return NewBinary(StackGet(a1),StackGet(a2),Binary::LT,itr->bytecode_location());
      }
    case BC_FEND2:
      {
        std::uint8_t a1,a2,a3; std::uint32_t a4;
        itr->GetOperand(&a1,&a2,&a3,&a4);
        auto addition = NewBinary(StackGet(a1),StackGet(a3), Binary::ADD, itr->bytecode_location());
        StackSet(a1,addition);
        return NewBinary(StackGet(a1),StackGet(a2), Binary::LT, itr->bytecode_location());
      }
    case BC_FEEND:
      {
        std::uint8_t a1;
        std::uint16_t pc;
        itr->GetOperand(&a1,&pc);
        return NewItrNext(StackGet(a1),itr->bytecode_location());
      }
    default:
      return NewBoolean(true);
  }

  lava_die(); return NULL;
}

GraphBuilder::StopReason
GraphBuilder::BuildLoopBody( BytecodeIterator* itr , ControlFlow* loop_header ) {
  Loop*       body        = NULL;
  LoopExit*   exit        = NULL;
  auto        after       = LoopMerge::New(graph_);
  Environment loop_env   (*env(),Environment::LOOP_SCOPE);

  if(loop_header->Is<LoopHeader>()) {
    loop_header->As<LoopHeader>()->set_merge(after);
    // Only link the if_false edge back to loop header when it is actually a
    // loop header type. During OSR compilation , since we don't have a real
    // loop header , so we don't need to link it back
    after->AddBackwardEdge(loop_header);
  }

  BytecodeLocation cont_pc;
  BytecodeLocation brk_pc ;

  // backup the old environment and use a temporary environment
  BACKUP_ENVIRONMENT(&loop_env,this) {
    // entier the loop scope
    LoopScope lscope(this,itr->pc());

    // create new loop body node
    body = Loop::New(graph_);

    // set it as the current region node
    set_region(body);
    auto loop_effect_phi = env()->AsLoopEffectStart();

    // patch the loop effect phi with correct region node
    {
      body->set_loop_effect_start(loop_effect_phi);
      func_info().current_loop().loop_effect_phi = loop_effect_phi;
    }

    // generate PHI node at the head of the *block*
    GenerateLoopInductionVariable(body);

    // iterate all BC inside of the loop body
    StopReason reason = BuildLoopBlock(itr);
    if(reason == STOP_BAILOUT)
      return STOP_BAILOUT;
    lava_debug(NORMAL, lava_verify(reason == STOP_SUCCESS || reason == STOP_JUMP););
    cont_pc = itr->bytecode_location();

    // now we should stop at the FEND1/FEND2/FEEND instruction
    auto exit_cond = BuildLoopEndCondition(itr,body);
    lava_debug(NORMAL,lava_verify(!exit););

    // set up LoopExit node
    exit = LoopExit::New(graph_,exit_cond);
    body->set_loop_exit(exit);

    // connect each control flow node together
    exit->AddBackwardEdge (region());
    body->AddBackwardEdge (loop_header);
    body->AddBackwardEdge (exit);
    after->AddBackwardEdge(exit);

    // skip the last end instruction
    itr->Move();

    // patch all the Phi node
    PatchLoopInductionVariable();

    // set the loop effect phi node's backward effect operand , must be after
    // PatchLoopInductionVariable call
    func_info().current_loop().loop_effect_phi->SetBackwardEffect(env()->effect()->write_effect());

    // break should jump here which is *after* the merge region
    brk_pc = itr->bytecode_location();

    // patch all the pending continue and break node
    PatchUnconditionalJump( &func_info().current_loop().pending_continue , exit , cont_pc );
    PatchUnconditionalJump( &func_info().current_loop().pending_break    , after, brk_pc  );

    lava_debug(NORMAL,
      lava_verify(func_info().current_loop().pending_continue.empty());
      lava_verify(func_info().current_loop().pending_break.empty   ());
      lava_verify(func_info().current_loop().phi_list.empty        ());
    );
  }
  // set the current region as merged region
  set_region(after);

  // merge the loop header
  if(loop_header->Is<LoopHeader>()) InsertPhi(env(),&loop_env,after);

  return STOP_SUCCESS;
}

GraphBuilder::StopReason GraphBuilder::BuildLoop( BytecodeIterator* itr ) {
  lava_debug(NORMAL,lava_verify(IsLoopStartBytecode(itr->opcode())););
  // normal path for loop graph
  auto loop_header = LoopHeader::New(graph_,region());
  set_region(loop_header);
  if(itr->opcode() == BC_FSTART) {
    loop_header->set_condition(StackGet(kAccRegisterIndex));
  } else if(itr->opcode() == BC_FESTART) {
    std::uint8_t a1; std::uint16_t a2;
    itr->GetOperand(&a1,&a2);
    auto inew  = NewItrNew (StackGet(a1),itr->bytecode_location());
    auto itest = NewItrTest(inew        ,itr->bytecode_location());
    StackSet(a1,inew);
    loop_header->set_condition(itest);
  } else {
    lava_unreachF("should not reach here, forever loop should be "
                  "foleded before , bytecode %s",itr->opcode_name());
  }

  // skip the loop start bytecode
  itr->Move();
  return BuildLoopBody(itr,loop_header);
}

GraphBuilder::StopReason GraphBuilder::BuildBytecode( BytecodeIterator* itr ) {
  std::uint32_t a1,a2,a3,a4;
  Expr* temp;
  itr->FetchOperand(&a1,&a2,&a3,&a4);

  switch(itr->opcode()) {
    case BC_ADDRV:
    case BC_SUBRV:
    case BC_MULRV:
    case BC_DIVRV:
    case BC_MODRV:
    case BC_POWRV:
    case BC_LTRV :
    case BC_LERV :
    case BC_GTRV :
    case BC_GERV :
    case BC_EQRV :
    case BC_NERV :
      temp = NewBinary(NewNumber(a2),
                       StackGet(a3),
                       Binary::BytecodeToOperator(itr->opcode()),
                       itr->bytecode_location());
      StackSet(a1,temp);
      break;

    case BC_ADDVR:
    case BC_SUBVR:
    case BC_MULVR:
    case BC_DIVVR:
    case BC_MODVR:
    case BC_POWVR:
    case BC_LTVR :
    case BC_LEVR :
    case BC_GTVR :
    case BC_GEVR :
    case BC_EQVR :
    case BC_NEVR :
      temp = NewBinary(StackGet(a2),
                       NewNumber(a3),
                       Binary::BytecodeToOperator(itr->opcode()),
                       itr->bytecode_location());
      StackSet(a1,temp);
      break;
    case BC_ADDVV:
    case BC_SUBVV:
    case BC_MULVV:
    case BC_DIVVV:
    case BC_MODVV:
    case BC_POWVV:
    case BC_LTVV :
    case BC_LEVV :
    case BC_GTVV :
    case BC_GEVV :
    case BC_EQVV :
    case BC_NEVV :
      temp = NewBinary(StackGet(a2),
                       StackGet(a3),
                       Binary::BytecodeToOperator(itr->opcode()),
                       itr->bytecode_location());
      StackSet(a1,temp);
      break;

    case BC_EQSV:
    case BC_NESV:
      temp = NewBinary(NewString(a2),
                       StackGet(a3),
                       Binary::BytecodeToOperator(itr->opcode()),
                       itr->bytecode_location());
      StackSet(a1,temp);
      break;

    case BC_EQVS:
    case BC_NEVS:
      temp = NewBinary(StackGet(a2),
                       NewString(a3),
                       Binary::BytecodeToOperator(itr->opcode()),
                       itr->bytecode_location());
      StackSet(a1,temp);
      break;

    case BC_AND:
    case BC_OR:
      return BuildLogic(itr);

    case BC_TERN:
      return BuildTernary(itr);

    case BC_NEGATE: case BC_NOT:
      temp = NewUnary(StackGet(a2),
                      Unary::BytecodeToOperator(itr->opcode()),itr->bytecode_location());
      StackSet(a1,temp);
      break;

    case BC_MOVE:
      StackSet(a1,StackGet(a2));
      break;

    case BC_LOAD0:
    case BC_LOAD1:
    case BC_LOADN1:
      {
        std::int32_t num;
        if(itr->opcode() == BC_LOAD1)
          num = 1;
        else if(itr->opcode() == BC_LOADN1)
          num = -1;
        else
          num = 0;
        StackSet(a1,NewConstNumber(num));
      }
      break;

    case BC_LOADR:
      StackSet(a1,NewNumber(a2));
      break;

    case BC_LOADSTR:
      StackSet(a1,NewString(a2));
      break;

    case BC_LOADTRUE: case BC_LOADFALSE:
      StackSet(a1,NewBoolean(itr->opcode() == BC_LOADTRUE));
      break;

    case BC_LOADNULL:
      StackSet(a1,Nil::New(graph_));
      break;

    case BC_LOADLIST0:
      StackSet(a1,NewIRList(0));
      break;
    case BC_LOADLIST1:
      {
        auto list = NewIRList(0);
        list->Add(StackGet(a2));
        StackSet(a1,list);
      }
      break;
    case BC_LOADLIST2:
      {
        auto list = NewIRList(0);
        list->Add(StackGet(a2));
        list->Add(StackGet(a3));
        StackSet(a1,list);
      }
      break;
    case BC_NEWLIST:
      {
        auto list = NewIRList(a2);
        StackSet(a1,list);
      }
      break;
    case BC_ADDLIST:
      {
        IRList* l = StackGet(a1)->As<IRList>();
        for( std::size_t i = 0 ; i < a3 ; ++i ) {
          l->Add(StackGet(a2+i));
        }
      }
      break;

    case BC_LOADOBJ0:
      StackSet(a1,NewIRObject(0));
      break;
    case BC_LOADOBJ1:
      {
        auto obj = NewIRObject(1);
        obj->Add(StackGet(a2),StackGet(a3));
        StackSet(a1,obj);
      }
      break;
    case BC_NEWOBJ:
      StackSet(a1,NewIRObject(a2));
      break;
    case BC_ADDOBJ:
      {
        IRObject* obj = StackGet(a1)->As<IRObject>();
        obj->Add(StackGet(a2),StackGet(a3));
      }
      break;
    case BC_LOADCLS:
      StackSet(a1,Closure::New(graph_,a2));
      break;

    case BC_PROPGET:
    case BC_PROPGETSSO:
      temp = (itr->opcode() == BC_PROPGET ? NewString(a3): NewSSO   (a3));
      StackSet(a1,NewPGet(StackGet(a2),temp,itr->bytecode_location()));
      break;

    case BC_PROPSET:
    case BC_PROPSETSSO:
      temp = (itr->opcode() == BC_PROPSET ? NewString(a2): NewSSO   (a2));
      NewPSet(StackGet(a1),temp,StackGet(a3),itr->bytecode_location());
      break;

    case BC_IDXGET:
    case BC_IDXGETI:
      temp = (itr->opcode() == BC_IDXGET ? StackGet(a3) : NewConstNumber(a3));
      StackSet(a1,NewIGet(StackGet(a2),temp,itr->bytecode_location()));
      break;

    case BC_IDXSET:
    case BC_IDXSETI:
      temp = (itr->opcode() == BC_IDXSET ? StackGet(a2) : NewConstNumber(a2));
      NewISet(StackGet(a1),temp,StackGet(a3),itr->bytecode_location());
      break;

    case BC_UVGET:
      NewUGet(a1,a2);
      break;

    case BC_UVSET:
      NewUSet(a1,a2);
      break;

    case BC_GGET: case BC_GGETSSO:
      NewGGet(a1,a2,(itr->opcode() == BC_GGETSSO));
      break;

    case BC_GSET:
    case BC_GSETSSO:
      NewGSet(a1,a2,(itr->opcode() == BC_GSETSSO));
      break;

    case BC_ICALL:
    case BC_TICALL:
      StackSet(kAccRegisterIndex,NewICall(a1,
                                          a2,
                                          a3,
                                          (itr->opcode() == BC_TICALL),
                                          itr->bytecode_location()));
      break;

    case BC_TCALL:
    case BC_CALL:
      NewCall(itr);
      break;

    case BC_JMPF:
      return BuildIf(itr);

    case BC_FSTART   :
    case BC_FESTART  :
    case BC_FEVRSTART:
      return BuildLoop(itr);

    case BC_IDREF:
      lava_debug(NORMAL,lava_verify(func_info().HasLoop()););
      {
        auto iref       = NewItrDeref    (StackGet(a3),itr->bytecode_location());
        Projection* key = Projection::New(graph_,iref,ItrDeref::PROJECTION_KEY);
        Projection* val = Projection::New(graph_,iref,ItrDeref::PROJECTION_VAL);
        StackSet(a1,key);
        StackSet(a2,val);
      }
      break;

    case BC_BRK:
    case BC_CONT:
      lava_debug(NORMAL,lava_verify(func_info().HasLoop()););
      {
        /** OffsetAt(pc) returns jump target address **/
        Jump* jump = Jump::New(graph_,itr->OffsetAt(a1),region());
        set_region(jump);
        if(itr->opcode() == BC_BRK)
          func_info().current_loop().AddBreak   (jump,itr->OffsetAt(a1),*env());
        else
          func_info().current_loop().AddContinue(jump,itr->OffsetAt(a1),*env());
      }
      break;

    case BC_RET:
    case BC_RETNULL:
      {
        JumpWithValue* ret;
        Expr* retval = itr->opcode() == BC_RET ? StackGet(kAccRegisterIndex) : Nil::New(graph_);

        // generate a jump value node instead of return node when we are in
        // 1) inline frame
        // 2) not a tail call since tail call should directly return
        if(IsInlineFrame() && !func_info().tcall) {
          ret  = JumpValue::New(graph_,retval,region());
          set_region(ret);
          func_info().return_list.push_back(ret);
        } else {
          ret  = Return::New   (graph_,retval,region());
          set_region(ret);
          top_func ().return_list.push_back(ret);
        }
      }
      break;

    default:
      lava_unreachF("ouch, bytecode %s cannot reach here !",itr->opcode_name());
      break;
  }

  itr->Move(); // consume this bytecode
  return STOP_SUCCESS;
}

GraphBuilder::StopReason GraphBuilder::BuildBasicBlock( BytecodeIterator* itr , const std::uint32_t* end_pc ) {
  while(itr->HasNext()) {
    if(itr->pc() == end_pc) return STOP_END;
    // save the opcode from the bytecode iterator
    auto opcode = itr->opcode();
    // build this instruction
    if(BuildBytecode(itr) == STOP_BAILOUT) return STOP_BAILOUT;
    // check if last opcode is break or continue which is unconditional
    // jump. so we can just abort the construction of this basic block
    if(IsBlockJumpBytecode(opcode)) return STOP_JUMP;
  }
  return STOP_SUCCESS;
}

bool GraphBuilder::Build( const Handle<Prototype>& entry , Graph* graph ) {
  graph_ = graph;
  zone_  = graph->zone();
  // start the basic block building setup the main environment object
  Environment root_env(temp_zone(),this);

  // 1. create the start and end region
  Start* start   = Start::New(graph_,init_checkpoint_,init_barrier_);
  End*  end      = NULL;
  Fail* fail     = Fail::New(graph_);
  Success* succ  = Success::New(graph_);
  Region* region = Region::New(graph_,start);

  BACKUP_ENVIRONMENT(&root_env,this) {
    // enter into the top level function
    FuncScope scope(this,entry,region);
    // setup the bytecode iterator
    auto itr = entry->GetBytecodeIterator();
    // set the current region
    set_region(region);
    // start to execute the build basic block
    if(BuildBasicBlock(&itr) == STOP_BAILOUT)
      return false;
    // finish all the exit node work
    PatchExitNode(succ,fail);
    end = End::New(graph_,succ,fail);
  }

  // initialize the graph
  graph->Initialize(start,end);
  return true;
}

void GraphBuilder::BuildOSRLocalVariable() {
  auto loop_header = func_info().bc_analyze.LookUpLoopHeader(func_info().osr_start);
  lava_debug(NORMAL,lava_verify(loop_header););
  lava_foreach( auto v , BytecodeAnalyze::LocalVariableIterator(loop_header->bb,func_info().bc_analyze) ) {
    lava_debug(NORMAL,lava_verify(!vstk()->at(v)););
    vstk()->at(v) = OSRLoad::New(graph_,v);
  }
}

GraphBuilder::StopReason GraphBuilder::BuildOSRLoop( BytecodeIterator* itr ) {
  lava_debug(NORMAL, lava_verify(func_info().IsOSR());
                     lava_verify(func_info().osr_start == itr->pc()););
  return BuildLoopBody(itr,region());
}

void GraphBuilder::SetupOSRLoopCondition( BytecodeIterator* itr ) {
  // now we should stop at the FEND1/FEND2/FEEND instruction
  if(itr->opcode() == BC_FEND1) {
    std::uint8_t a1,a2,a3; std::uint32_t a4;
    itr->GetOperand(&a1,&a2,&a3,&a4);
    auto comparison = NewBinary(StackGet(a1), StackGet(a2), Binary::LT , itr->bytecode_location());
    StackSet(kAccRegisterIndex,comparison);
  } else if(itr->opcode() == BC_FEND2) {
    std::uint8_t a1,a2,a3; std::uint32_t a4;
    itr->GetOperand(&a1,&a2,&a3,&a4);
    // the addition node will use the PHI node as its left hand side
    auto addition   = NewBinary(StackGet(a1),StackGet(a3), Binary::ADD, itr->bytecode_location());
    StackSet(a1,addition);
    auto comparison = NewBinary(StackGet(a1),StackGet(a2), Binary::LT , itr->bytecode_location());
    StackSet(kAccRegisterIndex,comparison);
  } else if(itr->opcode() == BC_FEEND) {
    std::uint8_t a1;
    std::uint16_t pc;
    itr->GetOperand(&a1,&pc);
    StackSet(a1,NewItrNext(StackGet(a1),itr->bytecode_location()));
  } else {
    lava_debug(NORMAL,lava_verify(itr->opcode() == BC_FEVREND););
  }
}

GraphBuilder::StopReason GraphBuilder::PeelOSRLoop( BytecodeIterator* itr ) {
  bool is_osr = true;
  UnconditionalJumpList temp_break;
  // check whether we have any parental loop , if so we peel everyone
  // until we hit the end
  do {
    if(is_osr) {
      // build the OSR loop
      if(BuildOSRLoop(itr) == STOP_BAILOUT) return STOP_BAILOUT;
      is_osr = false;
    } else {
      // rebuild the loop
      {
        StopReason reason = BuildLoop(itr);
        if(reason == STOP_BAILOUT) return STOP_BAILOUT;
      }
      // now we can link the peeled break part to here
      PatchUnconditionalJump(&temp_break,region(),itr->bytecode_location());
    }
    if(func_info().HasLoop()) {
      // now start to peel the parental loop's rest instructions/bytecodes
      // the input iterator should sits right after the loop end bytecode
      // of the nested loop
      while(itr->HasNext()) {
        if(IsLoopEndBytecode(itr->opcode())) {
          break;
        }
        auto opcode = itr->opcode();
        if(BuildBytecode(itr) == STOP_BAILOUT) return STOP_BAILOUT;

        if(IsBlockJumpBytecode(opcode)) {
          // skip util we hit a loop end bytecode
          lava_verify( itr->SkipTo(
            []( BytecodeIterator* itr ) {
              return !IsLoopEndBytecode(itr->opcode());
            }
          ));
          break;
        }
      }
      // now we should end up with the loop end bytecode and this bytecode will
      // be ignored entirely since we will rewind the iterator back to the very
      // first instruction of the parental loop
      lava_debug(NORMAL,lava_verify(IsLoopEndBytecode(itr->opcode())););
      // setup osr-loop's initial condition
      SetupOSRLoopCondition(itr);
      // skip the last loop end bytecode
      itr->Move();
      // patch continue region inside of the peel part
      if(!func_info().current_loop().pending_continue.empty()) {
        // create a new region lazily
        Region* r = Region::New(graph_,region());
        PatchUnconditionalJump(&(func_info().current_loop().pending_continue),
                               r,
                               itr->bytecode_location());
        set_region(r);
      }
      // save peeled part's all break
      temp_break.swap(func_info().current_loop().pending_break);
      // now we rewind the iterator to the start of this loop and regenerate everything
      // again as natural fallthrough
      //
      // The start instruction for current loop doesn't include BC_FSTART/FEVRSTART/FESTART
      // so we need to backward by 1
      itr->BranchTo( func_info().current_loop_header()->start - 1 );
      // leave the current loop
      func_info().LeaveLoop();
    } else {
      break;
    }
  } while(true);
  return STOP_SUCCESS;
}

GraphBuilder::StopReason
GraphBuilder::BuildOSRStart( const Handle<Prototype>& entry ,  const std::uint32_t* pc , Graph* graph ) {
  graph_ = graph;
  zone_  = graph->zone();

  // initialize the environment for graph building
  Environment root_env(temp_zone(),this);

  // 1. create OSRStart node which is the entry of OSR compilation
  OSRStart* start = OSRStart::New(graph,init_checkpoint_,init_barrier_);
  OSREnd*   end   = NULL;
  Region*  header = Region::New(graph,start);
  Fail*      fail = Fail::New(graph);
  Success*   succ = Success::New(graph);

  BACKUP_ENVIRONMENT(&root_env,this) {
    // set up the OSR scope
    OSRScope scope(this,entry,header,pc);
    // set up OSR local variable
    BuildOSRLocalVariable();
    // craft a bytecode iterator *starts* at the OSR instruction entry
    // which should be a loop start instruction like FESTART,FSTART,FEVRSTART
    const std::uint32_t* code_buffer   = entry->code_buffer();
    const std::size_t code_buffer_size = entry->code_buffer_size();
    lava_debug(NORMAL,lava_verify(pc >= code_buffer););
    BytecodeIterator itr(code_buffer,code_buffer_size);
    itr.BranchTo(pc);
    lava_debug(NORMAL,lava_verify(itr.HasNext()););
    // peel all nested loop until hit the outermost one
    if(PeelOSRLoop(&itr) == STOP_BAILOUT) return STOP_BAILOUT;
    // add a trap right after the OSR generation
    fail->AddBackwardEdge(Trap::New(graph_,GenerateCheckpoint(itr.bytecode_location()),region()));
    // finish all exit node work
    PatchExitNode(succ,fail);
    // lastly create the end node for the osr graph
    end = OSREnd::New(graph_,succ,fail);
  }

  // initialize the graph via OSR compilation
  graph->Initialize(start,end);
  return STOP_SUCCESS;
}

bool GraphBuilder::BuildOSR( const Handle<Prototype>& entry ,
                             const std::uint32_t* osr_start ,
                             Graph* graph ) {
  return BuildOSRStart(entry,osr_start,graph) == STOP_SUCCESS;
}

} // namespace

bool BuildPrototype( const Handle<Script>& script ,
                     const Handle<Prototype>& prototype ,
                     const RuntimeTrace& rt ,
                     Graph* output ) {

  GraphBuilder gb(script,rt);
  return gb.Build(prototype,output);
}

bool BuildPrototypeOSR( const Handle<Script>& script ,
                        const Handle<Prototype>& prototype ,
                        const RuntimeTrace& rt ,
                        const std::uint32_t* address ,
                        Graph* graph ) {

  GraphBuilder gb(script,rt);
  return gb.BuildOSR(prototype,address,graph);
}

} // namespace hir
} // namespace cbase
} // namespace lavascript
