#ifndef CBASE_HIR_INL_H_
#define CBASE_HIR_INL_H_

namespace lavascript {
namespace cbase      {
namespace hir        {

namespace detail {

template< typename T >
std::uint64_t Float64BinaryGVNImpl<T>::GVNHashImpl() const {
  auto self = static_cast<const T*>(this);
  return GVNHash3(self->type_name(),self->op(),
                                    self->lhs()->GVNHash(),
                                    self->rhs()->GVNHash());
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

CBASE_IR_LIST(__)

#undef __ // __

template< typename T >
inline T* Node::As() { lava_debug(NORMAL,lava_verify(Is<T>());); return static_cast<T*>(this); }

template< typename T >
inline const T* Node::As() const {
  lava_debug(NORMAL,lava_verify(Is<T>()););
  return static_cast<const T*>(this);
}

inline const zone::String& Node::AsZoneString() const {
  lava_debug(NORMAL,lava_verify(IsString()););
  return IsLString() ? *AsLString()->value() :
                       *AsSString()->value() ;
}

inline void Expr::AddOperand( Expr* node ) {
  auto itr = operand_list_.PushBack(zone(),node);
  node->AddRef(this,itr);
  if(node->HasSideEffect()) SetHasSideEffect();
}

inline void Expr::ReplaceOperand( std::size_t index , Expr* node ) {
  lava_debug(NORMAL,lava_verify(index < operand_list_.size()););
  auto itr(operand_list_.GetForwardIterator());
  lava_verify(itr.Advance(index));
  node->AddRef(this,itr);           // add reference for the new node
  itr.value()->RemoveRef(itr,this); // remove reference from the old value's refernece list
  itr.set_value(node);              // update to the new value
}

inline void Expr::AddEffect ( Expr* node ) {
  if( !node->IsNoMemoryEffectNode() ) {
    auto itr = effect_list_.PushBack(zone(),node);
    node->AddRef(this,itr);
    SetHasSideEffect();
  }
}

inline void Expr::AddEffectIfNotExist( Expr* node ) {
  auto itr = effect_list()->Find(node);
  if(itr.HasNext()) return;
  AddEffect(node);
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

inline zone::Zone* Node::zone() const {
  return graph_->zone();
}

inline bool Expr::IsLeaf() const {
#define __(A,B,C,D) case IRTYPE_##B: return D;
  switch(type()) {
    CBASE_IR_EXPRESSION(__)
    default: lava_die(); return false;
  }
#undef __ // __
}

inline bool Expr::IsMemoryRead() const {
  switch(type()) {
    case IRTYPE_IGET: case IRTYPE_PGET: case IRTYPE_OBJECT_GET: case IRTYPE_LIST_GET:
      return true;
    case IRTYPE_NO_READ_EFFECT: case IRTYPE_READ_EFFECT_PHI:
      return true;
    default:
      return false;
  }
}

inline bool Expr::IsMemoryWrite() const {
  switch(type()) {
    case IRTYPE_ISET: case IRTYPE_PSET: case IRTYPE_OBJECT_SET: case IRTYPE_LIST_SET:
      return true;
    case IRTYPE_NO_WRITE_EFFECT: case IRTYPE_WRITE_EFFECT_PHI:
      return true;
    default:
      return false;
  }
}

inline bool Expr::IsMemoryOp() const { return IsMemoryRead() || IsMemoryWrite(); }

inline bool Expr::IsMemoryNode() const {
  switch(type()) {
    case IRTYPE_ARG: case IRTYPE_GGET: case IRTYPE_UGET: case IRTYPE_LIST: case IRTYPE_OBJECT:
      return true;
    default:
      return false;
  }
}

inline MemoryWrite* Expr::AsMemoryWrite() {
  lava_debug(NORMAL,lava_verify(IsMemoryWrite()););
  return static_cast<MemoryWrite*>(this);
}

inline const MemoryWrite* Expr::AsMemoryWrite() const {
  lava_debug(NORMAL,lava_verify(IsMemoryWrite()););
  return static_cast<const MemoryWrite*>(this);
}

inline MemoryRead* Expr::AsMemoryRead() {
  lava_debug(NORMAL,lava_verify(IsMemoryRead()););
  return static_cast<MemoryRead*>(this);
}

inline const MemoryRead* Expr::AsMemoryRead() const {
  lava_debug(NORMAL,lava_verify(IsMemoryRead()););
  return static_cast<const MemoryRead*>(this);
}

inline MemoryOp* Expr::AsMemoryOp() {
  lava_debug(NORMAL,lava_verify(IsMemoryOp()););
  return static_cast<MemoryOp*>(this);
}

inline const MemoryOp* Expr::AsMemoryOp() const {
  lava_debug(NORMAL,lava_verify(IsMemoryOp()););
  return static_cast<const MemoryOp*>(this);
}

inline MemoryNode* Expr::AsMemoryNode() {
  lava_debug(NORMAL,lava_verify(IsMemoryNode()););
  return static_cast<MemoryNode*>(this);
}

inline const MemoryNode* Expr::AsMemoryNode() const {
  lava_debug(NORMAL,lava_verify(IsMemoryNode()););
  return static_cast<const MemoryNode*>(this);
}

inline bool Expr::IsNoMemoryEffectNode() const {
  return type() == IRTYPE_NO_READ_EFFECT || type() == IRTYPE_NO_WRITE_EFFECT;
}

inline bool Expr::IsPhiNode() const {
  switch(type()) {
    case IRTYPE_PHI: case IRTYPE_READ_EFFECT_PHI: case IRTYPE_WRITE_EFFECT_PHI:
      return true;
    default:
      return false;
  }
}

inline Arg* Arg::New( Graph* graph , std::uint32_t index ) {
  return graph->zone()->New<Arg>(graph,graph->AssignID(),index);
}

inline Float64* Float64::New( Graph* graph , double value , IRInfo* info ) {
  return graph->zone()->New<Float64>(graph,graph->AssignID(),value,info);
}

inline Boolean* Boolean::New( Graph* graph , bool value , IRInfo* info ) {
  return graph->zone()->New<Boolean>(graph,graph->AssignID(),value,info);
}

inline LString* LString::New( Graph* graph , const LongString& str , IRInfo* info ) {
  return graph->zone()->New<LString>(graph,graph->AssignID(),
      zone::String::New(graph->zone(),static_cast<const char*>(str.data()),str.size),info);
}

inline LString* LString::New( Graph* graph , const char* data , IRInfo* info ) {
  auto str = zone::String::New(graph->zone(),data);
  lava_debug(NORMAL,lava_verify(!str->IsSSO()););
  return graph->zone()->New<LString>(graph,graph->AssignID(),str,info);
}

inline LString* LString::New( Graph* graph , const zone::String* str , IRInfo* info ) {
  lava_debug(NORMAL,lava_verify(!str->IsSSO()););
  return graph->zone()->New<LString>(graph,graph->AssignID(),str,info);
}

inline SString* SString::New( Graph* graph , const SSO& str , IRInfo* info ) {
  return graph->zone()->New<SString>(graph,graph->AssignID(),
      zone::String::New(graph->zone(),static_cast<const char*>(str.data()),str.size()),info);
}

inline SString* SString::New( Graph* graph , const char* data , IRInfo* info ) {
  auto str = zone::String::New(graph->zone(),data);
  lava_debug(NORMAL,lava_verify(str->IsSSO()););
  return graph->zone()->New<SString>(graph,graph->AssignID(),str,info);
}

inline SString* SString::New( Graph* graph , const zone::String* str , IRInfo* info ) {
  lava_debug(NORMAL,lava_verify(str->IsSSO()););
  return graph->zone()->New<SString>(graph,graph->AssignID(),str,info);
}

inline Expr* NewString( Graph* graph , const zone::String* str , IRInfo* info ) {
  return str->IsSSO() ? static_cast<Expr*>(SString::New(graph,str,info)) :
                        static_cast<Expr*>(LString::New(graph,str,info)) ;
}

inline Expr* NewString( Graph* graph , const char* data , IRInfo* info ) {
  auto str = zone::String::New(graph->zone(),data);
  return NewString(graph,str,info);
}

inline Expr* NewString( Graph* graph , const void* data , std::size_t size , IRInfo* info ) {
  auto str = zone::String::New(graph->zone(),static_cast<const char*>(data),size);
  return NewString(graph,str,info);
}

inline Expr* NewStringFromBoolean( Graph* graph , bool value , IRInfo* info ) {
  std::string temp;
  LexicalCast(value,&temp);
  auto str = zone::String::New(graph->zone(),temp.c_str(),temp.size());
  return NewString(graph,str,info);
}

inline Expr* NewStringFromReal( Graph* graph , double value , IRInfo* info ) {
  std::string temp;
  LexicalCast(value,&temp);
  auto str = zone::String::New(graph->zone(),temp.c_str(),temp.size());
  return NewString(graph,str,info);
}

inline Nil* Nil::New( Graph* graph , IRInfo* info ) {
  return graph->zone()->New<Nil>(graph,graph->AssignID(),info);
}

inline IRList* IRList::New( Graph* graph , std::size_t size , IRInfo* info ) {
  return graph->zone()->New<IRList>(graph,graph->AssignID(),size,info);
}

inline IRObjectKV* IRObjectKV::New( Graph* graph , Expr* key , Expr* val , IRInfo* info ) {
  return graph->zone()->New<IRObjectKV>(graph,graph->AssignID(),key,val,info);
}

inline IRObject* IRObject::New( Graph* graph , std::size_t size , IRInfo* info ) {
  return graph->zone()->New<IRObject>(graph,graph->AssignID(),size,info);
}

inline Binary* Binary::New( Graph* graph , Expr* lhs , Expr* rhs, Operator op ,
                                                                  IRInfo* info ) {
  return graph->zone()->New<Binary>(graph,graph->AssignID(),lhs,rhs,op,info);
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
    case BC_AND: return AND; case BC_OR : return OR;
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

inline Unary* Unary::New( Graph* graph , Expr* opr , Operator op , IRInfo* info ) {
  return graph->zone()->New<Unary>(graph,graph->AssignID(),opr,op,info);
}

inline Ternary* Ternary::New( Graph* graph , Expr* cond , Expr* lhs , Expr* rhs ,
                                                                      IRInfo* info ) {
  return graph->zone()->New<Ternary>(graph,graph->AssignID(),cond,lhs,rhs,info);
}

inline UGet* UGet::New( Graph* graph , std::uint8_t index , std::uint32_t method , IRInfo* info ) {
  return graph->zone()->New<UGet>(graph,graph->AssignID(),index,method,info);
}

inline USet* USet::New( Graph* graph , std::uint8_t index , std::uint32_t method , Expr* opr , IRInfo* info ) {
  auto ret = graph->zone()->New<USet>(graph,graph->AssignID(),index,method,opr,info);
  return ret;
}

inline PGet* PGet::New( Graph* graph , Expr* obj , Expr* key , IRInfo* info , ControlFlow* region ) {
  auto ret = graph->zone()->New<PGet>(graph,graph->AssignID(),obj,key,info);
  return ret;
}

inline PSet* PSet::New( Graph* graph , Expr* obj , Expr* key , Expr* value , IRInfo* info, ControlFlow* region ) {
  auto ret = graph->zone()->New<PSet>(graph,graph->AssignID(),obj,key,value,info);
  return ret;
}

inline IGet* IGet::New( Graph* graph , Expr* obj, Expr* key , IRInfo* info , ControlFlow* region ) {
  auto ret = graph->zone()->New<IGet>(graph,graph->AssignID(),obj,key,info);
  return ret;
}

inline ISet* ISet::New( Graph* graph , Expr* obj , Expr* key , Expr* val , IRInfo* info , ControlFlow* region ) {
  auto ret = graph->zone()->New<ISet>(graph,graph->AssignID(),obj,key,val,info);
  return ret;
}

inline GGet* GGet::New( Graph* graph , Expr* key , IRInfo* info , ControlFlow* region ) {
  auto ret = graph->zone()->New<GGet>(graph,graph->AssignID(),key,info);
  return ret;
}

inline GSet* GSet::New( Graph* graph , Expr* key, Expr* value , IRInfo* info , ControlFlow* region ) {
  auto ret = graph->zone()->New<GSet>(graph,graph->AssignID(),key,value,info);
  return ret;
}

inline ItrNew* ItrNew::New( Graph* graph , Expr* operand , IRInfo* info , ControlFlow* region ) {
  auto ret = graph->zone()->New<ItrNew>(graph,graph->AssignID(),operand,info);
  return ret;
}

inline ItrNext* ItrNext::New( Graph* graph , Expr* operand , IRInfo* info , ControlFlow* region ) {
  auto ret = graph->zone()->New<ItrNext>(graph,graph->AssignID(),operand,info);
  return ret;
}

inline ItrTest* ItrTest::New( Graph* graph , Expr* operand , IRInfo* info , ControlFlow* region ) {
  auto ret = graph->zone()->New<ItrTest>(graph,graph->AssignID(),operand,info);
  return ret;
}

inline ItrDeref* ItrDeref::New( Graph* graph , Expr* operand , IRInfo* info , ControlFlow* region ) {
  auto ret = graph->zone()->New<ItrDeref>(graph,graph->AssignID(),operand,info);
  return ret;
}

inline void Phi::RemovePhiFromRegion( Phi* phi ) {
  if(phi->region()) {
    lava_verify(phi->region()->RemoveOperand(phi));
  }
}

inline Phi::Phi( Graph* graph , std::uint32_t id , ControlFlow* region , IRInfo* info ):
  Expr           (IRTYPE_PHI,id,graph,info),
  region_        (region)
{
  region->AddOperand(this);
}

inline Phi* Phi::New( Graph* graph , Expr* lhs , Expr* rhs , ControlFlow* region , IRInfo* info ) {
  auto ret = graph->zone()->New<Phi>(graph,graph->AssignID(),region,info);
  ret->AddOperand(lhs);
  ret->AddOperand(rhs);
  return ret;
}

inline Phi* Phi::New( Graph* graph , ControlFlow* region , IRInfo* info ) {
  return graph->zone()->New<Phi>(graph,graph->AssignID(),region,info);
}

inline ReadEffectPhi::ReadEffectPhi( Graph* graph , std::uint32_t id , ControlFlow* region , IRInfo* info ):
  MemoryRead(IRTYPE_READ_EFFECT_PHI,id,graph,info),
  region_   (region)
{
  region->AddOperand(this);
}

inline ReadEffectPhi* ReadEffectPhi::New( Graph* graph , MemoryRead* lhs , MemoryRead* rhs ,
                                                                           ControlFlow* region ,
                                                                           IRInfo* info ) {
  auto ret = graph->zone()->New<ReadEffectPhi>(graph,graph->AssignID(),region,info);
  ret->AddOperand(lhs);
  ret->AddOperand(rhs);
  return ret;
}

inline ReadEffectPhi* ReadEffectPhi::New( Graph* graph , ControlFlow* region , IRInfo* info ) {
  return graph->zone()->New<ReadEffectPhi>(graph,graph->AssignID(),region,info);
}

inline WriteEffectPhi::WriteEffectPhi( Graph* graph , std::uint32_t id , ControlFlow* region  , IRInfo* info ):
  MemoryWrite(IRTYPE_WRITE_EFFECT_PHI,id,graph,info),
  region_    (region)
{
  region->AddOperand(this);
}

inline WriteEffectPhi* WriteEffectPhi::New( Graph* graph , MemoryWrite* lhs , MemoryWrite* rhs ,
                                                                              ControlFlow* region  ,
                                                                              IRInfo* info ) {
  auto ret = graph->zone()->New<WriteEffectPhi>(graph,graph->AssignID(),region,info);
  ret->AddOperand(lhs);
  ret->AddOperand(rhs);
  return ret;
}

inline WriteEffectPhi* WriteEffectPhi::New( Graph* graph , ControlFlow* region , IRInfo* info ) {
  return graph->zone()->New<WriteEffectPhi>(graph,graph->AssignID(),region,info);
}

inline NoReadEffect* NoReadEffect::New( Graph* graph ) {
  return graph->zone()->New<NoReadEffect>(graph,graph->AssignID());
}

inline NoWriteEffect* NoWriteEffect::New( Graph* graph ) {
  return graph->zone()->New<NoWriteEffect>(graph,graph->AssignID());
}

inline ICall* ICall::New( Graph* graph , interpreter::IntrinsicCall ic , bool tc, IRInfo* info ) {
  return graph->zone()->New<ICall>(graph,graph->AssignID(),ic,tc,info);
}

inline LoadCls* LoadCls::New( Graph* graph , std::uint32_t ref , IRInfo* info ) {
  return graph->zone()->New<LoadCls>(graph,graph->AssignID(),ref,info);
}

inline Projection* Projection::New( Graph* graph , Expr* operand , std::uint32_t index , IRInfo* info ) {
  return graph->zone()->New<Projection>(graph,graph->AssignID(),operand,index,info);
}

inline OSRLoad* OSRLoad::New( Graph* graph , std::uint32_t index ) {
  return graph->zone()->New<OSRLoad>(graph,graph->AssignID(),index);
}

inline Checkpoint* Checkpoint::New( Graph* graph ) {
  return graph->zone()->New<Checkpoint>(graph,graph->AssignID());
}

inline void Checkpoint::AddStackSlot( Expr* val , std::uint32_t index ) {
  AddOperand(StackSlot::New(graph(),val,index));
}

inline TestType* TestType::New( Graph* graph , TypeKind tc , Expr* object , IRInfo* info ) {
  return graph->zone()->New<TestType>(graph,graph->AssignID(),tc,object,info);
}

inline TestListOOB* TestListOOB::New( Graph* graph , Expr* object , Expr* key , IRInfo* info ) {
  return graph->zone()->New<TestListOOB>(graph,graph->AssignID(),object,key,info);
}

inline Float64Negate* Float64Negate::New( Graph* graph , Expr* opr , IRInfo* info ) {
  return graph->zone()->New<Float64Negate>(graph,graph->AssignID(),opr,info);
}

inline Float64Arithmetic* Float64Arithmetic::New( Graph* graph , Expr* lhs , Expr* rhs , Operator op,
                                                                                         IRInfo* info ) {
  return graph->zone()->New<Float64Arithmetic>(graph,graph->AssignID(),lhs,rhs,op,info);
}

inline Float64Bitwise* Float64Bitwise::New( Graph* graph , Expr* lhs , Expr* rhs , Operator op, IRInfo* info ) {
  return graph->zone()->New<Float64Bitwise>(graph,graph->AssignID(),lhs,rhs,op,info);
}

inline Float64Compare* Float64Compare::New( Graph* graph , Expr* lhs , Expr* rhs , Operator op, IRInfo* info ) {
  return graph->zone()->New<Float64Compare>(graph,graph->AssignID(),lhs,rhs,op,info);
}

inline BooleanNot* BooleanNot::New( Graph* graph , Expr* opr , IRInfo* info ) {
  return graph->zone()->New<BooleanNot>(graph,graph->AssignID(),opr,info);
}

inline BooleanLogic* BooleanLogic::New( Graph* graph , Expr* lhs , Expr* rhs , Operator op, IRInfo* info ) {
  return graph->zone()->New<BooleanLogic>(graph,graph->AssignID(),lhs,rhs,op,info);
}

inline StringCompare* StringCompare::New( Graph* graph , Expr* lhs , Expr* rhs , Operator op , IRInfo* info ) {
  return graph->zone()->New<StringCompare>(graph,graph->AssignID(),lhs,rhs,op,info);
}

inline SStringEq* SStringEq::New( Graph* graph , Expr* lhs , Expr* rhs , IRInfo* info ) {
  return graph->zone()->New<SStringEq>(graph,graph->AssignID(),lhs,rhs,info);
}

inline SStringNe* SStringNe::New( Graph* graph , Expr* lhs , Expr* rhs , IRInfo* info ) {
  return graph->zone()->New<SStringNe>(graph,graph->AssignID(),lhs,rhs,info);
}

inline ListGet* ListGet::New( Graph* graph , Expr* obj , Expr* index , IRInfo* info ) {
  return graph->zone()->New<ListGet>(graph,graph->AssignID(),obj,index,info);
}

inline ListSet* ListSet::New( Graph* graph , Expr* obj , Expr* index , Expr* value , IRInfo* info ) {
  return graph->zone()->New<ListSet>(graph,graph->AssignID(),obj,index,value,info);
}

inline ObjectGet* ObjectGet::New( Graph* graph , Expr* obj , Expr* key , IRInfo* info ) {
  return graph->zone()->New<ObjectGet>(graph,graph->AssignID(),obj,key,info);
}

inline ObjectSet* ObjectSet::New( Graph* graph , Expr* obj , Expr* key , Expr* value , IRInfo* info ) {
  return graph->zone()->New<ObjectSet>(graph,graph->AssignID(),obj,key,value,info);
}

inline Box* Box::New( Graph* graph , Expr* obj , TypeKind tk , IRInfo* info ) {
  return graph->zone()->New<Box>(graph,graph->AssignID(),obj,tk,info);
}

inline Unbox* Unbox::New( Graph* graph , Expr* obj , TypeKind tk , IRInfo* info ) {
  return graph->zone()->New<Unbox>(graph,graph->AssignID(),obj,tk,info);
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

inline LoopHeader* LoopHeader::New( Graph* graph , ControlFlow* parent ) {
  return graph->zone()->New<LoopHeader>(graph,graph->AssignID(),parent);
}

inline Loop* Loop::New( Graph* graph ) { return graph->zone()->New<Loop>(graph,graph->AssignID()); }

inline LoopExit* LoopExit::New( Graph* graph , Expr* condition ) {
  return graph->zone()->New<LoopExit>(graph,graph->AssignID(),condition);
}

inline Guard* Guard::New( Graph* graph , Expr* test , Checkpoint* cp , ControlFlow* region ) {
  return graph->zone()->New<Guard>(graph,graph->AssignID(),test,cp,region);
}

inline If* If::New( Graph* graph , Expr* condition , ControlFlow* parent ) {
  return graph->zone()->New<If>(graph,graph->AssignID(),condition,parent);
}

inline CastToBoolean* CastToBoolean::New( Graph* graph , Expr* value , IRInfo* info ) {
  return graph->zone()->New<CastToBoolean>(graph,graph->AssignID(),value,info);
}

inline Expr* CastToBoolean::NewNegateCast( Graph* graph , Expr* value , IRInfo* info ) {
  auto cast = New(graph,value,info);
  auto unbox= Unbox::New(graph,cast,TPKIND_BOOLEAN,info);
  return BooleanNot::New(graph,unbox,info);
}

inline TypeAnnotation::TypeAnnotation( Graph* graph , std::uint32_t id , Guard* node , IRInfo* info ):
  Expr      (IRTYPE_TYPE_ANNOTATION,id,graph,info),
  type_kind_(node->test()->AsTestType()->type_kind())
{}

inline TypeAnnotation* TypeAnnotation::New( Graph* graph , Guard* node , IRInfo* info ) {
  return graph->zone()->New<TypeAnnotation>(graph,graph->AssignID(),node,info);
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

inline Start* Start::New( Graph* graph ) {
  return graph->zone()->New<Start>(graph,graph->AssignID());
}

inline End* End::New( Graph* graph , Success* s , Fail* f ) {
  return graph->zone()->New<End>(graph,graph->AssignID(),s,f);
}

inline Trap* Trap::New( Graph* graph , Checkpoint* cp , ControlFlow* region ) {
  return graph->zone()->New<Trap>(graph,graph->AssignID(),cp,region);
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

#endif // CBASE_HIR_INL_H_
