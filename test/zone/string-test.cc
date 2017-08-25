#include <src/zone/string.h>
#include <src/core/trace.h>
#include <gtest/gtest.h>
#include <cstdlib>

namespace lavascript {
namespace zone {

std::string RndStr( size_t length ) {
  std::string buf;
  buf.reserve(length);
  static const char kArr[] = {
    '+','-','*','/','a','c','d','e','f','g','h','i','k','j','u','0','A','B','C','D'
  };
  static const int kArrLen = sizeof(kArr);
  for( size_t i = 0 ; i < length ; ++i ) {
    buf.push_back( kArr[ rand() % kArrLen ] );
  }
  return buf;
}

TEST(Zone,String) {
  Zone zone(1,4);

  {
    String string;
    ASSERT_TRUE( strcmp(string.data(),"") == 0 );
    ASSERT_EQ  ( 0 , string.size() );
  }

  {
    String string(&zone,"ABC");
    ASSERT_TRUE( strcmp(string.data(),"ABC") == 0);
    ASSERT_EQ  ( 3 , string.size() );
  }

  {
    String string(&zone,"ABCD");
    ASSERT_EQ(string,"ABCD");
    ASSERT_EQ(4,string.size());
  }

  {
    const char* str = "ABCDEFFFFFFFFFFFFFFFFFFFFFF";
    String string(&zone,str);
    ASSERT_EQ( string , str );
    ASSERT_EQ( strlen(str) , string.size() );
  }

  {
    /**
     * Do a simple brutle force testing against the Zone to see
     * 1) can handle large chunk of random allocation
     * 2) can handle large chunk of single random allocation
     * 3) do we have leak
     */
    std::vector<std::string> arr;
    std::vector<String*> arr_str;
    for( size_t i = 0 ; i < 10240 ; ++i ) {
      std::string rand_str = RndStr(i+1);
      arr.push_back(rand_str);
      arr_str.push_back( String::New( &zone , rand_str ) );
    }

    for( size_t i = 0 ; i < 10240 ; ++i ) {
      ASSERT_EQ( *arr_str[i] , arr[i] );
    }
  }

}

} // namespace zone
} // namespace lavascript

int main( int argc, char* argv[] ) {
  srand(0);
  ::lavascript::core::InitTrace("-");
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}
