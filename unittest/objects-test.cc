#include <src/objects.h>
#include <src/gc.h>
#include <climits>
#include <cstring>
#include <gtest/gtest.h>
#include <random>
#include <src/trace.h>
#include <math.h>

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
  ASSERT_TRUE(Value(1.1).IsReal());
  ASSERT_TRUE(Value(true).IsBoolean());
  {
    Value v(true);
    ASSERT_TRUE(v.IsBoolean());
    ASSERT_TRUE(v.IsTrue());
    v.SetReal(1.5);
    ASSERT_TRUE(v.IsReal());
    ASSERT_EQ(1.5,v.GetReal());
    v.SetBoolean(false);
    ASSERT_FALSE(v.GetBoolean());
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

    v.SetReal(std::numeric_limits<double>::max());
    ASSERT_EQ(std::numeric_limits<double>::max(),v.GetReal());
  }

  {
    Value v;
    ASSERT_TRUE(v.type() == TYPE_NULL) << v.type();
    v.SetReal(2.0);
    ASSERT_TRUE(v.type() == TYPE_REAL) << v.type();
    v.SetBoolean(true);
    ASSERT_TRUE(v.type() == TYPE_BOOLEAN) << v.type();
    v.SetBoolean(false);
    ASSERT_TRUE(v.type() == TYPE_BOOLEAN) << v.type();
    v.SetNull();
    ASSERT_TRUE(v.type() == TYPE_NULL) << v.type();
  }

  // infinity value
  {
    Value v(std::numeric_limits<double>::infinity());
    ASSERT_TRUE(v.type() == TYPE_REAL);
    ASSERT_TRUE(v.GetReal() == std::numeric_limits<double>::infinity());
  }

  // nan
  {
    Value v(std::numeric_limits<double>::quiet_NaN());
    ASSERT_TRUE(v.type() == TYPE_REAL);
    ASSERT_TRUE(isnan(v.GetReal()));
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


/**
 * ====================================================================
 *
 * Testing Objects
 *
 * ==================================================================*/

std::string RandStr( std::size_t length ) {
  std::random_device device;
  std::default_random_engine el(device());
  std::uniform_int_distribution<std::uint64_t>
    dist(1,std::numeric_limits<std::uint64_t>::max());

  static const char kChar[] = { 'a','b','c','d','e','f','g',
                                'h','A','B','C','D','E','F',
                                '+','-','*','/','&','$','%',
                                '@','Z','z','X','y','U','u',
                                '<','>','?','"','[',']','{',
                                '}'
  };
  static const std::size_t kArrSize = sizeof(kChar);
  std::string buf;
  buf.reserve(length);
  for( std::size_t i = 0 ; i < length ; ++i ) {
    std::size_t idx = dist(el) % kArrSize;
    buf.push_back(kChar[idx]);
  }
  return buf;
}

std::size_t RandRange( std::size_t start , std::size_t end ) {
  std::random_device device;
  std::default_random_engine el(device());
  std::uniform_int_distribution<std::size_t>
    dist(1,std::numeric_limits<std::size_t>::max());

  lava_verify(start < end);

  std::size_t range = (end-start);
  std::size_t r = dist(el);
  return r % range + start;
}

TEST(Objects,String) {
  GC gc(NULL);

  {
    Handle<String> empty_string(String::New(&gc));
    ASSERT_TRUE( !empty_string.IsNull() );
    ASSERT_TRUE(  empty_string->size() == 0 );
    ASSERT_TRUE(  *empty_string == "" );
    ASSERT_TRUE(  *empty_string == std::string() );

    Handle<String> another(String::New(&gc));
    ASSERT_TRUE( !empty_string.IsNull() );
    ASSERT_TRUE(  empty_string->size() == 0 );
    ASSERT_TRUE(  *empty_string == "" );
    ASSERT_TRUE(  *empty_string == std::string() );

    ASSERT_TRUE(  *empty_string == *another );
  }

  {
    std::vector<std::string> expect_set;
    std::vector<Handle<String>> string_set;

    for( std::size_t i = 0 ; i < 10000 ; ++i ) {
      std::string str =  RandStr( RandRange( 2 , kSSOMaxSize+1 ) );
      expect_set.push_back(str);
      Handle<String> gc_str( String::New(&gc,str) );
      ASSERT_TRUE( *gc_str == str );
      ASSERT_TRUE( gc_str->IsSSO());
      string_set.push_back(gc_str);
    }

    for( std::size_t i = 0 ; i < 10000 ; ++i ) {
      const std::string& lhs = expect_set[i];
      const Handle<String>& rhs = string_set[i];
      ASSERT_TRUE( *rhs == lhs );
      ASSERT_TRUE( rhs->IsSSO() );
    }
  }

  {
    std::vector<std::string> expect_set;
    std::vector<Handle<String>> string_set;
    for( std::size_t i = 0 ; i < 1000 ; ++i ) {
      std::string str = RandStr( RandRange(kSSOMaxSize+1,1024) );
      expect_set.push_back(str);
      Handle<String> gc_str( String::New(&gc,str) );
      ASSERT_TRUE(gc_str->IsLongString());
      string_set.push_back(gc_str);
      ASSERT_TRUE(*gc_str == str);
    }

    for( std::size_t i = 0 ; i < 1000 ; ++i ) {
      const std::string& lhs = expect_set[i];
      const Handle<String>& rhs = string_set[i];
      ASSERT_TRUE( *rhs == lhs );
      ASSERT_TRUE( rhs->IsLongString() );
    }
  }

  /* ------------------------------------------------------
   * String operator testing
   * -----------------------------------------------------*/
  {
    Handle<String> empty_string(String::New(&gc));
    ASSERT_TRUE(*empty_string == "");
    ASSERT_TRUE(*empty_string == std::string());
    ASSERT_TRUE(*empty_string == *String::New(&gc));
    ASSERT_TRUE(*empty_string >= "");
    ASSERT_TRUE(*empty_string >= std::string());
    ASSERT_TRUE(*empty_string >= *String::New(&gc));
    ASSERT_TRUE(*empty_string <= "");
    ASSERT_TRUE(*empty_string <= std::string());
    ASSERT_TRUE(*empty_string <= *String::New(&gc));
  }

  {
    {
      std::string str(RandStr(127));
      Handle<String> gcstr( String::New(&gc,str) );
      ASSERT_TRUE( *gcstr == str );

      /* just mutate a character */
      str[126] = str[126] + 1;

      ASSERT_TRUE( *gcstr != str );
      ASSERT_TRUE( *gcstr <  str );
      ASSERT_TRUE( *gcstr <= str );

      str[126] = str[126] - 2;

      ASSERT_TRUE( *gcstr != str );
      ASSERT_TRUE( *gcstr >  str );
      ASSERT_TRUE( *gcstr >= str );
    }
    {
      std::string sstr( RandStr(127) );
      std::string lstr( sstr );
      lstr.append("SOME-MORE");

      Handle<String> gcsstr( String::New(&gc,sstr) );
      Handle<String> gclstr( String::New(&gc,lstr) );

      ASSERT_TRUE( *gcsstr != *gclstr );
      ASSERT_FALSE( *gcsstr == *gclstr );
      ASSERT_TRUE( *gcsstr < *gclstr );
      ASSERT_TRUE( *gclstr > *gcsstr );
      ASSERT_TRUE( *gcsstr <= *gclstr );
      ASSERT_TRUE( *gclstr >= *gcsstr );

      ASSERT_TRUE( *gcsstr == sstr );
      ASSERT_FALSE(*gcsstr != sstr );
      ASSERT_TRUE( *gclstr == lstr );
      ASSERT_FALSE(*gclstr != lstr );

      ASSERT_TRUE( *gcsstr < lstr );
      ASSERT_TRUE( *gcsstr <= lstr);
      ASSERT_TRUE( *gcsstr < lstr.c_str() );
      ASSERT_TRUE( *gcsstr <= lstr.c_str() );

      ASSERT_TRUE( *gclstr > sstr );
      ASSERT_TRUE( *gclstr >= sstr );
      ASSERT_TRUE( *gclstr > sstr.c_str() );
      ASSERT_TRUE( *gclstr >= sstr.c_str() );
    }
  }

  /* SSO string comparison */
  {
    gc::SSOPool sso_pool(2,1,1,NULL);
    {
      SSO* empty_string = sso_pool.Get("",0);
      ASSERT_TRUE(*empty_string == "");
      ASSERT_TRUE(*empty_string == std::string());
      ASSERT_TRUE(*empty_string == *sso_pool.Get("",0));
      ASSERT_TRUE(*empty_string == *String::New(&gc,""));
    }
    {
      SSO* str = sso_pool.Get("abcde",5);
      ASSERT_TRUE(*str == "abcde");
      ASSERT_TRUE(*str == std::string("abcde"));
      ASSERT_TRUE(*str == *String::New(&gc,"abcde"));
      ASSERT_TRUE(*str == *sso_pool.Get("abcde",5));

      ASSERT_TRUE(*str < "abcdf");
      ASSERT_TRUE(*str < std::string("abcdf"));
      ASSERT_TRUE(*str < *String::New(&gc,"abcdf"));
      ASSERT_TRUE(*str < *sso_pool.Get("abcdf",5));

      ASSERT_TRUE(*str < "abcdef");
      ASSERT_TRUE(*str < std::string("abcdef"));
      ASSERT_TRUE(*str < *String::New(&gc,"abcdef"));
      ASSERT_TRUE(*str < *sso_pool.Get("abcdef",6));

      ASSERT_TRUE(*str > "abcd");
      ASSERT_TRUE(*str > std::string("abcd"));
      ASSERT_TRUE(*str > *String::New(&gc,"abcd"));
      ASSERT_TRUE(*str > *sso_pool.Get("abcd",4));

      ASSERT_TRUE(*str >= "abcde");
      ASSERT_TRUE(*str >= std::string("abcde"));
      ASSERT_TRUE(*str >= *String::New(&gc,"abcde"));
      ASSERT_TRUE(*str >= *sso_pool.Get("abcde",5));

      ASSERT_TRUE(*str <= "abcde");
      ASSERT_TRUE(*str <= std::string("abcde"));
      ASSERT_TRUE(*str <= *String::New(&gc,"abcde"));
      ASSERT_TRUE(*str <= *sso_pool.Get("abcde",5));
    }
  }
}

TEST(Slice,Slice) {
  GC gc(NULL);
  {
    Handle<Slice> slice(Slice::New(&gc,8));
    ASSERT_EQ(8,slice->capacity());
    ASSERT_FALSE(slice->IsEmpty());
    for( std::size_t i = 0 ; i < 8 ; ++i ) {
      ASSERT_TRUE( slice->Index(i).IsNull() );
    }
  }
  {
    Handle<Slice> slice(Slice::New(&gc,1024));
    ASSERT_EQ(1024,slice->capacity());
    ASSERT_FALSE(slice->IsEmpty());
    std::vector<std::string> str_vec;
    for( std::size_t i = 0 ; i < 1024 ; ++i ) {
      std::string str(RandStr(RandRange(2,1024)));
      str_vec.push_back(str);
      slice->Index(i).SetHandle(String::New(&gc,str));
    }

    for( std::size_t i = 0 ; i < 1024 ; ++i ) {
      ASSERT_TRUE(slice->Index(i).IsString());
      Value v = slice->Index(i);
      Handle<String> str(v.GetString());
      ASSERT_TRUE(*str == str_vec[i]);
    }
  }
}

TEST(List,List) {
  GC gc(NULL);
  {
    Handle<List> list(List::New(&gc,8));
    ASSERT_EQ(0,list->size());
    ASSERT_EQ(8,list->capacity());
    ASSERT_TRUE(list->IsEmpty());

    for( std::size_t i = 0 ; i < 1000 ; ++i ) {
      list->Push(&gc,Value(static_cast<double>(i)));
    }

    ASSERT_EQ(1000,list->size());
    ASSERT_FALSE(list->IsEmpty());

    for( std::size_t i = 0 ; i < 1000 ; ++i ) {
      ASSERT_TRUE(list->Index(i).IsReal());
      ASSERT_EQ(i,list->Index(i).GetReal());
    }
  }

  {
    Handle<List> list(List::New(&gc,8));
    ASSERT_EQ(0,list->size());
    ASSERT_TRUE(list->IsEmpty());

    std::vector<std::string> vec;
    for( std::size_t i = 0 ; i < 1000 ; ++i ) {
      std::string str(RandStr(RandRange(128,10240)));
      vec.push_back(str);
      Handle<String> gcstr(String::New(&gc,str));
      list->Push(&gc,Value(gcstr));
      ASSERT_TRUE(*gcstr == str);
    }

    for( std::size_t i = 0 ; i < 1000 ; ++i ) {
      ASSERT_TRUE(*(list->Index(i).GetString()) == vec[i]);
    }
  }

  /* ------------------------------------
   * Empty List
   * ----------------------------------*/
  {
    Handle<List> list(List::New(&gc));
    ASSERT_EQ(0,list->size());
    ASSERT_TRUE(list->IsEmpty());

    /* Start growing */
    for( std::size_t i = 0 ; i < 1024 ; ++i ) {
      list->Push(&gc,Value(static_cast<int>(i)));
    }

    ASSERT_EQ(1024,list->size());
    ASSERT_FALSE(list->IsEmpty());
  }
}

bool ThrowDice( double probability ) {
  std::random_device device;
  std::default_random_engine el(device());
  std::uniform_int_distribution<std::uint64_t>
    dist(1,std::numeric_limits<std::uint64_t>::max());
  std::uint64_t r = dist(el);
  std::uint64_t threshold = probability *
    std::numeric_limits<std::uint64_t>::max();
  return r < threshold;
}

struct Entry {
  std::string key;
  Value value;
  Entry( const std::string& k , const Value& v ):
    key(k),
    value(v)
  {}
};

TEST(Map,Map) {
  GC gc(NULL);
  {
    Handle<Map> map(Map::New(&gc,1024));
    ASSERT_EQ(1024,map->capacity());
    ASSERT_EQ(0,map->size());
    ASSERT_EQ(0,map->slot_size());
    ASSERT_FALSE(map->NeedRehash());
    ASSERT_TRUE(map->IsEmpty());

    std::vector<Entry> vec;
    for( std::size_t i = 0 ; i < 1024 ; ++i ) {
      std::string key(RandStr(RandRange(2,1024)));
      Value v;
      while(map->Get(key,&v)) {
        key = RandStr(RandRange(2,1024));
      }
      vec.push_back(Entry(key,Value(static_cast<int>(i))));
      ASSERT_TRUE(map->Set(&gc,key,Value(static_cast<int>(i))));
    }
    ASSERT_EQ(1024,map->size());

    for( auto &e : vec ) {
      Value v;
      ASSERT_TRUE(map->Get(e.key,&v))<<e.key<<std::endl;
      ASSERT_TRUE(v.IsReal());
      ASSERT_EQ(v.GetReal() , e.value.GetReal());
    }

    for( std::size_t i = 0 ; i < 1024 ; ++i ) {
      std::size_t index = RandRange(0,1024);
      Value v;
      const Entry& e = vec[index];
      ASSERT_TRUE(map->Get(e.key,&v));
      ASSERT_EQ(v.GetReal(),e.value.GetReal());
    }
  }

  {
    /* ---------------------------------------------
     * Deletiong test                              |
     * -------------------------------------------*/

    Handle<Map> map(Map::New(&gc,1024));
    std::vector<Entry> vec;
    for( std::size_t i = 0 ; i < 1024 ; ++i ) {
      std::string key(RandStr(RandRange(2,1024)));
      Value v;
      while(map->Get(key,&v)) {
        key = RandStr(RandRange(2,1024));
      }
      Handle<String> value(String::New(&gc,
            RandStr(RandRange(2,1024))));
      vec.push_back(Entry(key,Value(value)));
      ASSERT_TRUE(map->Set(&gc,key,vec.back().value));
    }
    ASSERT_EQ(1024,map->size());

    auto end = (std::remove_if(vec.begin(),
                               vec.end(),
                               [&map](const Entry& e) {
          if(ThrowDice(0.9)) {
            return false;
          } else {
            map->Delete(e.key);
            return true;
          }
        }));
    vec.erase(end,vec.end());

    for( auto &e : vec ) {
      Value v;
      ASSERT_TRUE(map->Get(e.key,&v));
      ASSERT_TRUE(v.IsString());
      ASSERT_TRUE(*v.GetString() == *e.value.GetString());
    }
  }

  {
    Handle<Map> map(Map::New(&gc,8));
    std::vector<Entry> vec;
    for( std::size_t i = 0 ; i < 7 ; ++i ) {
      std::string key(RandStr(RandRange(1024,2048)));
      Value v;
      while(map->Get(key,&v)) {
        key = RandStr(RandRange(1024,2048));
      }
      Handle<String> value(String::New(&gc,
            RandStr(RandRange(1024,2048))));
      vec.push_back(Entry(key,Value(value)));
      ASSERT_TRUE(map->Set(&gc,key,vec.back().value));
    }

    for( auto &e : vec ) {
      ASSERT_FALSE(map->Set(&gc,e.key,Value(0)));
    }

    for( auto &e : vec ) {
      ASSERT_TRUE(map->Update(&gc,e.key,Value(1)));
    }

    for( auto& e : vec ) {
      (map->Put(&gc,e.key,Value(1)));
    }

    for( auto& e : vec ) {
      Value v;
      ASSERT_TRUE(map->Get(e.key,&v));
      ASSERT_EQ(v.GetReal(),1);
    }
  }

  /* --------------------------------------
   * empty map
   * -------------------------------------*/
  {
    Handle<Map> map(Map::New(&gc));
    ASSERT_EQ(0,map->capacity());
    ASSERT_EQ(0,map->size());
    ASSERT_EQ(0,map->slot_size());
    ASSERT_TRUE(map->NeedRehash());
    ASSERT_TRUE(map->IsEmpty());
    Value v;

    ASSERT_TRUE(!map->Get(String::New(&gc),&v));
    ASSERT_TRUE(!map->Get("abcd",&v));
    ASSERT_TRUE(!map->Update(&gc,"abcd",Value(2)));
    ASSERT_TRUE(!map->Delete("abcd"));
  }
}

TEST(Object,Object) {
  GC gc(NULL);
  {
    Handle<Object> object(Object::New(&gc));

    std::vector<Entry> vec;
    for( std::size_t i = 0 ; i < 8 ; ++i ) {
      Value v;
      std::string str(RandStr(RandRange(2,1024)));
      while(object->Get(str,&v)) {
        str = RandStr(RandRange(2,1024));
      }
      vec.push_back(Entry(str,Value(static_cast<int>(i))));
      ASSERT_TRUE(object->Set(&gc,vec.back().key,vec.back().value));
    }

    for( auto &e : vec ) {
      Value v;
      ASSERT_TRUE(object->Get(e.key,&v));
      ASSERT_EQ(v.GetReal(),e.value.GetReal());
    }
  }

  {
    Handle<Object> object(Object::New(&gc,8));
    ASSERT_EQ(0,object->size());
    ASSERT_EQ(8,object->capacity());
    ASSERT_TRUE(object->IsEmpty());

    std::vector<Entry> vec;

    for( std::size_t i = 0 ; i < 1024; ++i ) {
      std::string key(RandStr(RandRange(2,1024)));
      vec.push_back(Entry(key,Value(static_cast<int>(i))));
      ASSERT_TRUE(object->Set(&gc,key,Value(static_cast<int>(i))));
    }

    for( auto &e : vec ) {
      Value v;
      ASSERT_TRUE(object->Get(e.key,&v));
      ASSERT_EQ(v.GetReal(),e.value.GetReal());
    }
  }
}

} // namespace lavascript

int main( int argc, char* argv[] ) {
  ::lavascript::InitTrace("-");
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}
