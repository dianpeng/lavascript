#include "predicate.h"
#include "src/double.h"
#include "src/trace.h"
#include <algorithm>

namespace lavascript {
namespace cbase      {
namespace hir        {
namespace            {

// decide and classify the predicate that can be used to represent an input expression
// if we cannot find a proper predicate object to represent the expression, then we just
// bailout with unknown predicate.
class Classifier {
 public:
  Classifier() : var_(NULL) , type_(UNKNOWN_PREDICATE) {}
  PredicateClassifyResult DoClassify( Expr* );
 private:
  bool Check    ( Expr* );
  bool DoCheck  ( Expr* , Expr* );
  bool CheckType( PredicateType );
  bool CheckVar ( Expr* );
  bool Bailout  () { var_ = NULL; type_ = UNKNOWN_PREDICATE; return false; }
 private:
  Expr*         var_;
  PredicateType type_;
};

bool Classifier::DoCheck( Expr* l , Expr* r ) { return Check(l) && Check(r); }

bool Classifier::CheckType( PredicateType t ) {
  if(type_ == UNKNOWN_PREDICATE) {
    type_ = t;
    return true;
  }
  return type_ == t;
}
bool Classifier::CheckVar( Expr* node ) {
  if(var_)
    return node == var_;
  else {
    var_ = node;
    return true;
  }
}
bool Classifier::Check( Expr* node ) {
  // if this node has side effect, just bailout , no need to evaluate it since
  // we cannot remove node that has side effect.
  if(node->HasSideEffect()) return Bailout();

  switch(node->type()) {
    case HIR_BOOLEAN_LOGIC:
      {
        auto l = node->AsBooleanLogic();
        return DoCheck(l->lhs(),l->rhs());
      }
    case HIR_FLOAT64_COMPARE:
      {
        if(!CheckType(FLOAT64_PREDICATE)) return false;
        auto fcomp = node->AsFloat64Compare();
        auto lhs   = fcomp->lhs();
        auto rhs   = fcomp->rhs();
        // check if lhs and rhs are format that can be handled
        if(lhs->IsFloat64()) {
          lava_debug(NORMAL,lava_verify(!rhs->IsFloat64()););
          return CheckVar(rhs);
        } else if(rhs->IsFloat64()) {
          return CheckVar(lhs);
        }
      }
      break;
    case HIR_TEST_TYPE:
      {
        if(!CheckType(TYPE_PREDICATE)) return false;
        auto tt = node->AsTestType();
        auto n  = tt->object();
        return CheckVar(n);
      }
      break;
    default:
      {
        if(!CheckType(BOOLEAN_PREDICATE)) return false;
        auto n = node->IsBooleanNot() ? node->AsBooleanNot()->operand() : node ;
        return CheckVar(n);
      }
      break;
  }
  return Bailout();
}

PredicateClassifyResult Classifier::DoClassify( Expr* node ) {
  if(!Check(node)) return PredicateClassifyResult();
  return PredicateClassifyResult(type_,var_);
}

} // namespace

PredicateClassifyResult ClassifyPredicate( Expr* node ) {
  return Classifier().DoClassify(node);
}

/* --------------------------------------------------------------------------
 *
 * Float64Predicate Implementation
 *
 * -------------------------------------------------------------------------*/
Float64Predicate::NumberPoint Float64Predicate::NumberPoint::kPosInf(Double::PosInf(),false);
Float64Predicate::NumberPoint Float64Predicate::NumberPoint::kNegInf(Double::NegInf(),false);

namespace {

// [C < (3
inline Float64Predicate::NumberPoint LowerMin( const Float64Predicate::NumberPoint& lhs ,
                                               const Float64Predicate::NumberPoint& rhs ) {
  if(lhs.value < rhs.value)
    return lhs;
  else if(lhs.value == rhs.value) {
    if(lhs.close && !rhs.close)
      return lhs;
    else if(!lhs.close && rhs.close)
      return rhs;
  }
  return rhs;
}

inline Float64Predicate::NumberPoint LowerMax( const Float64Predicate::NumberPoint& lhs ,
                                               const Float64Predicate::NumberPoint& rhs ) {
  auto v = LowerMin(lhs,rhs);
  return v == lhs ? rhs : lhs;
}

// C) < 3]
inline Float64Predicate::NumberPoint UpperMin( const Float64Predicate::NumberPoint& lhs ,
                                               const Float64Predicate::NumberPoint& rhs ) {
  if(lhs.value < rhs.value)
    return lhs;
  else if(lhs.value == rhs.value) {
    if(lhs.close && !rhs.close)
      return rhs;
    else if(!lhs.close && rhs.close)
      return lhs;
  }

  return rhs;
}

inline Float64Predicate::NumberPoint UpperMax( const Float64Predicate::NumberPoint& lhs,
                                               const Float64Predicate::NumberPoint& rhs ) {
  auto v = UpperMin(lhs,rhs);
  return v == lhs ? rhs : lhs;
}

} // namespace

// TODO:: Refactory ? This code is so so messy
int Float64Predicate::Range::Test( const Range& range ) const {
  if(lower == range.lower && upper == range.upper) {
    return Float64Predicate::SAME;
  } else if(upper.value < range.lower.value ||
           (upper.value == range.lower.value && (upper.close ^ range.lower.close))) {
    return Float64Predicate::LEXCLUDE;
  } else if(lower.value > range.upper.value ||
           (lower.value == range.upper.value && (lower.close ^ range.upper.close))) {
    return Float64Predicate::REXCLUDE;
  } else if((range.lower.value > lower.value ||
            ((range.lower.value == lower.value) && (!range.lower.close && lower.close)) ||
            (range.lower == lower)) &&
            (range.upper.value < upper.value ||
            ((range.upper.value == upper.value) && (!range.upper.close && upper.close)) ||
            (range.upper == upper))) {

    return Float64Predicate::INCLUDE;
  } else {
    // special cases that looks like this :
    // ...,A) (A,...
    // Both ends are equal , but they are not overlapped
    if((upper == range.lower && !upper.close))
      return Float64Predicate::LEXCLUDE;
    else if((lower == range.upper && !lower.close))
      return Float64Predicate::REXCLUDE;
    else
      return Float64Predicate::OVERLAP;
  }
}

int Float64Predicate::Scan( const Range& range , std::int64_t* lower , std::int64_t* upper ) const {
  std::int64_t start = -1;
  std::int64_t end   = -1;
  int  rcode = -1;

  lava_debug(NORMAL,lava_verify(!sets_.empty()););

  const int len = static_cast<int>(sets_.size());

  for( int i = 0 ; i < len ; ++i ) {
    auto ret = sets_[i].Test(range);

    switch(ret) {
      case Predicate::INCLUDE:
        if(start == -1) {
          start = i;
          end = i + 1;
          rcode = Predicate::INCLUDE;
        }
        goto done;
      case Predicate::OVERLAP:
        if(start == -1) {
          start = i;
          rcode = Predicate::OVERLAP;
        }
        break;
      case Predicate::REXCLUDE:
        if(start == -1) {
          start = i;
          rcode = Predicate::REXCLUDE;
        }
        end = i;
        goto done;
      case Predicate::LEXCLUDE:
        lava_debug(NORMAL,lava_verify(start == -1););
        break; // continue search
      case Predicate::SAME:
        start = i; end = i + 1;
        rcode = Predicate::SAME;
        goto done;
      default: lava_die(); break;
    }
  }
done:
  if(start == -1 && end == -1) {
    lava_debug(NORMAL,lava_verify(rcode == -1););
    // When we reach here it means the whole range is LEXCLUDE from
    // the input range, so we just need to set where to insert the
    // range
    start = end = sets_.size();
    rcode = Predicate::LEXCLUDE;

  } else if(end == -1) {
    end = sets_.size();
  }

  if(start != -1) {
    lava_debug(NORMAL,lava_verify(end !=-1););
    lava_debug(NORMAL,lava_verify(rcode != -1););

    *lower = start;
    *upper = end;
  }

  return rcode;
}

void Float64Predicate::Merge( std::int64_t index ) {
  std::int64_t pitr = 0 , nitr = 0;
  bool has_pitr = false;
  bool has_nitr = false;
  auto &rng= sets_[index];
  // check left hand side range
  if(index >0) {
    auto prev = index - 1;
    auto &lhs = sets_[prev];

    if(lhs.upper.value == rng.lower.value &&
      (lhs.upper.close || rng.lower.close)) {
      pitr = prev; has_pitr = true;
      rng.lower  = lhs.lower;
    }
  }
  // check right hand side range
  {
    auto next = index + 1;
    if(static_cast<std::size_t>(next) < sets_.size()) {
      auto &rhs = sets_[next];
      if(rhs.lower.value == rng.upper.value &&
        (rhs.lower.close || rng.upper.close)) {
        nitr = next; has_nitr = true;
        rng.upper = rhs.upper;
      }
    }
  }
  if(has_pitr) sets_.Remove(pitr);
  if(has_nitr) sets_.Remove(nitr);
}

Float64Predicate::Range Float64Predicate::NewRange( Binary::Operator op , double value ) const {
  switch(op) {
    case Binary::GT: return Range( NumberPoint(value,false) , NumberPoint::kPosInf );
    case Binary::GE: return Range( NumberPoint(value,true ) , NumberPoint::kPosInf );
    case Binary::LT: return Range( NumberPoint::kNegInf , NumberPoint(value,false) );
    case Binary::LE: return Range( NumberPoint::kNegInf , NumberPoint(value,true ) );
    case Binary::EQ: return Range( NumberPoint(value,true) , NumberPoint(value,true));
    default: lava_die(); return Range();
  }
}

void Float64Predicate::Union( Binary::Operator op , Expr* value ) {
  lava_debug(NORMAL,lava_verify(value->IsFloat64()););
  Union(op,value->AsFloat64()->value());
}

void Float64Predicate::UnionRange( const Range& range ) {
  if(sets_.empty()) {
    sets_.Add(zone_,range);
  } else {
    std::int64_t lower, upper;
    auto ret = Scan(range,&lower,&upper);
    std::int64_t modify_pos;
    switch(ret) {
      case Predicate::SAME:
      case Predicate::INCLUDE:
        lava_debug(NORMAL,lava_verify(lower == upper-1););
        break;
      case Predicate::REXCLUDE:
        lava_debug(NORMAL,lava_verify(lower == upper););
        modify_pos = sets_.Insert(zone_,lower,range).cursor();
        break;
      case Predicate::LEXCLUDE:
        lava_debug(NORMAL,lava_verify(lower == upper););
        lava_debug(NORMAL,lava_verify(lower == static_cast<int>(sets_.size())););
        sets_.Add(zone_,range);
        modify_pos = (sets_.size() - 1);
        break;
      case Predicate::OVERLAP:
        {
          lava_debug(NORMAL,lava_verify(upper-lower >=1););

          auto rng_lower = LowerMin( range.lower , sets_[lower  ].lower );
          auto rng_upper = UpperMax( range.upper , sets_[upper-1].upper );
          auto pos       = sets_.Remove(lower,upper);
          modify_pos     = sets_.Insert(zone_,pos,Range(rng_lower,rng_upper)).cursor();
        }
        break;

      default: lava_die(); break;
    }
    if(ret == Predicate::REXCLUDE || ret == Predicate::LEXCLUDE || ret == Predicate::OVERLAP) {
      Merge(modify_pos);
    }
  }
}

void Float64Predicate::Union( Binary::Operator op , double value ) {
  if(op != Binary::NE) {
    UnionRange(NewRange(op,value));
  } else {
    Union( Binary::LT , value ); // (-@,value)
    Union( Binary::GT , value ); // (value,+@)
  }
}

void Float64Predicate::Union( const Predicate& range ) {
  lava_debug(NORMAL,lava_verify(range.IsFloat64Predicate()););
  auto &r = static_cast<const Float64Predicate&>(range);
  for( std::size_t i = 0 ; i < r.sets_.size() ; ++i ) {
    UnionRange(r.sets_[i]);
  }
}

void Float64Predicate::Intersect( Binary::Operator op , Expr* value ) {
  lava_debug(NORMAL,lava_verify(value->IsFloat64()););
  Intersect(op,value->AsFloat64()->value());
}

void Float64Predicate::IntersectRange( const Range& range ) {
  if(!sets_.empty()) {
    std::int64_t lower , upper;
    auto ret = Scan(range,&lower,&upper);

    switch(ret) {
      case Predicate::INCLUDE:
        lava_debug(NORMAL,lava_verify(lower == upper-1););
        sets_[lower] = range;
        break;
      case Predicate::SAME:
        lava_debug(NORMAL,lava_verify(lower == upper-1););
        break;
      case Predicate::LEXCLUDE: // empty set
      case Predicate::REXCLUDE:
        sets_.Clear();
        break;
      case Predicate::OVERLAP:
        {
          lava_debug(NORMAL,lava_verify(upper - lower >=1););
          /**
           * This part for intersection is relatively hard since we need to
           * find out all the overlapped section and modify accordingly
           */
          for( ; lower != upper ; ++lower ) {
            auto &existed = sets_[lower];
            existed.lower = LowerMax(existed.lower,range.lower);
            existed.upper = UpperMin(existed.upper,range.upper);
          }

          // for intersection we don't need to do merge since the opereation
          // is more constraint in a monolithic way. basically if we don't
          // have overlap before intersection, we will not have overlap after
          // intersection
        }
        break;
      default: lava_die(); break;
    }
  }
}

void Float64Predicate::Intersect( Binary::Operator op , double value ) {
  if(op != Binary::NE) {
    IntersectRange( NewRange(op,value) );
  } else {
    // We convert intersection of a != C to be a set operation
    // as following:
    //
    // a != C -> (-@,C) U (C,+@)
    //
    // Which is:
    // |this| ^ (a != C) -> |this| ^ ( (-@,C) U (C,+@) )
    //
    // Per distribution law, we get simplfiied formula as:
    //
    // |this| U (-@,C) ^ |this| U (-@,C)
    Float64Predicate temp(*this);
    Union(Binary::LT,value);
    temp.Union(Binary::GT,value);
    Union(temp);
  }
}

void Float64Predicate::Intersect( const Predicate& range ) {
  lava_debug(NORMAL,lava_verify(range.IsFloat64Predicate()););
  auto &r = static_cast<const Float64Predicate&>(range);
  lava_foreach( auto k , r.sets_.GetForwardIterator() ) {
    IntersectRange(k);
  }
}

int Float64Predicate::InferRange( const Range& range ) const {
  if(sets_.empty()) return Predicate::UNKNOWN; // empty set is included by any set

  auto r = range.Test(sets_.First());

  for( std::size_t i = 0 ; i < sets_.size() ; ++i ) {
    auto ret = range.Test(sets_[i]);
    switch(ret) {
      case Predicate::INCLUDE:
      case Predicate::SAME:
        if(r == Predicate::INCLUDE || r == Predicate::SAME)
          continue;
        else
          return Predicate::UNKNOWN;
      case Predicate::LEXCLUDE:
      case Predicate::REXCLUDE:
        if(r == Predicate::LEXCLUDE || r == Predicate::REXCLUDE)
          continue;
        else
          return Predicate::UNKNOWN;
      default:
        return Predicate::UNKNOWN;
    }
  }

  if(r == Predicate::INCLUDE || r == Predicate::SAME)
    return Predicate::ALWAYS_TRUE;
  else
    return Predicate::ALWAYS_FALSE;
}

int Float64Predicate::Infer( Binary::Operator op , double value ) const {
  if(op != Binary::NE) {
    auto range = NewRange(op,value);
    return InferRange(range);
  } else {
    auto r = Infer( Binary::EQ , value );
    switch(r) {
      case Predicate::ALWAYS_TRUE:
        return Predicate::ALWAYS_FALSE;
      case Predicate::ALWAYS_FALSE:
        return Predicate::ALWAYS_TRUE;
      default:
        return Predicate::UNKNOWN;
    }
  }
}

int Float64Predicate::Infer( Binary::Operator op , Expr* value ) const {
  if(value->IsFloat64()) {
    return Infer(op,value->AsFloat64()->value());
  }
  return Predicate::UNKNOWN;
}

int Float64Predicate::Contain( const Range& range ) const {
  for( std::size_t i = 0 ; i < sets_.size() ; ++i ) {
    auto &e= sets_[i];

    auto r = e.Test(range);
    switch(r) {
      case Predicate::SAME:
      case Predicate::INCLUDE:
        return Predicate::ALWAYS_TRUE;
      case Predicate::LEXCLUDE:
        continue;
      case Predicate::REXCLUDE:
        return Predicate::ALWAYS_FALSE;
      case Predicate::OVERLAP:
        return Predicate::UNKNOWN;
      default:
        lava_die();
        return Predicate::UNKNOWN;
    }
  }
  return Predicate::ALWAYS_FALSE;
}

int Float64Predicate::Infer( const Predicate& range ) const {
  if(range.IsFloat64Predicate()) {
    // TODO:: This is a O(n^2) algorithm , if performance is a
    //        problem we may need to optimize it
    auto &r = static_cast<const Float64Predicate&>(range);
    // Handle empty set properly
    if(sets_.empty())   return Predicate::UNKNOWN;
    if(r.sets_.empty()) return Predicate::UNKNOWN;
    auto rcode = r.Contain(sets_.First());
    if(rcode == Predicate::UNKNOWN)
      return Predicate::UNKNOWN;
    for( std::size_t i = 1 ; i < sets_.size(); ++i ) {
      auto ret = r.Contain(sets_[i]);
      if(ret != rcode) return Predicate::UNKNOWN;
    }
    return rcode;
  }
  return Predicate::UNKNOWN;
}

bool  Float64Predicate::Collapse( double* output ) const {
  if(sets_.size() == 1 ) {
    auto &r = sets_.First();
    if(r.IsSingleton()) {
      *output = r.lower.value;
      return true;
    }
  }
  return false;
}

Expr* Float64Predicate::Collapse( Graph* graph ) const {
  double v;
  if(Collapse(&v)) {
    return Float64::New(graph,v);
  }
  return NULL;
}

void Float64Predicate::Dump( DumpWriter* writer ) const {
  writer->WriteL("-----------------------------------------------");
  if(sets_.empty()) {
    writer->WriteL("empty");
  } else {
    lava_foreach( auto &r , sets_.GetForwardIterator() ) {
      writer->WriteL("%s%f,%f%s",r.lower.close ? "[" : "(" ,
                                 r.lower.value ,
                                 r.upper.value ,
                                 r.upper.close ? "]" : ")" );
    }
  }
  writer->WriteL("-----------------------------------------------");
}

/* --------------------------------------------------------------------------
 *
 * UnknownPredicate Implementation
 *
 * -------------------------------------------------------------------------*/
UnknownPredicate* UnknownPredicate::Get() {
  static UnknownPredicate kRange;
  return &kRange;
}

void UnknownPredicate::Union( Binary::Operator op , Expr* value ) {
  (void)op;
  (void)value;
  return;
}

void UnknownPredicate::Union( const Predicate& v ) {
  (void)v;
  return;
}

void UnknownPredicate::Intersect( Binary::Operator op, Expr* value ) {
  (void)op;
  (void)value;
  return;
}

void UnknownPredicate::Intersect( const Predicate& r ) {
  (void)r;
  return;
}

int UnknownPredicate::Infer( Binary::Operator op , Expr* value ) const {
  (void)op;
  (void)value;
  return Predicate::UNKNOWN;
}

int UnknownPredicate::Infer( const Predicate& r ) const {
  (void)r;
  return Predicate::UNKNOWN;
}

Expr* UnknownPredicate::Collapse( Graph* graph ) const {
  (void)graph;
  return NULL;
}

void UnknownPredicate::Dump( DumpWriter* writer ) const {
  writer->WriteL("-----------------------------------------------");
  writer->WriteL("empty");
  writer->WriteL("-----------------------------------------------");
}

/* --------------------------------------------------------------------------
 *
 * BooleanPredicate Implementation
 *
 * -------------------------------------------------------------------------*/
void BooleanPredicate::Union( bool value ) {
  switch(state_) {
    case TRUE:  state_ = value ? TRUE : ANY  ; break;
    case FALSE: state_ = value ? ANY  : FALSE; break;
    case EMPTY: state_ = value ? TRUE : FALSE; break;
    case ANY:   break;
    default: lava_die(); break;
  }
}

void BooleanPredicate::Union( const Predicate& range ) {
  lava_debug(NORMAL,lava_verify(range.IsBooleanPredicate()););

  auto &r = static_cast<const BooleanPredicate&>(range);
  switch(state_) {
    case TRUE: state_ = (r.state_ == FALSE || r.state_ == ANY) ? ANY : TRUE ; break;
    case FALSE:state_ = (r.state_ == TRUE  || r.state_ == ANY) ? ANY : FALSE; break;
    case EMPTY:state_ =  r.state_; break;
    case ANY:  break;
    default: lava_die(); break;
  }
}

void BooleanPredicate::Union( Binary::Operator op , bool value ) {
  lava_debug(NORMAL,lava_verify(op == Binary::EQ || op == Binary::NE););
  if(op == Binary::EQ)
    Union(value);
  else
    Union(!value);
}

void BooleanPredicate::Union( Binary::Operator op , Expr* value ) {
  lava_debug(NORMAL,lava_verify(value->IsBoolean()););
  Union(op,value->AsBoolean()->value());
}

void BooleanPredicate::Intersect( bool value ) {
  switch(state_) {
    case TRUE:  state_ = value ? TRUE : EMPTY; break;
    case FALSE: state_ = value ? EMPTY: FALSE; break;
    case EMPTY: break;
    case ANY:   state_ = value ? TRUE : FALSE; break;
    default: lava_die(); break;
  }
}

void BooleanPredicate::Intersect( Binary::Operator op , bool value ) {
  lava_debug(NORMAL,lava_verify(op == Binary::EQ || op == Binary::NE););
  if(op == Binary::EQ)
    Intersect(value);
  else
    Intersect(!value);
}

void BooleanPredicate::Intersect( Binary::Operator op , Expr* value ) {
  lava_debug(NORMAL,lava_verify(value->IsBoolean()););
  Intersect(op,value->AsBoolean()->value());
}

void BooleanPredicate::Intersect( const Predicate& range ) {
  lava_debug(NORMAL,lava_verify(range.IsBooleanPredicate()););

  auto &r = static_cast<const BooleanPredicate&>(range);
  switch(state_) {
    case TRUE: state_ = (r.state_ == TRUE || r.state_ == ANY) ? TRUE : EMPTY; break;
    case FALSE:state_ = (r.state_ == FALSE|| r.state_ == ANY) ? FALSE: EMPTY; break;
    case EMPTY:break;
    case ANY:  state_ = r.state_; break;
    default: lava_die(); break;
  }
}

int BooleanPredicate::Infer( Binary::Operator op , bool value ) const {
  lava_debug(NORMAL,lava_verify(op == Binary::EQ || op == Binary::NE););
  value = (op == Binary::EQ) ? value : !value;

  switch(state_) {
    case TRUE:  return value ? Predicate::ALWAYS_TRUE : Predicate::ALWAYS_FALSE;
    case FALSE: return value ? Predicate::ALWAYS_FALSE : Predicate::ALWAYS_TRUE;
    case EMPTY: return Predicate::UNKNOWN;
    case ANY:   return Predicate::UNKNOWN;

    default: lava_die(); return Predicate::UNKNOWN;
  }
}

int BooleanPredicate::Infer( const Predicate& range ) const {
  if(range.IsBooleanPredicate()) {
    auto &r = static_cast<const BooleanPredicate&>(range);
    switch(state_) {
      case ANY: case EMPTY:
        return Predicate::UNKNOWN;
      case TRUE: case FALSE:
        return (state_ == r.state_ || r.state_ == ANY ) ?  Predicate::ALWAYS_TRUE :
                                                           Predicate::ALWAYS_FALSE;
      default:
        lava_die();
        return Predicate::UNKNOWN;
    }
  }
  return Predicate::UNKNOWN;
}

int BooleanPredicate::Infer( Binary::Operator op , Expr* value ) const {
  if(value->IsBoolean()) {
    return Infer(op,value->AsBoolean()->value());
  }
  return Predicate::UNKNOWN;
}

bool BooleanPredicate::Collapse( bool* output ) const {
  switch(state_) {
    case TRUE: *output = true; return true;
    case FALSE:*output = false;return true;
    default: return false;
  }
}

Expr* BooleanPredicate::Collapse( Graph* graph ) const {
  bool v;
  if(Collapse(&v)) return Boolean::New(graph,v);
  return NULL;
}

void BooleanPredicate::Dump( DumpWriter* writer ) const {
  writer->WriteL("-----------------------------------------------");
  switch(state_) {
    case TRUE: writer->WriteL("true"); break;
    case FALSE:writer->WriteL("false");break;
    case EMPTY:writer->WriteL("empty");break;
    case ANY:  writer->WriteL("any");  break;
    default: lava_die(); break;
  }
  writer->WriteL("-----------------------------------------------");
}

/* --------------------------------------------------------------------------
 *
 * TypePredicate Implementation
 *
 * -------------------------------------------------------------------------*/
void TypePredicate::Union( TypeKind tp ) {
  auto itr = set_.Find(tp);
  if(!itr.HasNext()) set_.Add(zone_,tp);
}

void TypePredicate::Union( Binary::Operator op , Expr* node ) {
  (void)op;
  lava_debug(NORMAL,lava_verify(node->IsTestType()););
  auto tt = node->AsTestType();
  Union(tt->type_kind());
}

void TypePredicate::Union( const Predicate& range ) {
  lava_debug(NORMAL,lava_verify(range.IsTypePredicate()););
  auto &rng = static_cast<const TypePredicate&>(range);
  for( auto itr = rng.set_.GetForwardIterator(); itr.HasNext(); itr.Move() ) {
    Union(itr.value());
  }
}

void TypePredicate::Intersect( TypeKind tp ) {
  auto itr = set_.Find(tp);
  if(itr.HasNext()) { set_.Clear(); set_.Add(zone_,tp); }
  else set_.Clear();
}

void TypePredicate::Intersect( Binary::Operator op , Expr* node ) {
  (void)op;
  lava_debug(NORMAL,lava_verify(node->IsTestType()););
  auto tt = node->AsTestType();
  Intersect(tt->type_kind());
}

void TypePredicate::Intersect( const Predicate& range ) {
  lava_debug(NORMAL,lava_verify(range.IsTypePredicate()););
  auto &rng = static_cast<const TypePredicate&>(range);
  ::lavascript::zone::Vector<TypeKind>::Intersect( zone_ , set_ , rng.set_ , &set_ );
}

int TypePredicate::Infer( Binary::Operator op , Expr* node ) const {
  (void)op;
  lava_debug(NORMAL,lava_verify(node->IsTestType()););
  auto tt = node->AsTestType();
  return set_.Find(tt->type_kind()).HasNext() ? Predicate::ALWAYS_TRUE :
                                                Predicate::ALWAYS_FALSE;
}

int TypePredicate::Infer( const Predicate& range ) const {
  lava_debug(NORMAL,lava_verify(range.IsTypePredicate()););
  auto &rng = static_cast<const TypePredicate&>(range);
  // since the relationship is *or* , just need to find at least one
  // appear in |this| set from input range.
  lava_foreach( auto k , rng.set_.GetForwardIterator() ) {
    if(set_.Find(k).HasNext()) return Predicate::ALWAYS_TRUE;
  }
  return Predicate::UNKNOWN;
}

void TypePredicate::Dump( DumpWriter* writer ) const {
  writer->WriteL("-----------------------------------------------");
  if(set_.empty()) {
    writer->WriteL("empty");
  } else {
    lava_foreach( auto k , set_.GetForwardIterator() ) {
      writer->Write("%s;",GetTypeKindName(k));
    }
  }
  writer->WriteL("-----------------------------------------------");
}

TypePredicate::TypePredicate( ::lavascript::zone::Zone* zone ):
  Predicate( TYPE_PREDICATE ),
  zone_(zone),
  set_ ()
{}

TypePredicate::TypePredicate( ::lavascript::zone::Zone* zone , TypeKind t ):
  Predicate( TYPE_PREDICATE ),
  zone_(zone),
  set_ (zone,1)
{
  set_.Add(zone,t);
}

TypePredicate::TypePredicate( const TypePredicate& that ):
  Predicate( that ),
  zone_(that.zone_),
  set_ (that.zone_,that.set_)
{}

} // namespace hir
} // namespace cbase
} // namespace lavascript
