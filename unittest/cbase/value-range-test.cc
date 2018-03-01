#include <src/trace.h>
#include <src/cbase/value-range.h>

#include <gtest/gtest.h>

#include <iostream>
#include <cassert>
#include <bitset>
#include <vector>

namespace lavascript {
namespace cbase      {
namespace hir        {

const char* OpName( Binary::Operator op ) {
  switch(op) {
    case Binary::GT: return ">";
    case Binary::GE: return ">=";
    case Binary::LT: return "<";
    case Binary::LE: return "<=";
    case Binary::EQ: return "==";
    case Binary::NE: return "!=";
    default: lava_die(); return NULL;
  }
}

typedef Float64ValueRange::Range R;
typedef Float64ValueRange::NumberPoint N;

R LOpen( double r, bool c ) { return R(N::kNegInf,N(r,c)); }
R ROpen( double l, bool c ) { return R(N(l,c),N::kPosInf); }
R Single( double r )        { return R(N(r,true),N(r,true)); }


TEST(ValueRange,Range) {
  // include
  ASSERT_TRUE( LOpen(3,true).Test(LOpen(3,false)) == ValueRange::INCLUDE );  // ,3] > ,3)
  ASSERT_TRUE( ROpen(3,true).Test(ROpen(3,false)) == ValueRange::INCLUDE );  // [3, > (3,
  ASSERT_TRUE( LOpen(3,true).Test(LOpen(2,true )) == ValueRange::INCLUDE );  // ,3] > ,2)
  ASSERT_TRUE( R(-2,true,3,true).Test(R(-2,true,1,true)) == ValueRange::INCLUDE );
  ASSERT_TRUE( R(-2,true,3,true).Test(R(-1,true,3,true)) == ValueRange::INCLUDE );
  ASSERT_TRUE( R(-3,true,3,false).Test(R(-3,false,2,false)) == ValueRange::INCLUDE );

  // same
  ASSERT_TRUE( LOpen(3,true).Test(LOpen(3,true)) == ValueRange::SAME );
  ASSERT_TRUE( ROpen(3,false).Test(ROpen(3,false)) == ValueRange::SAME );
  ASSERT_TRUE( R(-2,false,3,false).Test(R(-2,false,3,false)) == ValueRange::SAME );
  ASSERT_TRUE( R(-2,true,3,true).Test(R(-2,true,3,true)) == ValueRange::SAME );

  // lexclude
  ASSERT_TRUE( LOpen(2,false).Test(ROpen(2,true)) == ValueRange::LEXCLUDE );
  ASSERT_TRUE( LOpen(1,true ).Test(ROpen(2,true)) == ValueRange::LEXCLUDE );

  // rexclude
  ASSERT_TRUE( ROpen(2,true).Test(LOpen(2,false)) == ValueRange::REXCLUDE );
  ASSERT_TRUE( ROpen(2,false).Test(LOpen(2,true)) == ValueRange::REXCLUDE );

  ASSERT_TRUE( ROpen(2,true).Test(LOpen(1,true )) == ValueRange::REXCLUDE );

  // OVERLAP
  ASSERT_TRUE( ROpen(2,true).Test(LOpen(2,true)) == ValueRange::OVERLAP  );
  ASSERT_TRUE( LOpen(1,true).Test(ROpen(1,true)) == ValueRange::OVERLAP  );

  ASSERT_TRUE( ROpen(2,true).Test(LOpen(3,true)) == ValueRange::OVERLAP  );

  // Singleton range
  ASSERT_TRUE( Single(2).Test(Single(2)) == ValueRange::SAME );
  ASSERT_TRUE( Single(2).Test(Single(1)) == ValueRange::REXCLUDE );
  ASSERT_TRUE( Single(2).Test(Single(3)) == ValueRange::LEXCLUDE );

  ASSERT_TRUE( Single(2).Test(ROpen(2,true)) == ValueRange::OVERLAP );
  ASSERT_TRUE( ROpen(2,true).Test(Single(2)) == ValueRange::INCLUDE );
  ASSERT_TRUE( Single(2).Test(ROpen(2,false)) == ValueRange::LEXCLUDE );
  ASSERT_TRUE( ROpen(2,false).Test(Single(2)) == ValueRange::REXCLUDE );

  ASSERT_TRUE( Single(2).Test(LOpen(2,true)) == ValueRange::OVERLAP );
  ASSERT_TRUE( LOpen(2,true).Test(Single(2)) == ValueRange::INCLUDE );
  ASSERT_TRUE( Single(2).Test(ROpen(2,false)) == ValueRange::LEXCLUDE );
  ASSERT_TRUE( ROpen(2,false).Test(Single(2)) == ValueRange::REXCLUDE );
}


#define CHECK_TRUE(OP,V)                    \
  do {                                      \
    std::cerr<<"T:"<<OpName(OP)<<V<<'\n';   \
    ASSERT_EQ(ValueRange::ALWAYS_TRUE,      \
              range.Infer(OP,V));           \
  } while(false)

#define CHECK_FALSE(OP,V)                   \
  do {                                      \
    std::cerr<<"F:"<<OpName(OP)<<V<<'\n';   \
    ASSERT_EQ(ValueRange::ALWAYS_FALSE,     \
              range.Infer(OP,V));           \
  } while(false)

#define CHECK_UNKNOWN(OP,V)                 \
  do {                                      \
    std::cerr<<"F:"<<OpName(OP)<<V<<'\n';   \
    ASSERT_EQ(ValueRange::UNKNOWN,          \
              range.Infer(OP,V));           \
  } while(false)

#define CHECK_F64(V)                        \
  do {                                      \
    double v;                               \
    std::cerr<<"C:"<<(V)<<'\n';             \
    ASSERT_TRUE(range.Collpase(OP,&v));     \
    ASSERT_EQ(v,(V));                       \
  } while(false)


TEST(ValueRange,F64Union) {
  {
    Float64ValueRange range;
    range.Union(Binary::EQ,5); // == 5
    range.Union(Binary::GT,5); // > 5
    range.Union(Binary::GE,5); // >= 5
    range.Union(Binary::EQ,5); // == 5

    {
      DumpWriter writer;
      range.Dump(&writer);
    }

    CHECK_TRUE (Binary::GE,5);
    CHECK_TRUE (Binary::GE,4);
    CHECK_TRUE (Binary::GT,4);
    CHECK_TRUE (Binary::NE,4.99);
    CHECK_FALSE(Binary::EQ,3);
  }

  {
    Float64ValueRange range;
    range.Union(Binary::LT,2); // < 2
    range.Union(Binary::EQ,2); // == 2
    range.Union(Binary::EQ,2);
    range.Union(Binary::LE,2);
    {
      DumpWriter writer;
      range.Dump(&writer);
    }

    CHECK_TRUE (Binary::LE,2);
    CHECK_TRUE (Binary::LE,3);
    CHECK_FALSE(Binary::GT,2);
    CHECK_FALSE(Binary::GE,3);
    CHECK_UNKNOWN(Binary::EQ,2);
    CHECK_TRUE (Binary::NE,2.1);
    CHECK_FALSE(Binary::EQ,3);
  }
}

} // namespace hir
} // namespace cbase
} // namespace lavascript

int main( int argc, char* argv[] ) {
  ::lavascript::InitTrace("-");
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}
