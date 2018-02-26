#ifndef CBASE_VALUE_RANGE_H_
#define CBASE_VALUE_RANGE_H_
#include "hir.h"

#include <vector>
#include <gtest/gtest_prod.h>

namespace lavascript {
class DumpWriter;

namespace cbase      {
namespace hir        {


// ValueRange object represents a set of values and it supports set operations like
// Union and Intersect. Also user can use ValueRange as a way to infer whether a
// certain predicate's true false value.
class ValueRange {
 public:
  // Test whether a range's relationship with regards to another value range
  //
  // INCLUDE --> the range includes the test range
  // OVERLAP --> the range overlaps the test range
  // LEXCLUDE--> the range left hand side exclude the test range
  // REXCLUDE--> the range right hand side exclude the test range
  // SAME    --> both ranges are the same
  enum { INCLUDE , OVERLAP , LEXCLUDE , REXCLUDE , SAME };

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
  // represents a segment's end , it can be used to represent
  // upper bound or lower bound and it can be used to mark as
  // open or closed.
  //
  // the NumberPoint is ordered , and its order are defined as
  // follow :
  //
  // [C < (3 == 3) < 3]
  //
  struct NumberPoint {
    static NumberPoint kPosInf;
    static NumberPoint kNegInf;

    double value;       // value of the number point
    bool   close;       // whether this is closed number point

    NumberPoint() : value() , close() {}
    NumberPoint( double v , bool c ) : value(v), close(c) {}

    inline bool operator == ( const NumberPoint& ) const;
    inline bool operator != ( const NumberPoint& ) const;
  };

  // represents a segment/range on a number axis
  struct Range {
    static Range kAll;

    NumberPoint lower;
    NumberPoint upper;

    Range() : lower() , upper() {} // undefined

    Range( double l , bool cl , double u , bool cu ):
      lower(NumberPoint(l,cl)) , upper(NumberPoint(u,cu))
    {}

    Range( const NumberPoint& l , const NumberPoint& u ):
      lower(l) , upper(u)
    {}

    inline bool IsSingleton () const;
    int  Test       ( const Range& ) const;
    bool IsInclude  ( const Range& range ) const { return Test(range) == ValueRange::INCLUDE; }
    bool IsOverlap  ( const Range& range ) const { return Test(range) == ValueRange::OVERLAP; }
    bool IsLExclude ( const Range& range ) const { return Test(range) == ValueRange::LEXCLUDE; }
    bool IsRExclude ( const Range& range ) const { return Test(range) == ValueRange::REXCLUDE; }
    bool IsSame     ( const Range& range ) const { return Test(range) == ValueRange::SAME;    }
  };

  typedef std::vector<Range> RangeSet;

 public:
  Float64ValueRange():sets_() {}
  Float64ValueRange( Binary::Operator op , double value ): sets_() { Union(op,value); }
  Float64ValueRange( const Float64ValueRange& that ):sets_(that.sets_) {}

  virtual void Union( Binary::Operator op , Expr* );
  virtual void Intersect( Binary::Operator op , Expr* );
  virtual bool Infer( Binary::Operator , Expr* );

 public:
  void Dump( DumpWriter* ) const;

 private:
  Range NewRange ( Binary::Operator op , double ) const;

  // scan the input range inside of the RangSet and find its
  // status and lower and upper bound
  int  Scan( const Range& , int* lower , int* upper ) const;

  void Union    ( Binary::Operator op , double );
  void Intersect( Binary::Operator op , double );

  void Union( const Float64ValueRange& );
  void UnionRange ( const Range& );

 private:

  // the range stored inside of the std::vector must be
  // 1) none-overlapped
  // 2) sorted
  RangeSet sets_;

  FRIEND_TEST(ValueRange,F64Basic);
  FRIEND_TEST(ValueRange,F64Infer);

  void operator = ( const Float64ValueRange& ) = delete;
};

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

inline bool Float64ValueRange::Range::IsSingleton() const {
  auto r = (upper == lower);
  lava_debug(NORMAL,if(r) lava_verify(lower.close);); // we should not have empty set
  return r;
}

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_VALUE_RANGE_H_
