#ifndef CBASE_HIR_HIR_INL_H_
#define CBASE_HIR_HIR_INL_H_

namespace lavascript {
namespace cbase      {
namespace hir        {

template< typename T >
std::uint64_t GVNHash0( T* ptr ) {
  std::uint64_t type = reinterpret_cast<std::uint64_t>(ptr);
  return Hasher::Hash64(type);
}

template< typename T , typename V >
std::uint64_t GVNHash1( T* ptr , const V& value ) {
  std::uint64_t uval = static_cast<std::uint64_t>(value);
  std::uint64_t type = reinterpret_cast<std::uint64_t>(ptr);
  return Hasher::HashCombine64(uval,type);
}

template< typename T , typename V1 , typename V2 >
std::uint64_t GVNHash2( T* ptr , const V1& v1 , const V2& v2 ) {
  std::uint64_t uv2 = static_cast<std::uint64_t>(v2);
  return Hasher::HashCombine64(GVNHash1(ptr,v1),uv2);
}

template< typename T , typename V1, typename V2 , typename V3 >
std::uint64_t GVNHash3( T* ptr , const V1& v1 , const V2& v2 , const V3& v3 ) {
  std::uint64_t uv3 = static_cast<std::uint64_t>(v3);
  return Hasher::HashCombine64(GVNHash2(ptr,v1,v2),uv3);
}

template< typename T , typename V1, typename V2, typename V3 , typename V4 >
std::uint64_t GVNHash4( T* ptr , const V1& v1 , const V2& v2 , const V3& v3 , const V4& v4 ) {
  std::uint64_t uv4 = static_cast<std::uint64_t>(v4);
  return Hasher::HashCombine64(GVNHash3(ptr,v1,v2,v3),uv4);
}


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
inline bool Node::Is() const { return HIRTypePredicate<T>::Test(type()); }

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

inline const zone::String& Node::AsZoneString() const {
  lava_debug(NORMAL,lava_verify(IsString()););
  return IsLString() ? *AsLString()->value() : *AsSString()->value() ;
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

inline bool Binary::IsLogicalOperator( Operator op ) {
  return op == AND || op == OR;
}

inline Binary::Operator Binary::BytecodeToOperator( interpreter::Bytecode op ) {
  using namespace interpreter;
  switch(op) {
    case BC_ADDRV:
    case BC_ADDVR:
    case BC_ADDVV:
      return ADD;
    case BC_SUBRV:
    case BC_SUBVR:
    case BC_SUBVV:
      return SUB;
    case BC_MULRV:
    case BC_MULVR:
    case BC_MULVV:
      return MUL;
    case BC_DIVRV:
    case BC_DIVVR:
    case BC_DIVVV:
      return DIV;
    case BC_MODRV:
    case BC_MODVR:
    case BC_MODVV:
      return MOD;
    case BC_POWRV:
    case BC_POWVR:
    case BC_POWVV:
      return POW;
    case BC_LTRV :
    case BC_LTVR :
    case BC_LTVV :
      return LT ;
    case BC_LERV :
    case BC_LEVR :
    case BC_LEVV :
      return LE ;
    case BC_GTRV :
    case BC_GTVR :
    case BC_GTVV :
      return GT ;
    case BC_GERV :
    case BC_GEVR :
    case BC_GEVV :
      return GE ;
    case BC_EQRV :
    case BC_EQVR :
    case BC_EQSV :
    case BC_EQVS :
    case BC_EQVV :
      return EQ;
    case BC_NERV :
    case BC_NEVR :
    case BC_NESV :
    case BC_NEVS :
    case BC_NEVV :
      return NE;
    case BC_AND  :
      return AND;
    case BC_OR   :
      return OR;
    default:
      lava_unreachF("unknown bytecode %s",interpreter::GetBytecodeName(op));
      break;
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

inline Arithmetic* Arithmetic::New( Graph* graph , Expr* lhs , Expr* rhs, Operator op ) {
  return graph->zone()->New<Arithmetic>(graph,graph->AssignID(),lhs,rhs,op);
}

inline Compare* Compare::New( Graph* graph , Expr* lhs , Expr* rhs , Operator op ) {
  return graph->zone()->New<Compare>(graph,graph->AssignID(),lhs,rhs,op);
}

inline Logical* Logical::New( Graph* graph , Expr* lhs , Expr* rhs , Operator op ) {
  return graph->zone()->New<Logical>(graph,graph->AssignID(),lhs,rhs,op);
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
  return graph->zone()->New<USet>(graph,graph->AssignID(),index,method,opr);
}

inline PGet* PGet::New( Graph* graph , Expr* obj , Expr* key ) {
  return graph->zone()->New<PGet>(graph,graph->AssignID(),obj,key);
}

inline PSet* PSet::New( Graph* graph , Expr* obj , Expr* key , Expr* value ) {
  return graph->zone()->New<PSet>(graph,graph->AssignID(),obj,key,value);
}

inline IGet* IGet::New( Graph* graph , Expr* obj, Expr* key ) {
  return graph->zone()->New<IGet>(graph,graph->AssignID(),obj,key);
}

inline ISet* ISet::New( Graph* graph , Expr* obj , Expr* key , Expr* val ) {
  return graph->zone()->New<ISet>(graph,graph->AssignID(),obj,key,val);
}

inline ObjectFind* ObjectFind::New( Graph* graph , Expr* obj , Expr* key , Checkpoint* cp ) {
  return graph->zone()->New<ObjectFind>(graph,graph->AssignID(),obj,key,cp);
}

inline ObjectInsert* ObjectInsert::New( Graph* graph , Expr* obj , Expr* key ) {
  return graph->zone()->New<ObjectInsert>(graph,graph->AssignID(),obj,key);
}

inline ObjectUpdate* ObjectUpdate::New( Graph* graph , Expr* object , Expr* key ) {
  return graph->zone()->New<ObjectUpdate>(graph,graph->AssignID(),object,key);
}

inline ListIndex* ListIndex::New( Graph* graph , Expr* obj , Expr* index , Checkpoint* cp ) {
  return graph->zone()->New<ListIndex>(graph,graph->AssignID(),obj,index,cp);
}

inline ListInsert* ListInsert::New( Graph* graph , Expr* obj , Expr* index , Checkpoint* cp ) {
  return graph->zone()->New<ListInsert>(graph,graph->AssignID(),obj,index,cp);
}

// FieldRefNode --------------------------------------------------------
inline FieldRefNode::FieldRefNode( Expr* node ) : node_(node) {
  lava_debug(NORMAL,lava_verify( node->Is<ListInsert>()   ||
                                 node->Is<ListIndex> ()   ||
                                 node->Is<ObjectFind>()   ||
                                 node->Is<ObjectInsert>() ||
                                 node->Is<ObjectUpdate>()););
}

inline Expr* FieldRefNode::object() const {
  if(node_->Is<ListInsert>  ()) return node_->As<ListInsert>  ()->object();
  if(node_->Is<ListIndex>   ()) return node_->As<ListIndex>   ()->object();
  if(node_->Is<ObjectFind>  ()) return node_->As<ObjectFind>  ()->object();
  if(node_->Is<ObjectInsert>()) return node_->As<ObjectInsert>()->object();
  if(node_->Is<ObjectUpdate>()) return node_->As<ObjectUpdate>()->object();
  lava_die() ; return NULL;
}

inline Expr* FieldRefNode::comp() const {
  if(node_->Is<ListInsert>  ()) return node_->As<ListInsert>  ()->index();
  if(node_->Is<ListIndex>   ()) return node_->As<ListIndex>   ()->index();
  if(node_->Is<ObjectFind>  ()) return node_->As<ObjectFind>  ()->key  ();
  if(node_->Is<ObjectInsert>()) return node_->As<ObjectInsert>()->key  ();
  if(node_->Is<ObjectUpdate>()) return node_->As<ObjectUpdate>()->key  ();
  lava_die() ; return NULL;
}

inline bool FieldRefNode::IsListRef() const {
  return node_->Is<ListInsert>() || node_->Is<ListIndex>();
}

inline bool FieldRefNode::IsObjectRef() const {
  return node_->Is<ObjectFind>() || node_->Is<ObjectInsert>() || node_->Is<ObjectUpdate>();
}

inline bool FieldRefNode::IsRead() const {
  return node_->Is<ListIndex>() || node_->Is<ObjectFind>();
}

inline bool FieldRefNode::IsWrite() const {
  return !IsRead();
}

inline ObjectRefGet* ObjectRefGet::New( Graph* graph , Expr* oref ) {
  return graph->zone()->New<ObjectRefGet>(graph,graph->AssignID(),oref);
}

inline ObjectRefSet* ObjectRefSet::New( Graph* graph , Expr* oref , Expr* value ) {
  return graph->zone()->New<ObjectRefSet>(graph,graph->AssignID(),oref,value);
}

inline ListRefGet* ListRefGet::New( Graph* graph , Expr* lref ) {
  return graph->zone()->New<ListRefGet>(graph,graph->AssignID(),lref);
}

inline ListRefSet* ListRefSet::New( Graph* graph , Expr* lref , Expr* value ) {
  return graph->zone()->New<ListRefSet>(graph,graph->AssignID(),lref,value);
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

inline void Phi::set_region( ControlFlow* region ) {
	lava_debug(NORMAL,lava_verify(!region_););
	region_ = region;
	region->AddOperand(this);
}

inline void Phi::RemovePhiFromRegion( Phi* phi ) {
  if(phi->region()) {
    lava_verify(phi->region()->RemoveOperand(phi));
  }
}

inline Phi::Phi( Graph* graph , std::uint32_t id ):
  Expr           (HIR_PHI,id,graph),
  region_        ()
{}

inline Phi* Phi::New( Graph* graph , Expr* lhs , Expr* rhs , ControlFlow* region ) {
  auto ret = graph->zone()->New<Phi>(graph,graph->AssignID());
  ret->AddOperand(lhs);
  ret->AddOperand(rhs);
  ret->set_region(region);
  return ret;
}

inline Phi* Phi::New( Graph* graph , ControlFlow* region ) {
  auto ret = graph->zone()->New<Phi>(graph,graph->AssignID());
  ret->set_region(region);
  return ret;
}

inline Phi* Phi::New( Graph* graph ) {
  return graph->zone()->New<Phi>(graph,graph->AssignID());
}

inline Phi* Phi::New( Graph* graph , Expr* lhs , Expr* rhs ) {
  auto phi = Phi::New(graph);
  phi->AddOperand(lhs);
  phi->AddOperand(rhs);
  return phi;
}

inline void ReadEffect::SetWriteEffect( WriteEffect* node ) {
  auto itr = node->AddReadEffect(this);
  effect_edge_.node = node;
  effect_edge_.id   = itr;
}

inline EffectPhi* EffectPhi::New( Graph* graph ) {
  return graph->zone()->New<EffectPhi>(graph,graph->AssignID());
}

inline EffectPhi* EffectPhi::New( Graph* graph , ControlFlow* region ) {
  auto ret = New(graph);
  region->AddOperand(ret);
  ret->set_region(region);
  return ret;
}

inline EffectPhi* EffectPhi::New( Graph* graph , WriteEffect* lhs , WriteEffect* rhs ) {
  auto ret = New(graph);
  ret->AddOperand(lhs);
  ret->AddOperand(rhs);
  return ret;
}

inline EffectPhi* EffectPhi::New( Graph* graph , WriteEffect* lhs, WriteEffect* rhs ,
                                                                   ControlFlow* region ) {
  auto ret = New(graph,lhs,rhs);
  region->AddOperand(ret);
  ret->set_region(region);
  return ret;
}

inline LoopEffectPhi* LoopEffectPhi::New( Graph* graph , WriteEffect* lhs ) {
  auto ret = graph->zone()->New<LoopEffectPhi>(graph,graph->AssignID());
  ret->AddOperand(lhs);
  return ret;
}

inline InitBarrier* InitBarrier::New( Graph* graph ) {
  return graph->zone()->New<InitBarrier>(graph,graph->AssignID());
}

inline BranchStartEffect* BranchStartEffect::New( Graph* graph ) {
  return graph->zone()->New<BranchStartEffect>(graph,graph->AssignID());
}

inline BranchStartEffect* BranchStartEffect::New( Graph* graph , WriteEffect* effect ) {
  auto ret = New(graph);
  ret->HappenAfter(effect);
  return ret;
}

inline EmptyWriteEffect* EmptyWriteEffect::New( Graph* graph ) {
  return graph->zone()->New<EmptyWriteEffect>(graph,graph->AssignID());
}

inline EmptyWriteEffect* EmptyWriteEffect::New( Graph* graph , WriteEffect* before ) {
  auto e = New(graph);
  e->HappenAfter(before);
  return e;
}

inline ICall* ICall::New( Graph* graph , interpreter::IntrinsicCall ic , bool tc ) {
  return graph->zone()->New<ICall>(graph,graph->AssignID(),ic,tc);
}

inline Closure* Closure::New( Graph* graph , std::uint32_t ref ) {
  return graph->zone()->New<Closure>(graph,graph->AssignID(),ref);
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

inline Box* Box::New( Graph* graph , Expr* obj , TypeKind tk ) {
  return graph->zone()->New<Box>(graph,graph->AssignID(),obj,tk);
}

inline Unbox* Unbox::New( Graph* graph , Expr* obj , TypeKind tk ) {
  return graph->zone()->New<Unbox>(graph,graph->AssignID(),obj,tk);
}

inline ConvBoolean* ConvBoolean::New( Graph* graph , Expr* value ) {
  return graph->zone()->New<ConvBoolean>(graph,graph->AssignID(),value);
}

inline Box* ConvBoolean::NewBox( Graph* graph , Expr* value ) {
  auto unbox = New(graph,value);
  auto box   = Box::New(graph,unbox,TPKIND_BOOLEAN);
  return box;
}

inline ConvNBoolean* ConvNBoolean::New( Graph* graph , Expr* value ) {
  return graph->zone()->New<ConvNBoolean>(graph,graph->AssignID(),value);
}

inline Box* ConvNBoolean::NewBox( Graph* graph , Expr* value ) {
  auto unbox = New(graph,value);
  auto box   = Box::New(graph,unbox,TPKIND_BOOLEAN);
  return box;
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

inline Trap* Trap::New( Graph* graph , Checkpoint* cp , ControlFlow* region ) {
  return graph->zone()->New<Trap>(graph,graph->AssignID(),cp,region);
}

inline CondTrap* CondTrap::New( Graph* graph , Test* test , Checkpoint* cp ,
                                                            ControlFlow* region ) {
  return graph->zone()->New<CondTrap>(graph,graph->AssignID(),test,cp,region);
}

inline Start* Start::New( Graph* graph ) {
  return graph->zone()->New<Start>(graph,graph->AssignID());
}

inline End* End::New( Graph* graph , Success* s , Fail* f ) {
  return graph->zone()->New<End>(graph,graph->AssignID(),s,f);
}

inline OSRStart* OSRStart::New( Graph* graph ) {
  return graph->zone()->New<OSRStart>(graph,graph->AssignID());
}

inline OSREnd* OSREnd::New( Graph* graph , Success* s , Fail* f ) {
  return graph->zone()->New<OSREnd>(graph,graph->AssignID(),s,f);
}

inline InlineStart* InlineStart::New( Graph* graph , ControlFlow* region ) {
  return graph->zone()->New<InlineStart>(graph,graph->AssignID(),region);
}

inline InlineEnd*   InlineEnd::New( Graph* graph , ControlFlow* region ) {
  return graph->zone()->New<InlineEnd>(graph,graph->AssignID(),region);
}

inline InlineEnd*   InlineEnd::New( Graph* graph ) {
  return graph->zone()->New<InlineEnd>(graph,graph->AssignID());
}

template< typename T >
void Graph::GetControlFlowNode( zone::Zone* zone , T* output ) const {
  output->clear();
  lava_foreach( auto v , ControlFlowBFSIterator(zone,*this) ) {
    output->push_back(v);
  }
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

namespace detail {

template< typename GETTER >
template< typename T >
T* NodeDFSIterator<GETTER>::Next() {
  while(!stack_.empty()) {
recursion:
    auto top = stack_.Top();
    lava_foreach( auto &v , GETTER().Get(top) ) {
      if(stack_.Push(v)) goto recursion;
    }
    // when we reach here it means we scan through all its predecessor nodes and
    // don't see any one not visited , or maybe this node is a singleton/leaf.
    stack_.Pop();
    return static_cast<T*>(top);
  }
  return NULL;
}

template< typename GETTER >
template< typename T >
T* NodeRPOIterator<GETTER>::Next() {
  while(!stack_.empty()) {
recursion:
    auto top = stack_.Top();
    // 1. check whether all its predecessuor has been visited or not
    lava_foreach( auto cf , GETTER().Get(top) ) {
      if(!mark_[cf->id()] && stack_.Push(cf)) {
        goto recursion;
      }
    }
    // 2. visit the top node
    lava_debug(NORMAL,lava_verify(!mark_[top->id()]););
    mark_[top->id()] = true;
    stack_.Pop();
    return static_cast<T*>(top);
  }
  return NULL;
}

inline RegionListIterator ControlFlowForwardIteratorGetter::Get( Node* region ) const {
  auto cf = region->As<ControlFlow>();
  return cf->forward_edge()->GetForwardIterator();
}

inline RegionListIterator ControlFlowBackwardIteratorGetter::Get( Node* region ) const {
  auto cf = region->As<ControlFlow>();
  return cf->backward_edge()->GetForwardIterator();
}

inline OperandIterator ExprIteratorGetter::Get( Node* node ) const {
  auto expr = node->As<Expr>();
  return expr->operand_list()->GetForwardIterator();
}

} // namespace detail


} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_HIR_INL_H_
