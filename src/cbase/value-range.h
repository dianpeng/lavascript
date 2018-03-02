#ifndef CBASE_VALUE_RANGE_H_
#define CBASE_VALUE_RANGE_H_
#include "hir.h"

#include <memory>
#include <vector>
#include <gtest/gtest_prod.h>

namespace lavascript {
class DumpWriter;
namespace cbase      {
namespace hir        {

enum ValueRangeType {
  UNKNOWN_VALUE_RANGE,
  FLOAT64_VALUE_RANGE,
  BOOLEAN_VALUE_RANGE
};

// ValueRange object represents a set of values and it supports set operations like
// Union and Intersect. Also user can use ValueRange as a way to infer whether a
// certain predicate's true false value.
class ValueRange {
 public:
  ValueRange( ValueRangeType type ) : type_(type) {}

  virtual ~ValueRange() {}
 public:
  ValueRangeType type() const { return type_; }

  bool IsUnknownValueRange() const { return type() == UNKNOWN_VALUE_RANGE; }
  bool IsFloat64ValueRange() const { return type() == FLOAT64_VALUE_RANGE; }
  bool IsBooleanValueRange() const { return type() == BOOLEAN_VALUE_RANGE; }

  // Test whether a range's relationship with regards to another value range
  //
  // INCLUDE --> the range includes the test range
  // OVERLAP --> the range overlaps the test range
  // LEXCLUDE--> the range left hand side exclude the test range
  // REXCLUDE--> the range right hand side exclude the test range
  // SAME    --> both ranges are the same
  enum { INCLUDE , OVERLAP , LEXCLUDE , REXCLUDE , SAME };

  // Inference result
  enum { ALWAYS_TRUE , ALWAYS_FALSE , UNKNOWN };

  // Union a comparison/binary operation
  virtual void Union    ( Binary::Operator , Expr* ) = 0;

  // Union a value range object
  virtual void Union    ( const ValueRange& )        = 0;

  // Intersect a comparison/binary operation
  virtual void Intersect( Binary::Operator , Expr* ) = 0;

  // Intersect a value range object
  virtual void Intersect( const ValueRange& )        = 0;

  // Infer an expression based on the ValueRange existed. The return
  // value are :
  // 1) Always true, basically means the input range is the super set
  //    of the ValueRange's internal set ; or if ValueRange is true, the
  //    input range is always true
  //
  // 2) Always false, basically means the input range shares nothing with
  //    the ValueRange's internal set ; or it means the input range cannot
  //    be true. In DCE, it means this branch needs to be removed
  //
  // 3) Unknown , just means don't need to do anything the relationship
  //    between the input and existed ValueRange cannot be decided
  //
  virtual int Infer( Binary::Operator , Expr* ) const = 0;

  // Infer a ValueRange with |this| value range
  virtual int Infer( const ValueRange& ) const = 0;


  // Check whether we can collapse the set into a value , or simply puts
  // it does this value range represents a fixed number or boolean value ?
  //
  // It is used during GVN for inference
  virtual Expr* Collapse( Graph* , IRInfo* ) const = 0;

  // Clone another ValueRange from this ValueRange object. Used to do
  // constraint predication
  virtual std::unique_ptr<ValueRange> Clone() const = 0;

  // debug purpose
  virtual void Dump( DumpWriter* ) const = 0;

 private:
  ValueRangeType type_;
};

// Unknown ValueRange. Used to mark we cannot do anything with this constraint/
// It is a placeholder when the conditionaly constraint tries to cover multiple
// different types , eg: if(a > 3 || a == "string")
class UnknownValueRange : public ValueRange {
 public:
  virtual void  Union    ( Binary::Operator , Expr* );
  virtual void  Union    ( const ValueRange& );
  virtual void  Intersect( Binary::Operator , Expr* );
  virtual void  Intersect( const ValueRange& );

  virtual int   Infer    ( Binary::Operator , Expr* ) const;
  virtual int   Infer    ( const ValueRange& ) const;

  virtual Expr* Collapse ( Graph* , IRInfo*         ) const;

  virtual std::unique_ptr<ValueRange> Clone() const {
    return std::unique_ptr<ValueRange>( new UnknownValueRange() );
  }

  virtual void  Dump     ( DumpWriter* ) const;

  UnknownValueRange(): ValueRange( UNKNOWN_VALUE_RANGE ) {}
};


// Float64 value range object represents value range with type float64
class Float64ValueRange : public ValueRange {
 static const std::size_t kInitSize = 8;

 public:
  // represents a segment's end , it can be used to represent
  // upper bound or lower bound and it can be used to mark as
  // open or closed.
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

    int  Test       ( const Range&       ) const;
    bool IsInclude  ( const Range& range ) const { return Test(range) == ValueRange::INCLUDE;  }
    bool IsOverlap  ( const Range& range ) const { return Test(range) == ValueRange::OVERLAP;  }
    bool IsLExclude ( const Range& range ) const { return Test(range) == ValueRange::LEXCLUDE; }
    bool IsRExclude ( const Range& range ) const { return Test(range) == ValueRange::REXCLUDE; }
    bool IsSame     ( const Range& range ) const { return Test(range) == ValueRange::SAME;     }
  };

  typedef std::vector<Range> RangeSet;

 public:
  Float64ValueRange():
    ValueRange(FLOAT64_VALUE_RANGE),
    sets_()
  { sets_.reserve(kInitSize); }

  Float64ValueRange( Binary::Operator op , Expr*  value ):
    ValueRange(FLOAT64_VALUE_RANGE),
    sets_()
  { sets_.reserve(kInitSize); Union(op,value); }

  Float64ValueRange( Binary::Operator op , double value ):
    ValueRange(FLOAT64_VALUE_RANGE),
    sets_()
  { sets_.reserve(kInitSize); Union(op,value); }

  Float64ValueRange( const Float64ValueRange& that ):
    ValueRange(FLOAT64_VALUE_RANGE),
    sets_(that.sets_)
  {}

  virtual void  Union    ( Binary::Operator op , Expr* );
  virtual void  Union    ( const ValueRange& );
  virtual void  Intersect( Binary::Operator op , Expr* );
  virtual void  Intersect( const ValueRange& );

  virtual int   Infer    ( Binary::Operator , Expr* ) const;
  virtual int   Infer    ( const ValueRange&  ) const;

  virtual Expr* Collapse ( Graph* , IRInfo* ) const;
  virtual void  Dump     ( DumpWriter* ) const;

  virtual std::unique_ptr<ValueRange> Clone() const {
    return std::unique_ptr<ValueRange>( new Float64ValueRange(*this) );
  }

 private:
  Range NewRange  ( Binary::Operator op , double ) const;
  // scan the input range inside of the RangSet and find its
  // status and lower and upper bound
  int  Scan       ( const Range& , int* lower , int* upper ) const;
  // this function do a merge with adjuscent range if needed
  void Merge      ( const RangeSet::iterator& );
  void Union      ( Binary::Operator op , double );
  void Intersect  ( Binary::Operator op , double );

  void UnionRange    ( const Range& );
  void IntersectRange( const Range& );

  // check whether the input Range is contained inside of the ValueRange
  int  Contain    ( const Range& ) const;

  int  InferRange ( const Range& ) const;
  int  Infer      ( Binary::Operator , double ) const;
  bool Collapse   ( double* ) const;

 private:

  // the range stored inside of the std::vector must be
  // 1) none-overlapped
  // 2) sorted
  RangeSet sets_;

  FRIEND_TEST(ValueRange,F64Union);
  FRIEND_TEST(ValueRange,F64Intersect);
  FRIEND_TEST(ValueRange,F64All);

  void operator = ( const Float64ValueRange& ) = delete;
};

// Boolean value's ValueRange implementation
class BooleanValueRange : public ValueRange {
 private:
  enum State { EMPTY , ANY , TRUE , FALSE };
 public:
  BooleanValueRange():
    ValueRange(BOOLEAN_VALUE_RANGE),
    state_(EMPTY)
  {}

  BooleanValueRange( bool value ):
    ValueRange(BOOLEAN_VALUE_RANGE),
    state_(EMPTY)
  { Union(value); }

  BooleanValueRange( Binary::Operator op, Expr* value ):
    ValueRange(BOOLEAN_VALUE_RANGE),
    state_(EMPTY)
  { Union(op,value); }

  BooleanValueRange( const BooleanValueRange& that ):
    ValueRange(BOOLEAN_VALUE_RANGE),
    state_    (that.state_)
  {}

  virtual void  Union    ( Binary::Operator , Expr* );
  virtual void  Union    ( const ValueRange& );

  virtual void  Intersect( Binary::Operator , Expr* );
  virtual void  Intersect( const ValueRange& );

  virtual int   Infer    ( Binary::Operator , Expr* ) const;
  virtual int   Infer    ( const ValueRange& ) const;

  virtual Expr* Collapse ( Graph* , IRInfo*         ) const;
  virtual void  Dump     ( DumpWriter* ) const;

  virtual std::unique_ptr<ValueRange> Clone() const {
    return std::unique_ptr<ValueRange>( new BooleanValueRange(*this) );
  }

 private:
  void  Union    ( bool );
  void  Union    ( Binary::Operator , bool );

  void  Intersect( bool );
  void  Intersect( Binary::Operator , bool );
  int   Infer    ( Binary::Operator , bool ) const;
  bool  Collapse ( bool* ) const;

 private:
  State state_;

  FRIEND_TEST(ValueRange,BoolUnion);
  FRIEND_TEST(ValueRange,BoolIntersect);
  FRIEND_TEST(ValueRange,BoolAll);
};

inline
bool Float64ValueRange::NumberPoint::operator ==( const NumberPoint& that ) const {
  if(value == that.value) {
    if((close && that.close) || (!close && !that.close))
      return true;
  }
  return false;
}

inline
bool Float64ValueRange::NumberPoint::operator !=( const NumberPoint& that ) const {
  return !(*this == that);
}

inline
bool Float64ValueRange::Range::IsSingleton() const {
  auto r = (upper == lower);
  lava_debug(NORMAL,if(r) lava_verify(lower.close);); // we should not have empty set
  return r;
}

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_VALUE_RANGE_H_
