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


#define CHECK_TRUE(OP,V)                    \
  do {                                      \
    bool result;                            \
    std::cerr<<"T:"<<OpName(OP)<<V<<'\n';   \
    ASSERT_TRUE(range.Infer(OP,V,&result)); \
    ASSERT_TRUE(result);                    \
  } while(false)

#define CHECK_FALSE(OP,V)                   \
  do {                                      \
    bool result;                            \
    std::cerr<<"F:"<<OpName(OP)<<V<<'\n';   \
    ASSERT_TRUE(range.Infer(OP,V,&result)); \
    ASSERT_FALSE(result);                   \
  } while(false)

TEST(ValueRange,F64Basic) {
  {
    Float64ValueRange range;
    range.Union(Binary::EQ,5); // == 5
    range.Union(Binary::GT,5); // > 5
    {
      DumpWriter writer;
      range.Dump(&writer);
    }

    CHECK_TRUE (Binary::EQ,5);
    CHECK_TRUE (Binary::GE,5);
    CHECK_TRUE (Binary::GE,6);
    CHECK_FALSE(Binary::LT,5);
    CHECK_TRUE (Binary::EQ,7);
    CHECK_TRUE (Binary::NE,-1);
    CHECK_TRUE (Binary::NE,4.99);
  }

  {
    Float64ValueRange range;
    range.Union(Binary::LT,2); // < 2
    range.Union(Binary::EQ,2); // == 2
    {
      DumpWriter writer;
      range.Dump(&writer);
    }

    CHECK_TRUE (Binary::EQ,2);
    CHECK_TRUE (Binary::LE,2);
    CHECK_TRUE (Binary::LE,1);
    CHECK_FALSE(Binary::GT,2);
  }

  {
    Float64ValueRange range;
    range.Union(Binary::GT,5); // > 5
    range.Union(Binary::GE,5); // >=5
    {
      DumpWriter writer;
      range.Dump(&writer);
    }

    CHECK_TRUE (Binary::EQ,10);
    CHECK_TRUE (Binary::GE,5);
    CHECK_TRUE (Binary::GT,5);
    CHECK_TRUE (Binary::NE,2);
  }

  {
    Float64ValueRange range;
    range.Union(Binary::LT,5); // < 5
    range.Union(Binary::LE,5); // <=5
    {
      DumpWriter writer;
      range.Dump(&writer);
    }
  }

  {
    Float64ValueRange range;
    range.Union(Binary::GT,5);   // > 5
    range.Union(Binary::GT,100); // > 100
    {
      DumpWriter writer;
      range.Dump(&writer);
    }
  }

  {
    Float64ValueRange range;
    range.Union(Binary::LT,1); // < 1
    range.Union(Binary::LE,-10); // <= -10
    {
      DumpWriter writer;
      range.Dump(&writer);
    }
  }
  // multiple range overlap
  {
    Float64ValueRange range;
    range.Union(Binary::GT,30); // > 30
    range.Union(Binary::LT,29); // < 29
    range.Union(Binary::LE,30); // <= 30
    {
      DumpWriter writer;
      range.Dump(&writer);
    }
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
