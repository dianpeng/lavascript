#include <src/gc.h>
#include <src/trace.h>
#include <src/heap-object-header.h>
#include <src/macro.h>
#include <src/objects.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <vector>
#include <random>
#include <algorithm>

#ifndef max
#undef max
#endif

#ifndef min
#undef min
#endif

namespace lavascript {
namespace gc {

std::string Str( const SSO& sso ) {
  return std::string((const char*)sso.data(),sso.size());
}

void* Ptr( std::uintptr_t value ) {
  return reinterpret_cast<void*>(value);
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

std::size_t RandSize() {
  std::random_device device;
  std::default_random_engine el(device());
  std::uniform_int_distribution<std::uint64_t>
    dist(1,std::numeric_limits<std::uint64_t>::max());
  std::uint64_t r = dist(el);

  return r % 1024;
}

TEST(GC,GCRefPool) {
  /**
   * The following test is kind of slow ,but it simulates some random deletiong
   * interleaved with random allocation
   */
  {
    GCRefPool pool(1,1,NULL);
    /**
     * Just create tons of reference and set them to certain types of pointer
     * and then check there to be correct or not
     */

    std::vector<void**> ptr_set;
    for( std::size_t i = 0 ; i < 10000 ; ++i ) {
      void* p = Ptr(i);
      void** slot = reinterpret_cast<void**>(pool.Grab());
      *slot = p;
      ptr_set.push_back(slot);
    }

    ASSERT_TRUE( pool.size() == 10000 );

    /**
     * Now check all the pointer are valid pointer value
     */
    std::size_t count = 0;
    for( auto &e : ptr_set ) {
      void* p = Ptr(count);
      ASSERT_TRUE( *e == p );
      ++count;
    }


    std::vector<void**> left_ptr_set;

    GCRefPool::Iterator itr( pool.GetIterator() );

    count = 0;
    while(itr.HasNext()) {
      void** pptr = reinterpret_cast<void**>(itr.heap_object());
      if(ThrowDice(0.9)) {
        ASSERT_TRUE( std::find(ptr_set.begin(),ptr_set.end(),pptr) != ptr_set.end() );
        itr.Remove(&pool);
        ++count;
      } else {
        std::vector<void**>::iterator ritr =
          std::find(ptr_set.begin(),ptr_set.end(),pptr);
        ASSERT_FALSE( ritr == ptr_set.end() );
        ptr_set.erase(ritr);
        itr.Move();
      }
    }
    ASSERT_EQ(10000-count,pool.size());

    {
      std::vector<void**> new_set;
      for( std::size_t i = 0 ; i < 10000 ; ++i ) {
        void** pptr = reinterpret_cast<void**>(pool.Grab());
        *pptr = Ptr(i);
        new_set.push_back(pptr);
      }
      ASSERT_EQ(20000-count,pool.size());
      count = 0;
      for( auto &e : new_set ) {
        ASSERT_TRUE(*e == Ptr(count));
        ++count;
      }
    }

    for( auto& e : left_ptr_set ) {
      ASSERT_TRUE( std::find(ptr_set.begin(),ptr_set.end(),e) != ptr_set.end() );
    }
  }
}

HeapObjectHeader GetHeader( void* data ) {
  return HeapObjectHeader( reinterpret_cast<char*>(data) - HeapObjectHeader::kHeapObjectHeaderSize );
}

TEST(Heap,HeaderCheck) {
  {
    Heap heap(1,2,NULL);
    std::vector<std::uint64_t*> ptr_vec;
    for( std::size_t i = 0 ; i < 10000 ; ++i ) {
      std::uint64_t* ptr = reinterpret_cast<std::uint64_t*>(
          heap.Grab( sizeof(std::uint64_t) , TYPE_STRING ));
      *ptr = i;
      ptr_vec.push_back(ptr);
      ASSERT_TRUE( GetHeader(ptr).IsString() );
      ASSERT_TRUE( GetHeader(ptr).size() == 8 );
      ASSERT_TRUE( GetHeader(ptr).IsGCWhite() );
      ASSERT_FALSE( GetHeader(ptr).IsLongString() );

      // FXIME:: It may be proper to put this check here
      ASSERT_TRUE( GetHeader(ptr).IsEndOfChunk() );
    }
    ASSERT_EQ(10000,heap.alive_size());
    ASSERT_EQ(10001,heap.chunk_size());

    std::size_t count = 0;
    for( auto &e : ptr_vec ) {
      ASSERT_EQ(*e,count);
      ++count;
      ASSERT_TRUE( GetHeader(e).IsString() );
      ASSERT_TRUE( GetHeader(e).size() == 8);
      ASSERT_TRUE( GetHeader(e).gc_state() == GC_WHITE );
      ASSERT_FALSE( GetHeader(e).IsLongString() );
      ASSERT_TRUE( GetHeader(e).IsEndOfChunk() );
    }
    ASSERT_EQ(10000,count);

    Heap::Iterator itr(heap.GetIterator());
    count = 10000-1;
    while( itr.HasNext() ) {
      ASSERT_EQ(count,*reinterpret_cast<std::uint64_t*>(itr.heap_object()));
      ASSERT_TRUE( itr.hoh().IsString() );
      ASSERT_TRUE( itr.hoh().size() == 8 );
      ASSERT_TRUE( itr.hoh().gc_state() == GC_WHITE );
      ASSERT_FALSE( itr.hoh().IsLongString() );
      ASSERT_TRUE( itr.hoh().IsEndOfChunk() );
      --count;
      itr.Move();
    }
  }

  {
    Heap heap(1,1024,NULL);
    std::vector<std::uint64_t*> ptr_vec;
    for( std::size_t i = 0 ; i < 10000 ; ++i ) {
      std::uint64_t* ptr = reinterpret_cast<std::uint64_t*>(
          heap.Grab( sizeof(std::uint64_t) , TYPE_STRING ));
      *ptr = i;
      ptr_vec.push_back(ptr);
      ASSERT_TRUE( GetHeader(ptr).IsString() );
      ASSERT_TRUE( GetHeader(ptr).size() == 8 );
      ASSERT_TRUE( GetHeader(ptr).IsGCWhite() );
      ASSERT_FALSE( GetHeader(ptr).IsLongString() );

      // FXIME:: It may be proper to put this check here
      ASSERT_TRUE( GetHeader(ptr).IsEndOfChunk() );
    }
    ASSERT_EQ(10000,heap.alive_size());

    std::size_t count = 0;
    for( auto &e : ptr_vec ) {
      ASSERT_EQ(*e,count);
      ++count;
      ASSERT_TRUE( GetHeader(e).IsString() );
      ASSERT_TRUE( GetHeader(e).size() == 8);
      ASSERT_TRUE( GetHeader(e).gc_state() == GC_WHITE );
      ASSERT_FALSE( GetHeader(e).IsLongString() );
    }
    ASSERT_EQ(10000,count);

    Heap::Iterator itr(heap.GetIterator());
    count = 0;
    while( itr.HasNext() ) {
      ASSERT_TRUE( itr.hoh().IsString() );
      ASSERT_TRUE( itr.hoh().size() == 8 );
      ASSERT_TRUE( itr.hoh().gc_state() == GC_WHITE );
      ASSERT_FALSE( itr.hoh().IsLongString() );
      ++count;
      itr.Move();
    }
  }

  // -----------------------------------------------
  //
  // Swapping the Heap
  //
  // -----------------------------------------------
  {
    Heap heap(1,1024,NULL);
    std::vector<void*> ptr_vec;
    for( std::size_t i = 0 ; i < 10000 ; ++i ) {
      void* ret = heap.Grab( sizeof(RandSize()) , TYPE_STRING );
      ptr_vec.push_back(ret);
      ASSERT_TRUE(ret);
    }

    const std::size_t alive_size = heap.alive_size();
    const std::size_t allocated_bytes = heap.allocated_bytes();
    const std::size_t chunk_size = heap.chunk_size();
    const std::size_t chunk_capacity = heap.chunk_capacity();
    const std::size_t total_bytes = heap.total_bytes();

    Heap new_heap(1,1,NULL);

    new_heap.Swap(&heap);

    ASSERT_EQ( alive_size , new_heap.alive_size() );
    ASSERT_EQ( allocated_bytes , new_heap.allocated_bytes() );
    ASSERT_EQ( chunk_size , new_heap.chunk_size() );
    ASSERT_EQ( chunk_capacity , new_heap.chunk_capacity() );
    ASSERT_EQ( total_bytes , new_heap.total_bytes() );

    Heap::Iterator itr(new_heap.GetIterator());

    while(itr.HasNext()) {
      void* ptr = reinterpret_cast<void*>(itr.heap_object());
      ASSERT_TRUE( std::find( ptr_vec.begin() ,
                              ptr_vec.end() ,
                              ptr ) != ptr_vec.end() );
      itr.Move();
    }
  }
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

TEST(SSOPool,Basic) {
  {
    SSOPool sso_pool(2,1,1,NULL);
    {
      std::vector<std::string> str_vec;
      for( std::size_t i = 0 ; i < 10000 ; ++i ) {
        std::string str(RandStr(RandRange(2,kSSOMaxSize)));
        str_vec.push_back(str);
        SSO* sso = sso_pool.Get(str.c_str(),str.size());
        ASSERT_TRUE(Str(*sso) == str);
      }
      // Do not assume RandStr will generate string that is
      // definitly unique since if we put test like this ,
      // it will trigger flaky.
      for( auto &e : str_vec ) {
        std::size_t before = sso_pool.size();
        SSO* sso = sso_pool.Get( e.c_str() , e.size() );
        ASSERT_TRUE( Str(*sso) == e );
        ASSERT_EQ(before,sso_pool.size());
      }
    }
  }
}

} // namespace gc
} // namespace lavascript

int main( int argc, char* argv[] ) {
  ::lavascript::InitTrace("-");
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}
