#include "value-range.h"
#include "src/double.h"
#include "src/trace.h"
#include <algorithm>

namespace lavascript {
namespace cbase      {
namespace hir        {

Float64ValueRange::NumberPoint Float64ValueRange::NumberPoint::kPosInf(Double::PosInf(),false);
Float64ValueRange::NumberPoint Float64ValueRange::NumberPoint::kNegInf(Double::NegInf(),false);

int Float64ValueRange::Range::Test( const Range& range ) const {
  if(lower == range.lower && upper == range.upper) {
    return Float64ValueRange::SAME;
  } else if(upper < range.lower) {
    return Float64ValueRange::LEXCLUDE;
  } else if(lower > range.upper) {
    return Float64ValueRange::REXCLUDE;
  } else if(range.lower >= lower && range.upper <= upper) {
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

bool Float64ValueRange::DoUnion( std::size_t* pitr , const Range& rhs ) {
  bool term = false;
  auto itr = *pitr;
  auto &lhs = sets_[itr];
  auto r    = lhs.Test(rhs);
  switch(r) {
    case ValueRange::INCLUDE: ++itr; break;
    case ValueRange::SAME:    ++itr; break;
    case ValueRange::EXCLUDE:
      {
        // decide order of rhs and lhs
        if(lhs.upper <= rhs.lower) {
          auto pos = IteratorAt(sets_,itr+1);
          sets_.insert(pos,rhs); // insert rhs on the *right hand side* of lhs
        } else {
          auto pos = IteratorAt(sets_,itr);
          sets_.insert(pos,rhs);   // insert rhs on the *left hand side* of lhs
        }
        itr += 2;
      }
      break;
    case ValueRange::OVERLAP:
      {
        // decide the lower bound and upper bound of both sets
        auto l = std::min(lhs.lower,rhs.lower);
        auto u = std::max(lhs.upper,rhs.upper);
        lhs.lower = l;
        lhs.upper = u;
      }
      ++itr;
      break;
    default:
      lava_die(); break;
  }

  *pitr = itr;
}

std::size_t
Float64ValueRange::DoIntersect( std::size_t itr , const Range& rhs ) {
  auto &lhs = sets_[itr];
  auto r    = lhs.Test(rhs);
  switch(r) {
    case ValueRange::INCLUDE: lhs = rhs; ++itr;       break;
    case ValueRange::SAME:    ++itr;                  break;
    case ValueRange::EXCLUDE:
      {
        auto pos = IteratorAt(sets_,itr);
        sets_.erase(pos);
        ++itr;
      }
      break;
    case ValueRange::OVERLAP:
      {
        // decide the lower bound and upper bound of both sets
        auto l = std::max(lhs.lower,rhs.lower);
        auto r = std::min(lhs.upper,rhs.upper);
        lhs.lower = l;
        lhs.upper = r;
      }
      ++itr;
      break;
    default: lava_die();                             break;
  }
  return itr;
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

void Float64ValueRange::Union( Binary::Operator op , double value ) {
  if(op != Binary::NE) {
    auto range = NewRange(op,value);
    if(sets_.empty()) {
      sets_.push_back(range);
    } else {
      for( std::size_t i = 0 ; i < sets_.size() ; ) {
        i = DoUnion(i,range);
      }
    }
  } else {
    Union( Binary::LT , value ); // (-@,value)
    Union( Binary::GT , value ); // (value,+@)
  }
}

void Float64ValueRange::Union( const Float64ValueRange& range ) {
  // TODO:: Optimize it ?? It is a O(n^2) algorithm
  for( auto &r : range.sets_ ) {
    for( std::size_t i = 0 ; i < sets_.size(); ) {
      i = DoUnion(i,r);
    }
  }
}

void Float64ValueRange::Intersect( Binary::Operator op , Expr* value ) {
  lava_debug(NORMAL,lava_verify(value->IsFloat64()););
  Intersect(op,value->AsFloat64()->value());
}

void Float64ValueRange::Intersect( Binary::Operator op , double value ) {
  if(op != Binary::NE) {
    auto range = NewRange(op,value);
    if(sets_.empty()) {
      sets_.push_back(range);
    } else {
      for( std::size_t i = 0 ; i < sets_.size() ; ) {
        i = DoIntersect(i,range);
      }
    }
  } else {
    // a != C -> (-@,C) U (C,+@)
    // |this| ^ (a != C) -> |this| ^ ( (-@,C) U (C,+@) )
    // per distribution law, we get simplfiied formula as:
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
