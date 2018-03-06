#include "value-range.h"
#include "src/double.h"
#include "src/trace.h"
#include <algorithm>

namespace lavascript {
namespace cbase      {
namespace hir        {

/* --------------------------------------------------------------------------
 *
 * Float64ValueRange Implementation
 *
 * -------------------------------------------------------------------------*/

Float64ValueRange::NumberPoint Float64ValueRange::NumberPoint::kPosInf(Double::PosInf(),false);
Float64ValueRange::NumberPoint Float64ValueRange::NumberPoint::kNegInf(Double::NegInf(),false);

namespace {

// [C < (3
inline Float64ValueRange::NumberPoint LowerMin( const Float64ValueRange::NumberPoint& lhs ,
                                                const Float64ValueRange::NumberPoint& rhs ) {
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

inline Float64ValueRange::NumberPoint LowerMax( const Float64ValueRange::NumberPoint& lhs ,
                                                const Float64ValueRange::NumberPoint& rhs ) {
  auto v = LowerMin(lhs,rhs);
  return v == lhs ? rhs : lhs;
}

// C) < 3]
inline Float64ValueRange::NumberPoint UpperMin( const Float64ValueRange::NumberPoint& lhs ,
                                                const Float64ValueRange::NumberPoint& rhs ) {
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

inline Float64ValueRange::NumberPoint UpperMax( const Float64ValueRange::NumberPoint& lhs,
                                                const Float64ValueRange::NumberPoint& rhs ) {
  auto v = UpperMin(lhs,rhs);
  return v == lhs ? rhs : lhs;
}

} // namespace

int Float64ValueRange::Range::Test( const Range& range ) const {
  if(lower == range.lower && upper == range.upper) {
    return Float64ValueRange::SAME;
  } else if(upper.value < range.lower.value ||
           (upper.value == range.lower.value && (upper.close ^ range.lower.close))) {
    return Float64ValueRange::LEXCLUDE;
  } else if(lower.value > range.upper.value ||
           (lower.value == range.upper.value && (lower.close ^ range.upper.close))) {
    return Float64ValueRange::REXCLUDE;
  } else if((range.lower.value > lower.value ||
            ((range.lower.value == lower.value) && (!range.lower.close && lower.close)) ||
            (range.lower == lower)) &&
            (range.upper.value < upper.value ||
            ((range.upper.value == upper.value) && (!range.upper.close && upper.close)) ||
            (range.upper == upper))) {

    return Float64ValueRange::INCLUDE;
  } else {
    // special cases that looks like this :
    // ...,A) (A,...
    // Both ends are equal , but they are not overlapped
    if((upper == range.lower && !upper.close))
      return Float64ValueRange::LEXCLUDE;
    else if((lower == range.upper && !lower.close))
      return Float64ValueRange::REXCLUDE;
    else
      return Float64ValueRange::OVERLAP;
  }
}

int Float64ValueRange::Scan( const Range& range , std::int64_t* lower ,
                                                  std::int64_t* upper ) const {
  std::int64_t start = -1;
  std::int64_t end   = -1;
  int  rcode = -1;

  lava_debug(NORMAL,lava_verify(!sets_.empty()););

  const int len = static_cast<int>(sets_.size());

  for( int i = 0 ; i < len ; ++i ) {
    auto ret = sets_[i].Test(range);

    switch(ret) {
      case ValueRange::INCLUDE:
        if(start == -1) {
          start = i;
          end = i + 1;
          rcode = ValueRange::INCLUDE;
        }
        goto done;

      case ValueRange::OVERLAP:
        if(start == -1) {
          start = i;
          rcode = ValueRange::OVERLAP;
        }

        break;

      case ValueRange::REXCLUDE:
        if(start == -1) {
          start = i;
          rcode = ValueRange::REXCLUDE;
        }

        end = i;
        goto done;

      case ValueRange::LEXCLUDE:
        lava_debug(NORMAL,lava_verify(start == -1););
        break; // continue search

      case ValueRange::SAME:
        start = i; end = i + 1;
        rcode = ValueRange::SAME;
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
    rcode = ValueRange::LEXCLUDE;

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

void Float64ValueRange::Merge( std::int64_t index ) {
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

Float64ValueRange::Range Float64ValueRange::NewRange( Binary::Operator op ,
                                                      double value ) const {
  switch(op) {
    case Binary::GT: return Range( NumberPoint(value,false) , NumberPoint::kPosInf );
    case Binary::GE: return Range( NumberPoint(value,true ) , NumberPoint::kPosInf );
    case Binary::LT: return Range( NumberPoint::kNegInf , NumberPoint(value,false) );
    case Binary::LE: return Range( NumberPoint::kNegInf , NumberPoint(value,true ) );
    case Binary::EQ: return Range( NumberPoint(value,true) , NumberPoint(value,true));
    default: lava_die(); return Range();
  }
}

void Float64ValueRange::Union( Binary::Operator op , Expr* value ) {
  lava_debug(NORMAL,lava_verify(value->IsFloat64()););
  Union(op,value->AsFloat64()->value());
}

void Float64ValueRange::UnionRange( const Range& range ) {
  if(sets_.empty()) {
    sets_.Add(zone_,range);
  } else {
    std::int64_t lower, upper;
    auto ret = Scan(range,&lower,&upper);
    std::int64_t modify_pos;

    switch(ret) {
      case ValueRange::SAME:
      case ValueRange::INCLUDE:
        lava_debug(NORMAL,lava_verify(lower == upper-1););
        break;

      case ValueRange::REXCLUDE:
        lava_debug(NORMAL,lava_verify(lower == upper););
        modify_pos = sets_.Insert(zone_,lower,range).cursor();
        break;

      case ValueRange::LEXCLUDE:
        lava_debug(NORMAL,lava_verify(lower == upper););
        lava_debug(NORMAL,lava_verify(lower == static_cast<int>(sets_.size())););
        sets_.Add(zone_,range);
        modify_pos = (sets_.size() - 1);
        break;
      case ValueRange::OVERLAP:
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

    if(ret == ValueRange::REXCLUDE ||
       ret == ValueRange::LEXCLUDE ||
       ret == ValueRange::OVERLAP) {
      Merge(modify_pos);
    }
  }
}

void Float64ValueRange::Union( Binary::Operator op , double value ) {
  if(op != Binary::NE) {
    UnionRange(NewRange(op,value));
  } else {
    Union( Binary::LT , value ); // (-@,value)
    Union( Binary::GT , value ); // (value,+@)
  }
}

void Float64ValueRange::Union( const ValueRange& range ) {
  lava_debug(NORMAL,lava_verify(range.IsFloat64ValueRange()););
  auto &r = static_cast<const Float64ValueRange&>(range);
  for( std::size_t i = 0 ; i < r.sets_.size() ; ++i ) {
    UnionRange(r.sets_[i]);
  }
}

void Float64ValueRange::Intersect( Binary::Operator op , Expr* value ) {
  lava_debug(NORMAL,lava_verify(value->IsFloat64()););
  Intersect(op,value->AsFloat64()->value());
}

void Float64ValueRange::IntersectRange( const Range& range ) {
  if(!sets_.empty()) {
    std::int64_t lower , upper;
    auto ret = Scan(range,&lower,&upper);

    switch(ret) {
      case ValueRange::INCLUDE:
        lava_debug(NORMAL,lava_verify(lower == upper-1););
        sets_[lower] = range;
        break;
      case ValueRange::SAME:
        lava_debug(NORMAL,lava_verify(lower == upper-1););
        break;
      case ValueRange::LEXCLUDE: // empty set
      case ValueRange::REXCLUDE:
        sets_.Clear();
        break;
      case ValueRange::OVERLAP:
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

void Float64ValueRange::Intersect( Binary::Operator op , double value ) {
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
    Float64ValueRange temp(*this);
    Union(Binary::LT,value);
    temp.Union(Binary::GT,value);
    Union(temp);
  }
}

void Float64ValueRange::Intersect( const ValueRange& range ) {
  lava_debug(NORMAL,lava_verify(range.IsFloat64ValueRange()););
  auto &r = static_cast<const Float64ValueRange&>(range);
  for( std::size_t i = 0 ; i < r.sets_.size() ; ++i ) {
    IntersectRange(r.sets_[i]);
  }
}

int Float64ValueRange::InferRange( const Range& range ) const {
  if(sets_.empty()) return ValueRange::UNKNOWN; // empty set is included by any set

  auto r = range.Test(sets_.First());

  for( std::size_t i = 0 ; i < sets_.size() ; ++i ) {
    auto ret = range.Test(sets_[i]);
    switch(ret) {
      case ValueRange::INCLUDE:
      case ValueRange::SAME:
        if(r == ValueRange::INCLUDE || r == ValueRange::SAME)
          continue;
        else
          return ValueRange::UNKNOWN;
      case ValueRange::LEXCLUDE:
      case ValueRange::REXCLUDE:
        if(r == ValueRange::LEXCLUDE || r == ValueRange::REXCLUDE)
          continue;
        else
          return ValueRange::UNKNOWN;
      default:
        return ValueRange::UNKNOWN;
    }
  }

  if(r == ValueRange::INCLUDE || r == ValueRange::SAME)
    return ValueRange::ALWAYS_TRUE;
  else
    return ValueRange::ALWAYS_FALSE;
}

int Float64ValueRange::Infer( Binary::Operator op , double value ) const {
  if(op != Binary::NE) {
    auto range = NewRange(op,value);
    return InferRange(range);
  } else {
    auto r = Infer( Binary::EQ , value );
    switch(r) {
      case ValueRange::ALWAYS_TRUE:
        return ValueRange::ALWAYS_FALSE;
      case ValueRange::ALWAYS_FALSE:
        return ValueRange::ALWAYS_TRUE;
      default:
        return ValueRange::UNKNOWN;
    }
  }
}

int Float64ValueRange::Infer( Binary::Operator op , Expr* value ) const {
  if(value->IsFloat64()) {
    return Infer(op,value->AsFloat64()->value());
  }
  return ValueRange::UNKNOWN;
}

int Float64ValueRange::Contain( const Range& range ) const {
  for( std::size_t i = 0 ; i < sets_.size() ; ++i ) {
    auto &e= sets_[i];

    auto r = e.Test(range);
    switch(r) {
      case ValueRange::SAME:
      case ValueRange::INCLUDE:
        return ValueRange::ALWAYS_TRUE;
      case ValueRange::LEXCLUDE:
        continue;
      case ValueRange::REXCLUDE:
        return ValueRange::ALWAYS_FALSE;
      case ValueRange::OVERLAP:
        return ValueRange::UNKNOWN;
      default:
        lava_die();
        return ValueRange::UNKNOWN;
    }
  }
  return ValueRange::ALWAYS_FALSE;
}

int Float64ValueRange::Infer( const ValueRange& range ) const {
  if(range.IsFloat64ValueRange()) {
    // TODO:: This is a O(n^2) algorithm , if performance is a
    //        problem we may need to optimize it
    auto &r = static_cast<const Float64ValueRange&>(range);

    // Handle empty set properly
    if(sets_.empty())   return ValueRange::UNKNOWN;
    if(r.sets_.empty()) return ValueRange::UNKNOWN;

    auto rcode = r.Contain(sets_.First());

    if(rcode == ValueRange::UNKNOWN)
      return ValueRange::UNKNOWN;

    for( std::size_t i = 1 ; i < sets_.size(); ++i ) {
      auto ret = r.Contain(sets_[i]);
      if(ret != rcode) return ValueRange::UNKNOWN;
    }

    return rcode;
  }
  return ValueRange::UNKNOWN;
}

bool  Float64ValueRange::Collapse( double* output ) const {
  if(sets_.size() == 1 ) {
    auto &r = sets_.First();
    if(r.IsSingleton()) {
      *output = r.lower.value;
      return true;
    }
  }
  return false;
}

Expr* Float64ValueRange::Collapse( Graph* graph , IRInfo* info ) const {
  double v;
  if(Collapse(&v)) {
    return Float64::New(graph,v,info);
  }
  return NULL;
}

void Float64ValueRange::Dump( DumpWriter* writer ) const {
  writer->WriteL("-----------------------------------------------");
  if(sets_.empty()) {
    writer->WriteL("empty");
  } else {
    for( std::size_t i = 0 ; i < sets_.size() ; ++i ) {
      auto &r = sets_[i];
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
 * UnknownValueRange Implementation
 *
 * -------------------------------------------------------------------------*/
UnknownValueRange* UnknownValueRange::Get() {
  static UnknownValueRange kRange;
  return &kRange;
}

void UnknownValueRange::Union( Binary::Operator op , Expr* value ) {
  (void)op;
  (void)value;
  return;
}

void UnknownValueRange::Union( const ValueRange& v ) {
  (void)v;
  return;
}

void UnknownValueRange::Intersect( Binary::Operator op, Expr* value ) {
  (void)op;
  (void)value;
  return;
}

void UnknownValueRange::Intersect( const ValueRange& r ) {
  (void)r;
  return;
}

int UnknownValueRange::Infer( Binary::Operator op , Expr* value ) const {
  (void)op;
  (void)value;
  return ValueRange::UNKNOWN;
}

int UnknownValueRange::Infer( const ValueRange& r ) const {
  (void)r;
  return ValueRange::UNKNOWN;
}

Expr* UnknownValueRange::Collapse( Graph* graph , IRInfo* info ) const {
  (void)graph;
  (void)info;
  return NULL;
}

void UnknownValueRange::Dump( DumpWriter* writer ) const {
  writer->WriteL("-----------------------------------------------");
  writer->WriteL("empty");
  writer->WriteL("-----------------------------------------------");
}


/* --------------------------------------------------------------------------
 *
 * BooleanValueRange Implementation
 *
 * -------------------------------------------------------------------------*/

void BooleanValueRange::Union( bool value ) {
  switch(state_) {
    case TRUE:  state_ = value ? TRUE : ANY  ; break;
    case FALSE: state_ = value ? ANY  : FALSE; break;
    case EMPTY: state_ = value ? TRUE : FALSE; break;
    case ANY:   break;
    default: lava_die(); break;
  }
}

void BooleanValueRange::Union( const ValueRange& range ) {
  lava_debug(NORMAL,lava_verify(range.IsBooleanValueRange()););

  auto &r = static_cast<const BooleanValueRange&>(range);
  switch(state_) {
    case TRUE: state_ = (r.state_ == FALSE || r.state_ == ANY) ? ANY : TRUE; break;
    case FALSE:state_ = (r.state_ == TRUE  || r.state_ == ANY) ? ANY : FALSE;break;
    case EMPTY:state_ =  r.state_; break;
    case ANY:  break;
    default: lava_die(); break;
  }
}

void BooleanValueRange::Union( Binary::Operator op , bool value ) {
  lava_debug(NORMAL,lava_verify(op == Binary::EQ || op == Binary::NE););
  if(op == Binary::EQ)
    Union(value);
  else
    Union(!value);
}

void BooleanValueRange::Union( Binary::Operator op , Expr* value ) {
  lava_debug(NORMAL,lava_verify(value->IsBoolean()););
  Union(op,value->AsBoolean()->value());
}

void BooleanValueRange::Intersect( bool value ) {
  switch(state_) {
    case TRUE:  state_ = value ? TRUE : EMPTY; break;
    case FALSE: state_ = value ? EMPTY: FALSE; break;
    case EMPTY: break;
    case ANY:   state_ = value ? TRUE : FALSE; break;
    default: lava_die(); break;
  }
}

void BooleanValueRange::Intersect( Binary::Operator op , bool value ) {
  lava_debug(NORMAL,lava_verify(op == Binary::EQ || op == Binary::NE););
  if(op == Binary::EQ)
    Intersect(value);
  else
    Intersect(!value);
}

void BooleanValueRange::Intersect( Binary::Operator op , Expr* value ) {
  lava_debug(NORMAL,lava_verify(value->IsBoolean()););
  Intersect(op,value->AsBoolean()->value());
}

void BooleanValueRange::Intersect( const ValueRange& range ) {
  lava_debug(NORMAL,lava_verify(range.IsBooleanValueRange()););

  auto &r = static_cast<const BooleanValueRange&>(range);
  switch(state_) {
    case TRUE: state_ = (r.state_ == TRUE || r.state_ == ANY) ? TRUE : EMPTY; break;
    case FALSE:state_ = (r.state_ == FALSE|| r.state_ == ANY) ? FALSE: EMPTY; break;
    case EMPTY:break;
    case ANY:  state_ = r.state_; break;
    default: lava_die(); break;
  }
}

int BooleanValueRange::Infer( Binary::Operator op , bool value ) const {
  lava_debug(NORMAL,lava_verify(op == Binary::EQ || op == Binary::NE););
  value = (op == Binary::EQ) ? value : !value;

  switch(state_) {
    case TRUE:  return value ? ValueRange::ALWAYS_TRUE :
                               ValueRange::ALWAYS_FALSE;

    case FALSE: return value ? ValueRange::ALWAYS_FALSE :
                               ValueRange::ALWAYS_TRUE;

    case EMPTY: return ValueRange::UNKNOWN;
    case ANY:   return ValueRange::UNKNOWN;

    default: lava_die(); return ValueRange::UNKNOWN;
  }
}

int BooleanValueRange::Infer( const ValueRange& range ) const {
  if(range.IsBooleanValueRange()) {
    auto &r = static_cast<const BooleanValueRange&>(range);
    switch(state_) {
      case ANY:
      case EMPTY:
        return ValueRange::UNKNOWN;

      case TRUE:
      case FALSE:
        return (state_ == r.state_ || r.state_ == ANY ) ?
          ValueRange::ALWAYS_TRUE : ValueRange::ALWAYS_FALSE;

      default:
        lava_die();
        return ValueRange::UNKNOWN;
    }
  }
  return ValueRange::UNKNOWN;
}

int BooleanValueRange::Infer( Binary::Operator op , Expr* value ) const {
  if(value->IsBoolean()) {
    return Infer(op,value->AsBoolean()->value());
  }
  return ValueRange::UNKNOWN;
}

bool BooleanValueRange::Collapse( bool* output ) const {
  switch(state_) {
    case TRUE: *output = true; return true;
    case FALSE:*output = false;return true;
    default: return false;
  }
}

Expr* BooleanValueRange::Collapse( Graph* graph , IRInfo* info ) const {
  bool v;
  if(Collapse(&v)) return Boolean::New(graph,v,info);
  return NULL;
}

void BooleanValueRange::Dump( DumpWriter* writer ) const {
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

} // namespace hir
} // namespace cbase
} // namespace lavascript
