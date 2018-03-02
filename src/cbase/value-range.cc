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

int Float64ValueRange::Scan( const Range& range , int* lower , int* upper ) const {
  auto start = -1;
  auto end   = -1;
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

void Float64ValueRange::Merge( const Float64ValueRange::RangeSet::iterator& itr ) {
  RangeSet::iterator pitr,nitr;
  bool has_pitr = false;
  bool has_nitr = false;

  auto &rng= *itr;

  // check left hand side range
  if(sets_.begin() != itr) {
    auto prev(itr); --prev;
    auto &lhs = *prev;
    if(lhs.upper.value == rng.lower.value &&
      (lhs.upper.close || rng.lower.close)) {
      pitr = prev; has_pitr = true;
      itr->lower = lhs.lower;
    }
  }

  // check right hand side range
  {
    auto next(itr); ++next;
    auto &rhs = *next;

    if(next != sets_.end()) {
      if(rhs.lower.value == rng.upper.value &&
        (rhs.lower.close || rng.upper.close)) {
        nitr = next; has_nitr = true;
        itr->upper = rhs.upper;
      }
    }
  }

  if(has_pitr) sets_.erase(pitr);
  if(has_nitr) sets_.erase(nitr);
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
    sets_.push_back(range);
  } else {
    int lower, upper;
    auto ret = Scan(range,&lower,&upper);
    RangeSet::iterator modify_pos;

    switch(ret) {
      case ValueRange::SAME:
      case ValueRange::INCLUDE:
        lava_debug(NORMAL,lava_verify(lower == upper-1););
        break;

      case ValueRange::REXCLUDE:
        lava_debug(NORMAL,lava_verify(lower == upper););
        modify_pos = sets_.insert(IteratorAt(sets_,lower),range);
        break;

      case ValueRange::LEXCLUDE:
        lava_debug(NORMAL,lava_verify(lower == upper););
        lava_debug(NORMAL,lava_verify(lower == static_cast<int>(sets_.size())););
        modify_pos = sets_.insert(sets_.end(),range);
        break;
      case ValueRange::OVERLAP:
        {
          lava_debug(NORMAL,lava_verify(upper-lower >=1););

          auto rng_lower = LowerMin( range.lower , sets_[lower  ].lower );
          auto rng_upper = UpperMax( range.upper , sets_[upper-1].upper );

          auto itr_start = IteratorAt(sets_,lower);
          auto itr_end   = IteratorAt(sets_,upper);
          auto pos       = sets_.erase (itr_start,itr_end);
          modify_pos     = sets_.insert(pos,Range(rng_lower,rng_upper));
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

void Float64ValueRange::Union( const Float64ValueRange& range ) {
  for( auto &r : range.sets_ ) {
    UnionRange(r);
  }
}

void Float64ValueRange::Intersect( Binary::Operator op , Expr* value ) {
  lava_debug(NORMAL,lava_verify(value->IsFloat64()););
  Intersect(op,value->AsFloat64()->value());
}

void Float64ValueRange::Intersect( Binary::Operator op , double value ) {
  if(op != Binary::NE) {
    if(!sets_.empty()) {
      auto range = NewRange(op,value);
      int  lower , upper;
      auto ret = Scan(range,&lower,&upper);
      RangeSet::iterator modify_pos;

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
          sets_.clear();
          break;
        case ValueRange::OVERLAP:
          {
            lava_debug(NORMAL,lava_verify(upper - lower >=1););

            auto rng_lower = LowerMax( range.lower , sets_[lower  ].lower );
            auto rng_upper = UpperMin( range.upper , sets_[upper-1].upper );

            auto itr_start = IteratorAt(sets_,lower);
            auto itr_end   = IteratorAt(sets_,upper);
            auto pos       = sets_.erase(itr_start,itr_end);
            modify_pos     = sets_.insert(pos,Range(rng_lower,rng_upper));
          }
          break;
        default: lava_die(); break;
      }

      if(ret == ValueRange::OVERLAP) {
        Merge(modify_pos);
      }
    }
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

int Float64ValueRange::Infer( Binary::Operator op , Expr* value ) const {
  lava_debug(NORMAL,lava_verify(value->IsFloat64()););
  return Infer(op,value->AsFloat64()->value());
}

int Float64ValueRange::Infer( Binary::Operator op , double value ) const {
  if(op != Binary::NE) {
    if(sets_.empty()) return ValueRange::UNKNOWN; // empty set is included by any set

    auto range = NewRange(op,value);
    auto r     = range.Test(sets_.front());

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

bool  Float64ValueRange::Collapse( double* output ) const {
  if(sets_.size() == 1 ) {
    auto &r = sets_.front();
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
    for( auto &r : sets_ ) {
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

void UnknownValueRange::Union( Binary::Operator op , Expr* value ) {
  (void)op;
  (void)value;
  return;
}

void UnknownValueRange::Intersect( Binary::Operator op, Expr* value ) {
  (void)op;
  (void)value;
  return;
}

int UnknownValueRange::Infer( Binary::Operator op , Expr* value ) const {
  (void)op;
  (void)value;
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
    case INIT:  state_ = value ? TRUE : FALSE; break;
    case TRUE:  state_ = value ? TRUE : ANY  ; break;
    case FALSE: state_ = value ? ANY  : FALSE; break;
    case EMPTY: state_ = value ? TRUE : FALSE; break;
    case ANY:   break;
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
    case INIT:  state_ = value ? TRUE : FALSE; break;
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

int BooleanValueRange::Infer( Binary::Operator op , bool value ) const {
  lava_debug(NORMAL,lava_verify(op == Binary::EQ || op == Binary::NE););
  value = (op == Binary::EQ) ? value : !value;

  switch(state_) {
    case INIT:  return ValueRange::UNKNOWN;

    case TRUE:  return value ? ValueRange::ALWAYS_TRUE :
                               ValueRange::ALWAYS_FALSE;

    case FALSE: return value ? ValueRange::ALWAYS_FALSE :
                               ValueRange::ALWAYS_TRUE;

    case EMPTY: return ValueRange::ALWAYS_FALSE;
    case ANY:   return ValueRange::UNKNOWN;
    default: lava_die(); return ValueRange::UNKNOWN;
  }
}

int BooleanValueRange::Infer( Binary::Operator op , Expr* value ) const {
  lava_debug(NORMAL,lava_verify(value->IsBoolean()););
  return Infer(op,value->AsBoolean()->value());
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
    case INIT: writer->WriteL("init"); break;
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
