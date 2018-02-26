#include "value-range.h"
#include "src/double.h"
#include "src/trace.h"
#include <algorithm>

namespace lavascript {
namespace cbase      {
namespace hir        {

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
  int  rcode = ValueRange::OVERLAP;

  lava_debug(NORMAL,lava_verify(!sets_.empty()););

  const int len = static_cast<int>(sets_.size());

  for( int i = 0 ; i < len ; ++i ) {
    auto ret = sets_[i].Test(range);

    switch(ret) {
      case ValueRange::INCLUDE:
        start  = i; end = i + 1;

        rcode = ValueRange::INCLUDE;
        goto done;
      case ValueRange::OVERLAP:
        if(start == -1)
          start = i;
        break;

      case ValueRange::REXCLUDE:
        if(start == -1) {
          start = i;
          rcode = ValueRange::REXCLUDE;
        } else {
          rcode = ValueRange::OVERLAP;
        }
        end = i;

        goto done;
      case ValueRange::LEXCLUDE:
        if(start != -1) {
          end = i;
          rcode = ValueRange::OVERLAP;
          goto done;
        }
        break; // continue search
      case ValueRange::SAME:
        start = i; end = i + 1;

        rcode = ValueRange::SAME;
        goto done;
      default: lava_die(); break;
    }

  }

done:
  if(end == -1) {
    end = sets_.size();
  }

  if(start != -1) {
    lava_debug(NORMAL,lava_verify(end !=-1););

    *lower = start;
    *upper = end;
  }
  return rcode;
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

    switch(ret) {
      case ValueRange::INCLUDE:
      case ValueRange::SAME:
        lava_debug(NORMAL,lava_verify(lower == upper-1););
        break;

      case ValueRange::REXCLUDE:
        lava_debug(NORMAL,lava_verify(lower == upper););
        sets_.insert(IteratorAt(sets_,lower),range);
        break;

      case ValueRange::OVERLAP:
        {
          lava_debug(NORMAL,lava_verify(upper-lower >=1););

          auto rng_lower = LowerMin( range.lower , sets_[lower  ].lower );
          auto rng_upper = UpperMax( range.upper , sets_[upper-1].upper );

          auto itr_start = IteratorAt(sets_,lower);
          auto itr_end   = IteratorAt(sets_,upper);
          auto pos       = sets_.erase (itr_start,itr_end);

          sets_.insert(pos,Range(rng_lower,rng_upper));
        }
        break;

      default: lava_die(); break;
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

      switch(ret) {
        case ValueRange::INCLUDE:
          lava_debug(NORMAL,lava_verify(lower == upper-1););
          sets_[lower] = range;
          break;
        case ValueRange::SAME:
          lava_debug(NORMAL,lava_verify(lower == upper-1););
          break;
        case ValueRange::REXCLUDE: // empty set
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
            sets_.insert(pos,Range(rng_lower,rng_upper));
          }
          break;
        default: lava_die(); break;
      }
    }
  } else {

    // We convert intersection of a != C to be a set operation as following:
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

bool Float64ValueRange::Infer( Binary::Operator op , Expr* node ) {
  return false;
}

void Float64ValueRange::Dump( DumpWriter* writer ) const {
  writer->WriteL("-----------------------------------------------");
  for( auto &r : sets_ ) {
    writer->WriteL("%s%f,%f%s",r.lower.close ? "[" : "(" ,
                               r.lower.value ,
                               r.upper.value ,
                               r.upper.close ? "]" : ")" );
  }
  writer->WriteL("-----------------------------------------------");
}

} // namespace hir
} // namespace cbase
} // namespace lavascript
