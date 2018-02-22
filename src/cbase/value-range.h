#ifndef CBASE_VALUE_RANGE_H_
#define CBASE_VALUE_RANGE_H_
#include "hir.h"
#include "src/double.h"

#include <vector>

namespace lavascript {
namespace cbase      {
namespace hir        {


// ValueRange object represents a set of values and it supports set operations like
// Union and Intersect. Also user can use ValueRange as a way to infer whether a
// certain predicate's true false value.
class ValueRange {
 public:
  // Test whether a range's relationship with regards to another value range
  enum { INCLUDE , OVERLAP , EXCLUDE , SAME };

  virtual int Test( const ValueRange& ) = 0;

  // Union a value range
  virtual void Union( Binary::Operator , Expr* ) = 0;

  // Intersect a value into the range
  virtual void Intersect( Binary::Operator , Expr* ) = 0;

  // Do a inference of a predicate
  virtual bool Infer( Binary::Operator , Expr* ) = 0;

  virtual ~ValueRange() {}
};

// Float64 value range object represents value range with type float64
class Float64ValueRange : public ValueRange {
 public:
  Float64ValueRange( Binary::Operator , double );
  Float64ValueRange( const Float64ValueRange&  );

  virtual void Union( Binary::Operator op , Expr* );
  virtual void Intersect( Binary::Operator op , Expr* );
  virtual bool Infer( Binary::Operator , Expr* );

 private:
  void Union( Binary::Operator op , double );
  void Intersect( Binary::Operator op, double );
 private:

  // represents a segment's end , it can be used to represent
  // upper bound or lower bound and it can be used to mark as
  // open or closed.
  //
  // the NumberPoint is ordered , and its order are defined as
  // follow :
  //
  // the value of NumberPoint represents order , when the value
  // are the same, then we define :
  //
  // for closed number point the value of number point are the value;
  // otherwise the value of number point is , there exists a delta T
  // that approaching 0 the value of number point is T + value
  struct NumberPoint {
    double value;       // value of the number point
    bool   close;       // whether this is closed number point

    inline bool operator  < ( const NumberPoint& ) const;
    inline bool operator <= ( const NumberPoint& ) const;
    inline bool operator  > ( const NumberPoint& ) const;
    inline bool operator >= ( const NumberPoint& ) const;
    inline bool operator == ( const NumberPoint& ) const;
    inline bool operator != ( const NumberPoint& ) const;
  };

  // represents a segment/range on a number axis
  struct Range {
    NumberPoint upper;
    NumberPoint lower;

    inline bool IsSingleton () const;

    inline int  Test     ( const Range& ) const;
    inline void Union    ( const Range& ) const;
    inline void Intersect( const Range& ) const;

    bool IsInclude( const Range& range ) const { return Test(range) == ValueRange::INCLUDE; }
    bool IsOverlap( const Range& range ) const { return Test(range) == ValueRange::OVERLAP; }
    bool IsExclude( const Range& range ) const { return Test(range) == ValueRange::EXCLUDE; }
    bool IsSame   ( const Range& range ) const { return Test(range) == ValueRange::SAME;    }
  };

  // the range stored inside of the std::vector must be
  // 1) none-overlapped
  // 2) sorted
  std::vector<Range> sets_;
};

inline bool Float64ValueRange::NumberPoint::operator < ( const NumberPoint& that ) const {
  if(value == that.value) {
    if((close && that.close) || (!close && !that.close))
      return false; // they are equal
    else
      return close ? false : true;
  } else {
    return value < that.value;
  }
}

inline bool Float64ValueRange::NumberPoint::operator <=( const NumberPoint& that ) const {
  if(value == that.value) {
    if((close && that.close) || (!close && !that.close))
      return true;
    else
      return close ? false : true;
  } else {
    return value < that.value;
  }
}

inline bool Float64ValueRange::NumberPoint::operator > ( const NumberPoint& that ) const {
  return !(*this <= that);
}

inline bool Float64ValueRange::NumberPoint::operator >=( const NumberPoint& that ) const {
  return !(*this < that);
}

inline bool Float64ValueRange::NumberPoint::operator ==( const NumberPoint& that ) const {
  if(value == that.value) {
    if((close && that.close) || (!close && !that.close))
      return true;
  }
  return false;
}

inline bool Float64ValueRange::NumberPoint::operator !=( const NumberPoint& that ) const {
  return !(*this == that);
}

inline Float64ValueRange::Range::Test( const Range& range ) const {
}

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_VALUE_RANGE_H_
