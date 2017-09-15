#include <src/gc.h>
#include <src/trace.h>
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

void* Ptr( std::uintptr_t value ) {
  return reinterpret_cast<void*>(value);
}

bool ThrowDice( double probability ) {
  std::random_device device;
  std::default_random_engine el(device());
  std::uniform_int_distribution<std::uint64_t>
    dist(1,std::numeric_limits<std::uint64_t>::max());
  std::uint64_t r = dist(el);
  std::uint64_t threshold = probability * std::numeric_limits<std::uint64_t>::max();
  return r < threshold;
}


TEST(GC,GCRefPool) {
  /**
   * The following test is kind of slow , but it simulates some random deletiong
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
        std::vector<void**>::iterator ritr = std::find(ptr_set.begin(),ptr_set.end(),pptr);
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

} // namespace gc
} // namespace lavascript

int main( int argc, char* argv[] ) {
  ::lavascript::InitTrace("-");
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}
