#ifndef CBASE_PREDICATE_H_
#define CBASE_PREDICATE_H_
#include "hir.h"
#include "type.h"
#include "src/zone/zone.h"
#include "src/zone/vector.h"
#include <gtest/gtest_prod.h>

namespace lavascript {
class DumpWriter;
namespace cbase      {
namespace hir        {

// MUST start with 0
enum PredicateType {
  FLOAT64_PREDICATE = 0,
  BOOLEAN_PREDICATE,
  TYPE_PREDICATE,
  UNKNOWN_PREDICATE,
};

// This function is used to decide which types of predicate this expression can be. Joint
// predicate currently is not supported due to the complexity and uncommon to happen. Eg,
//
// if( a > 1 && a < 2 ) ==> float64_predicate
// if( a )              ==> boolean_predicate
// guard(a == 'string') or if(type(a) == 'string') ==> type_predicate
//
// things like if(a > 1 && a < 2 || a) ==> unknown_predicate
//
// For unknown predicate it will pollute the optimization afterwards , blocks that is dominated
// by this blocks. Look at optimization/infer.h for more information

struct PredicateClassifyResult {
  PredicateType type;
  Expr*         main_variable;
  PredicateClassifyResult() : type(UNKNOWN_PREDICATE) , main_variable(NULL) {}
  PredicateClassifyResult( PredicateType t , Expr* m) : type(t) , main_variable(m) {}
};

PredicateClassifyResult ClassifyPredicate( Expr* );

// Predicate object represents a set of values and it supports set operations like
// Union and Intersect. It is used during inference optimization phase. It is critical
// since conersion , inference , null check elimination , bound check elimination and
// type check elimination all rely on it.
class Predicate : public zone::ZoneObject {
 public:
  Predicate( PredicateType type ) : type_(type) {}
  virtual ~Predicate() {}
 public:
  PredicateType type() const { return type_; }
  bool IsUnknownPredicate() const { return type() == UNKNOWN_PREDICATE; }
  bool IsFloat64Predicate() const { return type() == FLOAT64_PREDICATE; }
  bool IsBooleanPredicate() const { return type() == BOOLEAN_PREDICATE; }
  bool IsTypePredicate   () const { return type() == TYPE_PREDICATE;    }
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
  virtual void Union    ( const Predicate& )        = 0;
  // Intersect a comparison/binary operation
  virtual void Intersect( Binary::Operator , Expr* ) = 0;
  // Intersect a value range object
  virtual void Intersect( const Predicate& )        = 0;
  // Infer an expression based on the Predicate existed. The return
  // value are :
  // 1) Always true, basically means the input range is the super set
  //    of the Predicate's internal set ; or if Predicate is true, the
  //    input range is always true
  //
  // 2) Always false, basically means the input range shares nothing with
  //    the Predicate's internal set ; or it means the input range cannot
  //    be true. In DCE, it means this branch needs to be removed
  //
  // 3) Unknown , just means don't need to do anything the relationship
  //    between the input and existed Predicate cannot be decided
  //
  virtual int Infer( Binary::Operator , Expr* ) const = 0;
  // Infer a Predicate with |this| value range
  virtual int Infer( const Predicate& ) const = 0;
  // Check whether we can collapse the set into a value , or simply puts
  // it does this value range represents a fixed number or boolean value ?
  //
  // It is used during GVN for inference
  virtual Expr* Collapse( Graph* , IRInfo* ) const = 0;
  // debug purpose
  virtual void Dump( DumpWriter* ) const = 0;
  // Check if the value range is empty set or not
  virtual bool IsEmpty() const = 0;
 private:
  PredicateType type_;
};

// Unknown Predicate. Used to mark we cannot do anything with this constraint/
// It is a placeholder when the conditionaly constraint tries to cover multiple
// different types , eg: if(a > 3 || a == "string")
class UnknownPredicate : public Predicate {
 public:
  // Use this function to get a unknown value range object's pointer
  // Since it is a singleton , we can save some memory
  static UnknownPredicate* Get();

  virtual void  Union    ( Binary::Operator , Expr* );
  virtual void  Union    ( const Predicate& );
  virtual void  Intersect( Binary::Operator , Expr* );
  virtual void  Intersect( const Predicate& );
  virtual int   Infer    ( Binary::Operator , Expr* ) const;
  virtual int   Infer    ( const Predicate& ) const;
  virtual Expr* Collapse ( Graph* , IRInfo*         ) const;
  virtual void  Dump     ( DumpWriter* ) const;
  virtual bool  IsEmpty  () const { return false; }

 private:
  UnknownPredicate(): Predicate( UNKNOWN_PREDICATE ) {}
};

// Float64 value range object represents value range with type float64
class Float64Predicate : public Predicate {
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
  struct Range : zone::ZoneObject {
    NumberPoint lower;
    NumberPoint upper;
    Range() : lower() , upper() {} // undefined
    Range( double l , bool cl , double u , bool cu ): lower(NumberPoint(l,cl)) , upper(NumberPoint(u,cu)) {}
    Range( const NumberPoint& l , const NumberPoint& u ): lower(l) , upper(u) {}
    inline bool IsSingleton () const;
    int  Test       ( const Range&       ) const;
    bool IsInclude  ( const Range& range ) const { return Test(range) == Predicate::INCLUDE;  }
    bool IsOverlap  ( const Range& range ) const { return Test(range) == Predicate::OVERLAP;  }
    bool IsLExclude ( const Range& range ) const { return Test(range) == Predicate::LEXCLUDE; }
    bool IsRExclude ( const Range& range ) const { return Test(range) == Predicate::REXCLUDE; }
    bool IsSame     ( const Range& range ) const { return Test(range) == Predicate::SAME;     }
  };

  typedef zone::Vector<Range> RangeSet;
 public:
  Float64Predicate( zone::Zone* zone ):
    Predicate(FLOAT64_PREDICATE),
    sets_(zone,kInitSize),
    zone_(zone)
  {}

  Float64Predicate( zone::Zone* zone , Binary::Operator op , Expr*  value ):
    Predicate(FLOAT64_PREDICATE),
    sets_(zone,kInitSize),
    zone_(zone)
  { Union(op,value); }

  Float64Predicate( zone::Zone* zone , Binary::Operator op , double value ):
    Predicate(FLOAT64_PREDICATE),
    sets_(zone,kInitSize),
    zone_(zone)
  { Union(op,value); }

  Float64Predicate( const Float64Predicate& that ):
    Predicate(FLOAT64_PREDICATE),
    sets_(that.zone_,that.sets_),
    zone_(that.zone_)
  {}

  virtual void  Union    ( Binary::Operator op , Expr* );
  virtual void  Union    ( const Predicate& );
  virtual void  Intersect( Binary::Operator op , Expr* );
  virtual void  Intersect( const Predicate& );
  virtual int   Infer    ( Binary::Operator , Expr* ) const;
  virtual int   Infer    ( const Predicate&  ) const;
  virtual Expr* Collapse ( Graph* , IRInfo* ) const;
  virtual void  Dump     ( DumpWriter* ) const;
  virtual bool  IsEmpty  () const { return sets_.empty(); }

 private:
  Range NewRange  ( Binary::Operator op , double ) const;
  // scan the input range inside of the RangSet and find its
  // status and lower and upper bound
  int  Scan       ( const Range& , std::int64_t* lower , std::int64_t* upper ) const;
  // this function do a merge with adjuscent range if needed
  void Merge      ( std::int64_t index );
  void Union      ( Binary::Operator op , double );
  void Intersect  ( Binary::Operator op , double );

  void UnionRange    ( const Range& );
  void IntersectRange( const Range& );

  // check whether the input Range is contained inside of the Predicate
  int  Contain    ( const Range& ) const;

  int  InferRange ( const Range& ) const;
  int  Infer      ( Binary::Operator , double ) const;
  bool Collapse   ( double* ) const;

 private:

  // the range stored inside of the std::vector must be
  // 1) none-overlapped
  // 2) sorted
  RangeSet    sets_;
  zone::Zone* zone_;

  FRIEND_TEST(Predicate,F64Union);
  FRIEND_TEST(Predicate,F64Intersect);
  FRIEND_TEST(Predicate,F64All);

  LAVA_DISALLOW_ASSIGN(Float64Predicate);
};

// Boolean value's Predicate implementation
class BooleanPredicate : public Predicate {
 private:
  enum State { EMPTY , ANY , TRUE , FALSE };
 public:
  BooleanPredicate():
    Predicate(BOOLEAN_PREDICATE),
    state_(EMPTY)
  {}

  BooleanPredicate( bool value ):
    Predicate(BOOLEAN_PREDICATE),
    state_(EMPTY)
  { Union(value); }

  BooleanPredicate( Binary::Operator op, Expr* value ):
    Predicate(BOOLEAN_PREDICATE),
    state_(EMPTY)
  { Union(op,value); }

  BooleanPredicate( const BooleanPredicate& that ):
    Predicate(BOOLEAN_PREDICATE),
    state_    (that.state_)
  {}

  virtual void  Union    ( Binary::Operator , Expr* );
  virtual void  Union    ( const Predicate& );

  virtual void  Intersect( Binary::Operator , Expr* );
  virtual void  Intersect( const Predicate& );

  virtual int   Infer    ( Binary::Operator , Expr* ) const;
  virtual int   Infer    ( const Predicate& ) const;

  virtual Expr* Collapse ( Graph* , IRInfo*         ) const;
  virtual void  Dump     ( DumpWriter* ) const;
  virtual bool  IsEmpty  () const { return state_ == EMPTY; }

 private:
  void  Union    ( bool );
  void  Union    ( Binary::Operator , bool );

  void  Intersect( bool );
  void  Intersect( Binary::Operator , bool );
  int   Infer    ( Binary::Operator , bool ) const;
  bool  Collapse ( bool* ) const;

 private:
  State state_;

  FRIEND_TEST(Predicate,BoolUnion);
  FRIEND_TEST(Predicate,BoolIntersect);
  FRIEND_TEST(Predicate,BoolAll);
};

// Type value range basically represents a simple disjoint set. Basically for any node,
// since the type is none-overlapping (we don't support overlapped type currently ),the
// value range of type can only be disjoint.
class TypePredicate : public Predicate {
 public:
  // A type value range initialized as an *empty* set
  TypePredicate( ::lavascript::zone::Zone* );
  TypePredicate( ::lavascript::zone::Zone* , TypeKind );
  TypePredicate( const TypePredicate& that );

  // For TypePredicate , all the Binary::Operator argument is ignored , only based on
  // the input Expr's node we are able to tell which kind of operation we need to do
  virtual void  Union    ( Binary::Operator , Expr* );
  virtual void  Union    ( const Predicate& );
  virtual void  Intersect( Binary::Operator , Expr* );
  virtual void  Intersect( const Predicate& );
  virtual int   Infer    ( Binary::Operator , Expr* ) const;
  virtual int   Infer    ( const Predicate& ) const;
  virtual Expr* Collapse ( Graph* , IRInfo* ) const { return NULL; }
  virtual void  Dump     ( DumpWriter* ) const;
  virtual bool  IsEmpty  () const { return set_.empty(); }
 private:
  void Union   ( TypeKind );
  void Intersect( TypeKind );
 private:
  ::lavascript::zone::Zone*            zone_;
  ::lavascript::zone::Vector<TypeKind> set_;

  LAVA_DISALLOW_ASSIGN(TypePredicate)
};

inline
bool Float64Predicate::NumberPoint::operator ==( const NumberPoint& that ) const {
  if(value == that.value) {
    if((close && that.close) || (!close && !that.close))
      return true;
  }
  return false;
}

inline
bool Float64Predicate::NumberPoint::operator !=( const NumberPoint& that ) const {
  return !(*this == that);
}

inline
bool Float64Predicate::Range::IsSingleton() const {
  auto r = (upper == lower);
  lava_debug(NORMAL,if(r) lava_verify(lower.close);); // we should not have empty set
  return r;
}

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_PREDICATE_H_
