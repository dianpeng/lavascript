#ifndef CBASE_GRAPH_BUILDER_H_
#define CBASE_GRAPH_BUILDER_H_
#include "hir.h"
#include "ool-vector.h"

#include "src/interpreter/intrinsic-call.h"
#include "src/objects.h"
#include "src/zone/zone.h"
#include "src/type-trace.h"

#include <cstdint>
#include <vector>
#include <map>
#include <string>

namespace lavascript {
namespace cbase {
namespace hir    {

typedef std::vector<Expr*> ValueStack;

// -------------------------------------------------------------------------------------
//
// This is a HIR/MIR graph consturction , the LIR is essentially a traditionaly CFG
// The graph is a sea of nodes style and it is responsible for all optimization before
// scheduling
//
// The builder can build 1) normal function call 2) OSR style function IR
class GraphBuilder {
 public:
  // all intenral class forward declaration
  struct FuncInfo;

  // Used to track a value inside of the ValueStack
  struct StackSlot {
    static const std::uint32_t kMax = std::numeric_limits<std::uint32_t>::max();
    Expr* node;
    std::uint32_t index;
    bool HasIndex() const { return index < kMax; }
    explicit StackSlot( Expr* n , std::uint32_t i = kMax ): node(n),index(i) {}
    StackSlot( StackSlot&& ss ) : node(ss.node), index(ss.index) {}
    StackSlot& operator = ( StackSlot&& that ) {
      node = that.node; index = that.index; return *this;
    }
  };

  // Environment --------------------------------------------------------------
  // this object records all the side effect that can be observed by the
  // function or its nested inline functions.
  // In general, we can observe side effect via three categories:
  // 1. input argument
  // 2. return value (if inlined)
  // 3. global variables
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
    typedef std::function<IRInfo* ()>  IRInfoProvider;

    Environment( GraphBuilder* );
    // root effect group
    EffectGroup* root()    { return &root_; }
    // init environment object from prototype object
    void EnterFunctionScope( const FuncInfo& );
    void PopulateArgument  ( const FuncInfo& );
    void ExitFunctionScope ( const FuncInfo& );
    // getter/setter
    Expr*  GetUpValue( std::uint8_t , const IRInfoProvider& );
    Expr*  GetGlobal ( const void* , std::size_t , const KeyProvider& , const IRInfoProvider& );
    void   SetUpValue( std::uint8_t , Expr* , const IRInfoProvider& );
    void   SetGlobal ( const void* , std::size_t , const KeyProvider& , Expr* , const IRInfoProvider& );
    // accessor
    ValueStack*      stack()    { return &stack_;  }
    UpValueVector*   upvalue()  { return &upvalue_;}
    GlobalMap*       global()   { return &global_; }
    // update a node appear in the environment with another node
    void UpdateNode  ( Expr* , Expr* );
   private:
    ValueStack stack_;          // register stack
    EffectGroup root_;          // root's unknown effect tracking region
    UpValueVector upvalue_;     // upvalue's effect group
    GlobalMap global_;          // global's effect group
    GraphBuilder* gb_;          // graph builder

    friend class GraphBuilder;
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
    BytecodeAnalyze           bc_analyze;
    const std::uint32_t*      osr_start;

   public:
    inline FuncInfo( const Handle<Prototype>& , ControlFlow* , std::uint32_t );
    inline FuncInfo( const Handle<Prototype>& , ControlFlow* , const std::uint32_t* );
    inline FuncInfo( FuncInfo&& );
    bool IsOSR() const { return osr_start != NULL; }
    bool IsLocalVar( std::uint8_t slot ) const { return slot < max_local_var_size; }
    // check whether we have loop currently
    bool HasLoop() const { return !loop_info.empty(); }
    // get the current loop
    GraphBuilder::LoopInfo& current_loop() { return loop_info.back(); }
    const GraphBuilder::LoopInfo& current_loop() const { return loop_info.back(); }
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
  // Build routine's return status code
  enum StopReason {
    STOP_BAILOUT = -1,   // the target is too complicated to be jitted
    STOP_JUMP,
    STOP_EOF ,
    STOP_END ,
    STOP_SUCCESS
  };

 public:
  inline GraphBuilder( const Handle<Script>& , const TypeTrace& );
  // Build a normal function's IR graph
  bool Build( const Handle<Prototype>& , Graph* );
  // Build a function's graph assume OSR
  bool BuildOSR( const Handle<Prototype>& , const std::uint32_t* , Graph* );

 public: // Stack accessing
  std::uint32_t StackIndex( std::uint32_t index ) const {
    return func_info().base+index;
  }
  void StackSet( std::uint32_t index , Expr* value ) {
    vstk()->at(StackIndex(index)) = value;
  }
  void StackReset( std::uint32_t index ) {
    vstk()->at(StackIndex(index)) = NULL;
  }
  Expr* StackGet( std::uint32_t index ) {
    return vstk()->at(StackIndex(index));
  }
  Expr* StackGet( std::uint32_t index , std::uint32_t base ) {
    return vstk()->at(base+index);
  }
  StackSlot StackGetSlot( std::uint32_t index ) {
    return StackSlot(StackGet(index),index);
  }

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
  // input argument size , the input argument size is the argument size that
  // is belong to the top most function since this function's input argument
  // remains as input argument, rest of the nested inlined function's input
  // argument is not argument but just local variables
  std::size_t input_argument_size()    const { return func_info_.front().prototype->argument_size(); }
 private: // Constant handling
  Expr* NewConstNumber( std::int32_t , const interpreter::BytecodeLocation& );
  Expr* NewConstNumber( std::int32_t );

  Expr* NewNumber     ( std::uint8_t , IRInfo* );
  Expr* NewNumber     ( std::uint8_t , const interpreter::BytecodeLocation& );
  Expr* NewNumber     ( std::uint8_t );

  Expr* NewString     ( std::uint8_t , IRInfo* );
  Expr* NewString     ( std::uint8_t , const interpreter::BytecodeLocation& );
  Expr* NewString     ( std::uint8_t );

  Expr* NewString     ( const Str& , const interpreter::BytecodeLocation& );
  Expr* NewString     ( const Str& , IRInfo* );
  Str   NewStr        ( std::uint32_t ref , bool sso );

  Expr* NewSSO        ( std::uint8_t , IRInfo* );
  Expr* NewSSO        ( std::uint8_t , const interpreter::BytecodeLocation& );
  Expr* NewSSO        ( std::uint8_t );

  Expr* NewBoolean    ( bool         , const interpreter::BytecodeLocation& );
  Expr* NewBoolean    ( bool );

 private: // IRList/IRObject
  IRList*   NewIRList   ( std::size_t , IRInfo* );
  IRObject* NewIRObject ( std::size_t , IRInfo* );

 private: // Guard handling
  // Add a type feedback with TypeKind into the stack slot pointed by index
  Expr* AddTypeFeedbackIfNeed( const StackSlot& , TypeKind     , const interpreter::BytecodeLocation& );
  Expr* AddTypeFeedbackIfNeed( const StackSlot& , const Value& , const interpreter::BytecodeLocation& );
  Expr* AddTypeFeedbackIfNeed( const StackSlot& , TypeKind     , IRInfo* );
  Expr* AddTypeFeedbackIfNeed( const StackSlot& , const Value& , IRInfo* );

 private: // Arithmetic
  // Unary
  Expr* NewUnary            ( const StackSlot& , Unary::Operator , const interpreter::BytecodeLocation& );
  Expr* TrySpeculativeUnary ( const StackSlot& , Unary::Operator , const interpreter::BytecodeLocation& );
  Expr* NewUnaryFallback    ( const StackSlot& , Unary::Operator , const interpreter::BytecodeLocation& );

  // Binary
  Expr* NewBinary           ( const StackSlot& , const StackSlot& , Binary::Operator ,
                                                                     const interpreter::BytecodeLocation& );
  Expr* TrySpecialTestBinary( const StackSlot& , const StackSlot& , Binary::Operator ,
                                                                     const interpreter::BytecodeLocation& );
  Expr* TrySpeculativeBinary( const StackSlot& , const StackSlot& , Binary::Operator ,
                                                                     const interpreter::BytecodeLocation& );
  Expr* NewBinaryFallback   ( const StackSlot& , const StackSlot& , Binary::Operator ,
                                                                     const interpreter::BytecodeLocation& );

  // Ternary
  Expr* NewTernary( const StackSlot& , Expr* , Expr* , const interpreter::BytecodeLocation& );

  // Intrinsic
  Expr* NewICall  ( std::uint8_t ,std::uint8_t ,std::uint8_t ,bool ,const interpreter::BytecodeLocation& );
  Expr* LowerICall( ICall* );

 private: // Property/Index Get/Set
  Expr* NewPSet( Expr* , Expr* , Expr* , IRInfo* );
  Expr* NewPGet( Expr* , Expr* , IRInfo* );
  Expr* NewISet( Expr* , Expr* , Expr* , IRInfo* );
  Expr* NewIGet( Expr* , Expr* , IRInfo* );

 private: // Global variables
  void NewGGet( std::uint8_t , std::uint8_t , const interpreter::BytecodeLocation& , bool sso = false );
  void NewGSet( std::uint8_t , std::uint8_t , const interpreter::BytecodeLocation& , bool sso = false );

 private: // Upvalue
  void NewUGet( std::uint8_t , std::uint8_t , const interpreter::BytecodeLocation& );
  void NewUSet( std::uint8_t , std::uint8_t , const interpreter::BytecodeLocation& );

 private: // Checkpoint generation
  Checkpoint* BuildCheckpoint( const interpreter::BytecodeLocation& );

 private: // OSR
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
  void InsertPhi( Environment* , Environment* , ControlFlow* , IRInfo* );
  void GeneratePhi( ValueStack* , const ValueStack& , const ValueStack& , std::size_t , ControlFlow* , IRInfo* );

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
  void GenerateLoopPhi       ( const interpreter::BytecodeLocation& );
  void PatchLoopPhi          ();

  // Iterate until we see FEEND/FEND1/FEND2/FEVREND
  StopReason BuildLoopBlock  ( interpreter::BytecodeIterator* itr );
  void PatchUnconditionalJump( UnconditionalJumpList* , ControlFlow* , const interpreter::BytecodeLocation& );

 private: // IRInfo
  IRInfo* NewIRInfo( const interpreter::BytecodeLocation& loc );
  IRInfo* NewIRInfo( interpreter::BytecodeIterator* );

 private: // Effect analyzing
  typedef std::function< void (Expr*,EffectGroup*) > EffectVisitor;

  // Analyze the input expression node and find *all* its satisfied effect group
  // in our effect group list and call the EffectVisitor callback function on each
  // group found
  void VisitEffect     ( Expr* node , const EffectVisitor& visitor );
  void VisitEffectRead ( Expr* node , MemoryRead* );
  void VisitEffectWrite( Expr* node , MemoryWrite*);
  // New a new effect group object inside of our internal zone object
  EffectGroup* NewEffectGroup( MemoryWrite* op = NULL );
 private:
  // Zone owned by the Graph object, and it is supposed to be stay around while the
  // optimization happenened
  zone::Zone*             zone_;
  Handle<Script>          script_;
  Graph*                  graph_;
  // Working set data , used when doing inline and other stuff
  Environment*            env_;
  std::vector<FuncInfo>   func_info_;
  // Type trace for speculative operation generation
  const TypeTrace&        type_trace_;
  // All tracked effect group except the root effect group
  OOLVector<EffectGroup*> effect_group_;
  // This zone is used for other transient memory costs during graph construction
  zone::SmallZone         temp_zone_;

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
  bc_analyze        (std::move(that.bc_analyze)),
  osr_start         (that.osr_start)
{}

inline GraphBuilder::GraphBuilder( const Handle<Script>& script , const TypeTrace& tt ):
  zone_             (NULL),
  script_           (script),
  graph_            (NULL),
  func_info_        (),
  type_trace_       (tt),
  effect_group_     (),
  temp_zone_        ()
{}

inline void GraphBuilder::FuncInfo::EnterLoop( const std::uint32_t* pc ) {
  const BytecodeAnalyze::LoopHeaderInfo* info = bc_analyze.LookUpLoopHeader(pc);
  lava_debug(NORMAL,lava_verify(info););
  loop_info.push_back(LoopInfo(info));
}

inline void GraphBuilder::EffectGroup::AddReadEffect( MemoryRead* read ) {
  // this is a read after write effect or *true* effect
  read->AddEffect(write_effect_);
  read_list_.push_back(read);
}

inline void GraphBuilder::EffectGroup::UpdateWriteEffect( MemoryWrite* write ) {
  // this is a write after read effect or *anti* effect
  for( auto &e : read_list_ ) write->AddEffect(e);
  // new write barrier will be setup, just clear the read list
  read_list_.clear();
  // update the new write effect
  write_effect_ = write;
}

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_GRAPH_BUILDER_H_
