#ifndef CBASE_HIR_HIR_INL_H_
#define CBASE_HIR_HIR_INL_H_

namespace lavascript {
namespace cbase      {
namespace hir        {

namespace detail {

template< typename T >
std::uint64_t Float64BinaryGVNImpl<T>::GVNHashImpl() const {
  auto self = static_cast<const T*>(this);
  return GVNHash3(self->type_name(),self->op(),self->lhs()->GVNHash(),self->rhs()->GVNHash());
}

template< typename T >
bool Float64BinaryGVNImpl<T>::EqualImpl( const Expr* that ) const {
  auto self = static_cast<const T*>(this);
  if(that->Is<T>()) {
    auto n = that->As<T>();
    return self->op() == n->op() && self->lhs()->Equal(n->lhs()) &&
                                    self->rhs()->Equal(n->rhs());
  }
  return false;
}

} // namespace detail

#define __(A,B,...)                           \
  inline A* Node::As##A() {                   \
    lava_debug(NORMAL,lava_verify(Is##A());); \
    return static_cast<A*>(this);             \
  }                                           \
  inline const A* Node::As##A() const {       \
    lava_debug(NORMAL,lava_verify(Is##A());); \
    return static_cast<const A*>(this);       \
  }

CBASE_HIR_LIST(__)

#undef __ // __

inline zone::Zone* Node::zone() const {
  return graph_->zone();
}

template< typename T >
inline T* Node::As() {
  lava_debug(NORMAL,lava_verify(Is<T>()););
  return static_cast<T*>(this);
}

template< typename T >
inline const T* Node::As() const {
  lava_debug(NORMAL,lava_verify(Is<T>()););
  return static_cast<const T*>(this);
}

template< typename T >
inline bool Node::Is() const { return MapIRClassToIRType<T>::Test(type()); }

inline bool Node::IsLeaf() const {
#define __(A,B,C,D,...) case HIR_##B: return D;
#define Leaf   true
#define NoLeaf false
  switch(type()) {
    CBASE_HIR_EXPRESSION(__)
    default: lava_die(); return false;
  }
#undef Leaf   // Leaf
#undef NoLeaf // NoLeaf
#undef __     // __
}

inline bool Node::IsExpr       () const { return Is<Expr>      ();  }
inline bool Node::IsControlFlow() const { return Is<ControlFlow>(); }
inline bool Node::IsReadEffect () const { return Is<ReadEffect>();  }
inline bool Node::IsWriteEffect() const { return Is<WriteEffect>(); }
inline bool Node::IsMemoryRead () const { return Is<MemoryRead>();  }
inline bool Node::IsMemoryWrite() const { return Is<MemoryWrite>(); }
inline bool Node::IsMemoryNode () const { return Is<MemoryNode>();  }
inline bool Node::IsTestNode   () const { return Is<Test>      ();  }

inline const zone::String& Node::AsZoneString() const {
  lava_debug(NORMAL,lava_verify(IsString()););
  return IsLString() ? *AsLString()->value() : *AsSString()->value() ;
}

inline ControlFlow* Node::AsControlFlow() {
  lava_debug(NORMAL,lava_verify(IsControlFlow()););
  return static_cast<ControlFlow*>(this);
}

inline const ControlFlow* Node::AsControlFlow() const {
  lava_debug(NORMAL,lava_verify(IsControlFlow()););
  return static_cast<const ControlFlow*>(this);
}

inline Expr* Node::AsExpr() {
  lava_debug(NORMAL,lava_verify(IsExpr()););
  return static_cast<Expr*>(this);
}

inline const Expr* Node::AsExpr() const {
  lava_debug(NORMAL,lava_verify(IsExpr()););
  return static_cast<const Expr*>(this);
}

inline WriteEffect* Node::AsWriteEffect() {
  lava_debug(NORMAL,lava_verify(IsWriteEffect()););
  return static_cast<WriteEffect*>(this);
}

inline const WriteEffect* Node::AsWriteEffect() const {
  lava_debug(NORMAL,lava_verify(IsWriteEffect()););
  return static_cast<const WriteEffect*>(this);
}

inline ReadEffect* Node::AsReadEffect() {
  lava_debug(NORMAL,lava_verify(IsReadEffect()););
  return static_cast<ReadEffect*>(this);
}

inline const ReadEffect* Node::AsReadEffect() const {
  lava_debug(NORMAL,lava_verify(IsReadEffect()););
  return static_cast<const ReadEffect*>(this);
}

inline MemoryWrite* Node::AsMemoryWrite() {
  lava_debug(NORMAL,lava_verify(IsMemoryWrite()););
  return static_cast<MemoryWrite*>(this);
}

inline const MemoryWrite* Node::AsMemoryWrite() const {
  lava_debug(NORMAL,lava_verify(IsMemoryWrite()););
  return static_cast<const MemoryWrite*>(this);
}

inline MemoryRead* Node::AsMemoryRead() {
  lava_debug(NORMAL,lava_verify(IsMemoryRead()););
  return static_cast<MemoryRead*>(this);
}

inline const MemoryRead* Node::AsMemoryRead() const {
  lava_debug(NORMAL,lava_verify(IsMemoryRead()););
  return static_cast<const MemoryRead*>(this);
}

inline MemoryNode* Node::AsMemoryNode() {
  lava_debug(NORMAL,lava_verify(IsMemoryNode()););
  return static_cast<MemoryNode*>(this);
}

inline const MemoryNode* Node::AsMemoryNode() const {
  lava_debug(NORMAL,lava_verify(IsMemoryNode()););
  return static_cast<const MemoryNode*>(this);
}

inline Test* Node::AsTest() {
  lava_debug(NORMAL,lava_verify(IsTestNode()););
  return static_cast<Test*>(this);
}

inline const Test* Node::AsTest() const {
  lava_debug(NORMAL,lava_verify(IsTestNode()););
  return static_cast<const Test*>(this);
}

inline bool Expr::IsReplaceable( const Expr* that ) const {
  return IsIdentical(that) || (!HasDependency() && Equal(that));
}

inline void Expr::AddOperand( Expr* node ) {
  auto itr = operand_list_.PushBack(zone(),node);
  node->AddRef(this,itr);
}

inline void Expr::ReplaceOperand( std::size_t index , Expr* node ) {
  lava_debug(NORMAL,lava_verify(index < operand_list_.size()););
  auto itr(operand_list_.GetForwardIterator());
  lava_verify(itr.Advance(index));
  node->AddRef(this,itr);           // add reference for the new node
  itr.value()->RemoveRef(itr,this); // remove reference from the old value's refernece list
  itr.set_value(node);              // update to the new value
}

inline Arg* Arg::New( Graph* graph , std::uint32_t index ) {
  return graph->zone()->New<Arg>(graph,graph->AssignID(),index);
}

inline Float64* Float64::New( Graph* graph , double value ) {
  return graph->zone()->New<Float64>(graph,graph->AssignID(),value);
}

inline Boolean* Boolean::New( Graph* graph , bool value ) {
  return graph->zone()->New<Boolean>(graph,graph->AssignID(),value);
}

inline LString* LString::New( Graph* graph , const LongString& str ) {
  return graph->zone()->New<LString>(graph,graph->AssignID(),
      zone::String::New(graph->zone(),static_cast<const char*>(str.data()),str.size));
}

inline LString* LString::New( Graph* graph , const char* data ) {
  auto str = zone::String::New(graph->zone(),data);
  lava_debug(NORMAL,lava_verify(!str->IsSSO()););
  return graph->zone()->New<LString>(graph,graph->AssignID(),str);
}

inline LString* LString::New( Graph* graph , const zone::String* str ) {
  lava_debug(NORMAL,lava_verify(!str->IsSSO()););
  return graph->zone()->New<LString>(graph,graph->AssignID(),str);
}

inline SString* SString::New( Graph* graph , const SSO& str ) {
  return graph->zone()->New<SString>(graph,graph->AssignID(),
      zone::String::New(graph->zone(),static_cast<const char*>(str.data()),str.size()));
}

inline SString* SString::New( Graph* graph , const char* data ) {
  auto str = zone::String::New(graph->zone(),data);
  lava_debug(NORMAL,lava_verify(str->IsSSO()););
  return graph->zone()->New<SString>(graph,graph->AssignID(),str);
}

inline SString* SString::New( Graph* graph , const zone::String* str ) {
  lava_debug(NORMAL,lava_verify(str->IsSSO()););
  return graph->zone()->New<SString>(graph,graph->AssignID(),str);
}

inline Expr* NewString( Graph* graph , const zone::String* str ) {
  return str->IsSSO() ? static_cast<Expr*>(SString::New(graph,str)) :
                        static_cast<Expr*>(LString::New(graph,str)) ;
}

inline Expr* NewString( Graph* graph , const char* data ) {
  auto str = zone::String::New(graph->zone(),data);
  return NewString(graph,str);
}

inline Expr* NewString( Graph* graph , const void* data , std::size_t size ) {
  auto str = zone::String::New(graph->zone(),static_cast<const char*>(data),size);
  return NewString(graph,str);
}

inline Expr* NewStringFromBoolean( Graph* graph , bool value ) {
  std::string temp;
  LexicalCast(value,&temp);
  auto str = zone::String::New(graph->zone(),temp.c_str(),temp.size());
  return NewString(graph,str);
}

inline Expr* NewStringFromReal( Graph* graph , double value ) {
  std::string temp;
  LexicalCast(value,&temp);
  auto str = zone::String::New(graph->zone(),temp.c_str(),temp.size());
  return NewString(graph,str);
}

inline Nil* Nil::New( Graph* graph ) {
  return graph->zone()->New<Nil>(graph,graph->AssignID());
}

inline IRList* IRList::New( Graph* graph , std::size_t size ) {
  return graph->zone()->New<IRList>(graph,graph->AssignID(),size);
}

inline IRObjectKV* IRObjectKV::New( Graph* graph , Expr* key , Expr* val ) {
  return graph->zone()->New<IRObjectKV>(graph,graph->AssignID(),key,val);
}

inline IRObject* IRObject::New( Graph* graph , std::size_t size ) {
  return graph->zone()->New<IRObject>(graph,graph->AssignID(),size);
}

inline Binary* Binary::New( Graph* graph , Expr* lhs , Expr* rhs, Operator op ) {
  return graph->zone()->New<Binary>(graph,graph->AssignID(),lhs,rhs,op);
}

inline bool Binary::IsComparisonOperator( Operator op ) {
  switch(op) {
    case LT : case LE : case GT : case GE : case EQ : case NE :
      return true;
    default:
      return false;
  }
}

inline bool Binary::IsArithmeticOperator( Operator op ) {
  switch(op) {
    case ADD : case SUB : case MUL : case DIV : case MOD : case POW :
      return true;
    default:
      return false;
  }
}

inline bool Binary::IsBitwiseOperator( Operator op ) {
  switch(op) {
    case BAND: case BOR: case BXOR: case BSHL: case BSHR: case BROL: case BROR:
      return true;
    default:
      return false;
  }
}

inline bool Binary::IsLogicOperator( Operator op ) {
  return op == AND || op == OR;
}

inline Binary::Operator Binary::BytecodeToOperator( interpreter::Bytecode op ) {
  using namespace interpreter;
  switch(op) {
    case BC_ADDRV: case BC_ADDVR: case BC_ADDVV: return ADD;
    case BC_SUBRV: case BC_SUBVR: case BC_SUBVV: return SUB;
    case BC_MULRV: case BC_MULVR: case BC_MULVV: return MUL;
    case BC_DIVRV: case BC_DIVVR: case BC_DIVVV: return DIV;
    case BC_MODRV: case BC_MODVR: case BC_MODVV: return MOD;
    case BC_POWRV: case BC_POWVR: case BC_POWVV: return POW;
    case BC_LTRV : case BC_LTVR : case BC_LTVV : return LT ;
    case BC_LERV : case BC_LEVR : case BC_LEVV : return LE ;
    case BC_GTRV : case BC_GTVR : case BC_GTVV : return GT ;
    case BC_GERV : case BC_GEVR : case BC_GEVV : return GE ;
    case BC_EQRV : case BC_EQVR : case BC_EQSV : case BC_EQVS: case BC_EQVV: return EQ;
    case BC_NERV : case BC_NEVR : case BC_NESV : case BC_NEVS: case BC_NEVV: return NE;
    case BC_AND  : return AND;
    case BC_OR   : return OR;
    default: lava_unreachF("unknown bytecode %s",interpreter::GetBytecodeName(op)); break;
  }
  return ADD;
}

inline const char* Binary::GetOperatorName( Operator op ) {
  switch(op) {
    case ADD :    return "add";
    case SUB :    return "sub";
    case MUL :    return "mul";
    case DIV :    return "div";
    case MOD :    return "mod";
    case POW :    return "pow";
    case LT  :    return "lt" ;
    case LE  :    return "le" ;
    case GT  :    return "gt" ;
    case GE  :    return "ge" ;
    case EQ  :    return "eq" ;
    case NE  :    return "ne" ;
    case AND :    return "and";
    case OR  :    return "or";
    case BAND:    return "band";
    case BOR :    return "bor";
    case BXOR:    return "bxor";
    case BSHL:    return "bshl";
    case BSHR:    return "bshr";
    case BROL:    return "brol";
    case BROR:    return "bror";
    default:
      lava_die(); return NULL;
  }
}

inline Unary::Operator Unary::BytecodeToOperator( interpreter::Bytecode bc ) {
  if(bc == interpreter::BC_NEGATE)
    return MINUS;
  else
    return NOT;
}

inline const char* Unary::GetOperatorName( Operator op ) {
  if(op == MINUS)
    return "minus";
  else
    return "not";
}

inline Unary* Unary::New( Graph* graph , Expr* opr , Operator op ) {
  return graph->zone()->New<Unary>(graph,graph->AssignID(),opr,op);
}

inline Ternary* Ternary::New( Graph* graph , Expr* cond , Expr* lhs , Expr* rhs ) {
  return graph->zone()->New<Ternary>(graph,graph->AssignID(),cond,lhs,rhs);
}

inline UGet* UGet::New( Graph* graph , std::uint8_t index , std::uint32_t method ) {
  return graph->zone()->New<UGet>(graph,graph->AssignID(),index,method);
}

inline USet* USet::New( Graph* graph , std::uint8_t index , std::uint32_t method , Expr* opr ) {
  auto ret = graph->zone()->New<USet>(graph,graph->AssignID(),index,method,opr);
  return ret;
}

inline PGet* PGet::New( Graph* graph , Expr* obj , Expr* key ) {
  auto ret = graph->zone()->New<PGet>(graph,graph->AssignID(),obj,key);
  return ret;
}

inline PSet* PSet::New( Graph* graph , Expr* obj , Expr* key , Expr* value ) {
  auto ret = graph->zone()->New<PSet>(graph,graph->AssignID(),obj,key,value);
  return ret;
}

inline IGet* IGet::New( Graph* graph , Expr* obj, Expr* key ) {
  auto ret = graph->zone()->New<IGet>(graph,graph->AssignID(),obj,key);
  return ret;
}

inline ISet* ISet::New( Graph* graph , Expr* obj , Expr* key , Expr* val ) {
  auto ret = graph->zone()->New<ISet>(graph,graph->AssignID(),obj,key,val);
  return ret;
}

inline GGet* GGet::New( Graph* graph , Expr* key ) {
  auto ret = graph->zone()->New<GGet>(graph,graph->AssignID(),key);
  return ret;
}

inline GSet* GSet::New( Graph* graph , Expr* key, Expr* value ) {
  auto ret = graph->zone()->New<GSet>(graph,graph->AssignID(),key,value);
  return ret;
}

inline ItrNew* ItrNew::New( Graph* graph , Expr* operand ) {
  auto ret = graph->zone()->New<ItrNew>(graph,graph->AssignID(),operand);
  return ret;
}

inline ItrNext* ItrNext::New( Graph* graph , Expr* operand ) {
  auto ret = graph->zone()->New<ItrNext>(graph,graph->AssignID(),operand);
  return ret;
}

inline ItrTest* ItrTest::New( Graph* graph , Expr* operand ) {
  auto ret = graph->zone()->New<ItrTest>(graph,graph->AssignID(),operand);
  return ret;
}

inline ItrDeref* ItrDeref::New( Graph* graph , Expr* operand ) {
  auto ret = graph->zone()->New<ItrDeref>(graph,graph->AssignID(),operand);
  return ret;
}

inline void Phi::RemovePhiFromRegion( Phi* phi ) {
  if(phi->region()) {
    lava_verify(phi->region()->RemoveOperand(phi));
  }
}

inline Phi::Phi( Graph* graph , std::uint32_t id , ControlFlow* region ):
  Expr           (HIR_PHI,id,graph),
  region_        (region)
{
  region->AddOperand(this);
}

inline Phi* Phi::New( Graph* graph , Expr* lhs , Expr* rhs , ControlFlow* region ) {
  auto ret = graph->zone()->New<Phi>(graph,graph->AssignID(),region);
  ret->AddOperand(lhs);
  ret->AddOperand(rhs);
  return ret;
}

inline Phi* Phi::New( Graph* graph , ControlFlow* region ) {
  return graph->zone()->New<Phi>(graph,graph->AssignID(),region);
}

inline void ReadEffect::SetWriteEffect( WriteEffect* effect ) {
  auto itr = effect->AddReadEffect(this);
  effect_edge_.node = effect;
  effect_edge_.id   = itr;
}

inline WriteEffectPhi::WriteEffectPhi( Graph* graph , std::uint32_t id , ControlFlow* region ):
  WriteEffect(HIR_WRITE_EFFECT_PHI,id,graph),
  region_    (region)
{
  region->AddOperand(this);
}

inline WriteEffectPhi* WriteEffectPhi::New( Graph* graph , WriteEffect* lhs , WriteEffect* rhs ,
                                                                              ControlFlow* region ) {
  auto ret = graph->zone()->New<WriteEffectPhi>(graph,graph->AssignID(),region);
  ret->AddOperand(lhs);
  ret->AddOperand(rhs);
  return ret;
}

inline WriteEffectPhi* WriteEffectPhi::New( Graph* graph , ControlFlow* region ) {
  return graph->zone()->New<WriteEffectPhi>(graph,graph->AssignID(),region);
}

inline NoReadEffect* NoReadEffect::New( Graph* graph ) {
  return graph->zone()->New<NoReadEffect>(graph,graph->AssignID());
}

inline NoWriteEffect* NoWriteEffect::New( Graph* graph ) {
  return graph->zone()->New<NoWriteEffect>(graph,graph->AssignID());
}

inline ICall* ICall::New( Graph* graph , interpreter::IntrinsicCall ic , bool tc ) {
  return graph->zone()->New<ICall>(graph,graph->AssignID(),ic,tc);
}

inline LoadCls* LoadCls::New( Graph* graph , std::uint32_t ref ) {
  return graph->zone()->New<LoadCls>(graph,graph->AssignID(),ref);
}

inline Projection* Projection::New( Graph* graph , Expr* operand , std::uint32_t index ) {
  return graph->zone()->New<Projection>(graph,graph->AssignID(),operand,index);
}

inline OSRLoad* OSRLoad::New( Graph* graph , std::uint32_t index ) {
  return graph->zone()->New<OSRLoad>(graph,graph->AssignID(),index);
}

inline Checkpoint* Checkpoint::New( Graph* graph , IRInfo* info ) {
  return graph->zone()->New<Checkpoint>(graph,graph->AssignID(),info);
}

inline void Checkpoint::AddStackSlot( Expr* val , std::uint32_t index ) {
  AddOperand(StackSlot::New(graph(),val,index));
}

inline Guard* Guard::New( Graph* graph , Test* test , Checkpoint* cp ) {
  return graph->zone()->New<Guard>(graph,graph->AssignID(),test,cp);
}

inline TestType* TestType::New( Graph* graph , TypeKind tc , Expr* object ) {
  return graph->zone()->New<TestType>(graph,graph->AssignID(),tc,object);
}

inline TestListOOB* TestListOOB::New( Graph* graph , Expr* object , Expr* key ) {
  return graph->zone()->New<TestListOOB>(graph,graph->AssignID(),object,key);
}

inline Float64Negate* Float64Negate::New( Graph* graph , Expr* opr ) {
  return graph->zone()->New<Float64Negate>(graph,graph->AssignID(),opr);
}

inline Float64Arithmetic* Float64Arithmetic::New( Graph* graph , Expr* lhs , Expr* rhs , Operator op ) {
  return graph->zone()->New<Float64Arithmetic>(graph,graph->AssignID(),lhs,rhs,op);
}

inline Float64Bitwise* Float64Bitwise::New( Graph* graph , Expr* lhs , Expr* rhs , Operator op ) {
  return graph->zone()->New<Float64Bitwise>(graph,graph->AssignID(),lhs,rhs,op);
}

inline Float64Compare* Float64Compare::New( Graph* graph , Expr* lhs , Expr* rhs , Operator op ) {
  return graph->zone()->New<Float64Compare>(graph,graph->AssignID(),lhs,rhs,op);
}

inline BooleanNot* BooleanNot::New( Graph* graph , Expr* opr ) {
  return graph->zone()->New<BooleanNot>(graph,graph->AssignID(),opr);
}

inline BooleanLogic* BooleanLogic::New( Graph* graph , Expr* lhs , Expr* rhs , Operator op ) {
  return graph->zone()->New<BooleanLogic>(graph,graph->AssignID(),lhs,rhs,op);
}

inline StringCompare* StringCompare::New( Graph* graph , Expr* lhs , Expr* rhs , Operator op ) {
  return graph->zone()->New<StringCompare>(graph,graph->AssignID(),lhs,rhs,op);
}

inline SStringEq* SStringEq::New( Graph* graph , Expr* lhs , Expr* rhs ) {
  return graph->zone()->New<SStringEq>(graph,graph->AssignID(),lhs,rhs);
}

inline SStringNe* SStringNe::New( Graph* graph , Expr* lhs , Expr* rhs ) {
  return graph->zone()->New<SStringNe>(graph,graph->AssignID(),lhs,rhs);
}

inline ListGet* ListGet::New( Graph* graph , Expr* obj , Expr* index ) {
  return graph->zone()->New<ListGet>(graph,graph->AssignID(),obj,index);
}

inline ListSet* ListSet::New( Graph* graph , Expr* obj , Expr* index , Expr* value ) {
  return graph->zone()->New<ListSet>(graph,graph->AssignID(),obj,index,value);
}

inline ObjectGet* ObjectGet::New( Graph* graph , Expr* obj , Expr* key ) {
  return graph->zone()->New<ObjectGet>(graph,graph->AssignID(),obj,key);
}

inline ObjectSet* ObjectSet::New( Graph* graph , Expr* obj , Expr* key , Expr* value ) {
  return graph->zone()->New<ObjectSet>(graph,graph->AssignID(),obj,key,value);
}

inline Box* Box::New( Graph* graph , Expr* obj , TypeKind tk ) {
  return graph->zone()->New<Box>(graph,graph->AssignID(),obj,tk);
}

inline Unbox* Unbox::New( Graph* graph , Expr* obj , TypeKind tk ) {
  return graph->zone()->New<Unbox>(graph,graph->AssignID(),obj,tk);
}

inline CastToBoolean* CastToBoolean::New( Graph* graph , Expr* value ) {
  return graph->zone()->New<CastToBoolean>(graph,graph->AssignID(),value);
}

inline Expr* CastToBoolean::NewNegateCast( Graph* graph , Expr* value ) {
  auto cast = New(graph,value);
  auto unbox= Unbox::New(graph,cast,TPKIND_BOOLEAN);
  return BooleanNot::New(graph,unbox);
}

inline StackSlot* StackSlot::New( Graph* graph , Expr* expr , std::uint32_t index ) {
  return graph->zone()->New<StackSlot>(graph,graph->AssignID(),expr,index);
}

inline Region* Region::New( Graph* graph ) {
  return graph->zone()->New<Region>(graph,graph->AssignID());
}

inline Region* Region::New( Graph* graph , ControlFlow* parent ) {
  auto ret = New(graph);
  ret->AddBackwardEdge(parent);
  return ret;
}

inline bool ControlFlow::RemoveOperand( Expr* node ) {
  auto itr = operand_list_.Find(node);
  if(itr.HasNext()) {
    // remove |this| from the node's reference list
    lava_verify(node->RemoveRef(itr,this));
    // remove the node from the operand list
    operand_list_.Remove(itr);
  }
  return false;
}

inline LoopHeader* LoopHeader::New( Graph* graph , ControlFlow* parent ) {
  return graph->zone()->New<LoopHeader>(graph,graph->AssignID(),parent);
}

inline Loop* Loop::New( Graph* graph ) { return graph->zone()->New<Loop>(graph,graph->AssignID()); }

inline LoopExit* LoopExit::New( Graph* graph , Expr* condition ) {
  return graph->zone()->New<LoopExit>(graph,graph->AssignID(),condition);
}

inline If* If::New( Graph* graph , Expr* condition , ControlFlow* parent ) {
  return graph->zone()->New<If>(graph,graph->AssignID(),condition,parent);
}
inline IfTrue* IfTrue::New( Graph* graph , ControlFlow* parent ) {
  lava_debug(NORMAL,lava_verify(
        parent->IsIf() && parent->forward_edge()->size() == 1););

  return graph->zone()->New<IfTrue>(graph,graph->AssignID(),parent);
}

inline IfTrue* IfTrue::New( Graph* graph ) {
  return IfTrue::New(graph,NULL);
}

inline IfFalse* IfFalse::New( Graph* graph , ControlFlow* parent ) {
  lava_debug(NORMAL,lava_verify(
        parent->IsIf() && parent->forward_edge()->size() == 0););

  return graph->zone()->New<IfFalse>(graph,graph->AssignID(),parent);
}

inline IfFalse* IfFalse::New( Graph* graph ) {
  return IfFalse::New(graph,NULL);
}

inline Jump* Jump::New( Graph* graph , const std::uint32_t* pc , ControlFlow* parent ) {
  return graph->zone()->New<Jump>(graph,graph->AssignID(),parent,pc);
}

inline Fail* Fail::New( Graph* graph ) {
  return graph->zone()->New<Fail>(graph,graph->AssignID());
}

inline Success* Success::New( Graph* graph ) {
  return graph->zone()->New<Success>(graph,graph->AssignID());
}

inline bool Jump::TrySetTarget( const std::uint32_t* bytecode_pc , ControlFlow* target ) {
  if(bytecode_pc_ == bytecode_pc) {
    target_ = target;
    return true;
  }
  // The target should not be set since this jump doesn't and shouldn't jump to the input region
  return false;
}

inline Return* Return::New( Graph* graph , Expr* value , ControlFlow* parent ) {
  return graph->zone()->New<Return>(graph,graph->AssignID(),value,parent);
}

inline JumpValue* JumpValue::New( Graph* graph , Expr* value , ControlFlow* parent ) {
  return graph->zone()->New<JumpValue>(graph,graph->AssignID(),value,parent);
}

inline Start* Start::New( Graph* graph ) {
  return graph->zone()->New<Start>(graph,graph->AssignID());
}

inline End* End::New( Graph* graph , Success* s , Fail* f ) {
  return graph->zone()->New<End>(graph,graph->AssignID(),s,f);
}

inline Trap* Trap::New( Graph* graph , Checkpoint* cp , ControlFlow* region ) {
  return graph->zone()->New<Trap>(graph,graph->AssignID(),cp,region);
}

inline CondTrap* CondTrap::New( Graph* graph , Test* test , Checkpoint* cp ,
                                                            ControlFlow* region ) {
  return graph->zone()->New<CondTrap>(graph,graph->AssignID(),test,cp,region);
}

inline OSRStart* OSRStart::New( Graph* graph ) {
  return graph->zone()->New<OSRStart>(graph,graph->AssignID());
}

inline OSREnd* OSREnd::New( Graph* graph , Success* s , Fail* f ) {
  return graph->zone()->New<OSREnd>(graph,graph->AssignID(),s,f);
}

// ----------------------------------------------------------------------------
// Hash adapter for using zone::Table object
// ----------------------------------------------------------------------------
struct HIRExprHasher {
  static std::uint32_t Hash( const Expr* expr ) {
    return static_cast<std::size_t>(expr->GVNHash());
  }
  static bool Equal( const Expr* left , const Expr* right ) {
    return left->Equal(right);
  }
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_HIR_INL_H_
