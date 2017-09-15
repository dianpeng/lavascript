#include <src/objects.h>
#include <climits>
#include <cstring>
#include <gtest/gtest.h>

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

namespace lavascript {

bool NaNEqual( double l , double r ) {
  void* lptr = reinterpret_cast<void*>(&l);
  void* rptr = reinterpret_cast<void*>(&r);
  return std::memcmp(lptr,rptr,sizeof(double)) == 0;
}

TEST(Objects,ValuePrimitive) {
  ASSERT_TRUE(Value().IsNull());
  ASSERT_TRUE(Value(1).IsInteger());
  ASSERT_TRUE(Value(1.1).IsReal());
  ASSERT_TRUE(Value(true).IsBoolean());
  {
    Value v(true);
    ASSERT_TRUE(v.IsBoolean());
    ASSERT_TRUE(v.IsTrue());
    v.SetInteger(1);
    ASSERT_TRUE(v.IsInteger());
    ASSERT_EQ(1,v.GetInteger());
    v.SetReal(1.5);
    ASSERT_TRUE(v.IsReal());
    ASSERT_EQ(1.5,v.GetReal());
    v.SetBoolean(false);
    ASSERT_FALSE(v.GetBoolean());
  }

  {
    /* Integer max and min */
    Value v(std::numeric_limits<std::int32_t>::min());
    ASSERT_TRUE(v.IsInteger());
    ASSERT_EQ(std::numeric_limits<std::int32_t>::min(),v.GetInteger());
    v.SetInteger(std::numeric_limits<std::int32_t>::max());
    ASSERT_EQ(std::numeric_limits<std::int32_t>::max(),v.GetInteger());
  }
  {
    Value v(std::numeric_limits<double>::min());
    ASSERT_TRUE(v.IsReal());
    ASSERT_EQ(std::numeric_limits<double>::min(),v.GetReal());
    v.SetReal(std::numeric_limits<double>::max());
    ASSERT_EQ(std::numeric_limits<double>::max(),v.GetReal());
    v.SetReal(std::numeric_limits<double>::quiet_NaN());
    ASSERT_TRUE(NaNEqual(std::numeric_limits<double>::quiet_NaN(),v.GetReal()));
  }
  {
    Value v(std::numeric_limits<double>::min());
    ASSERT_TRUE(v.IsReal());
    ASSERT_EQ(std::numeric_limits<double>::min(),v.GetReal());
    v.SetInteger(std::numeric_limits<std::int32_t>::max());
    ASSERT_TRUE(v.IsInteger());
    ASSERT_EQ(std::numeric_limits<std::int32_t>::max(),v.GetInteger());
    v.SetReal(std::numeric_limits<double>::max());
    ASSERT_EQ(std::numeric_limits<double>::max(),v.GetReal());
  }

  {
    Value v;
    ASSERT_TRUE(v.type() == TYPE_NULL) << v.type();
    v.SetInteger(1);
    ASSERT_TRUE(v.type() == TYPE_INTEGER) << v.type();
    v.SetReal(2.0);
    ASSERT_TRUE(v.type() == TYPE_REAL) << v.type();
    v.SetBoolean(true);
    ASSERT_TRUE(v.type() == TYPE_BOOLEAN) << v.type();
    v.SetBoolean(false);
    ASSERT_TRUE(v.type() == TYPE_BOOLEAN) << v.type();
    v.SetNull();
    ASSERT_TRUE(v.type() == TYPE_NULL) << v.type();
  }
}

HeapObject** Ptr( std::uintptr_t p ) {
  return reinterpret_cast<HeapObject**>(p);
}

/**
 * Testing pointer
 */
TEST(Objects,ValuePtr) {
  static const std::uintptr_t kLargestPointer =  0x0000ffffffffffff;
  Value v(Ptr(1));
  ASSERT_TRUE(v.IsHeapObject());
  ASSERT_TRUE(v.GetHeapObject() == Ptr(1));
  v.SetHeapObject(Ptr(kLargestPointer));
  ASSERT_TRUE(v.IsHeapObject());
  ASSERT_EQ(Ptr(kLargestPointer),v.GetHeapObject());
}

} // namespace lavascript

int main( int argc, char* argv[] ) {
  ::lavascript::InitTrace("-");
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}
