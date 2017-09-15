#ifndef GC_H_
#define GC_H_

#include <algorithm>
#include <vector>
#include <string>
#include <cstring>

#include "common.h"
#include "heap-object-header.h"
#include "heap-allocator.h"

#include "util.h"
#include "trace.h"
#include "free-list.h"
#include "bump-allocator.h"

namespace lavascript {

class Context;
class SSO;

/**
 * GC implemention for lavascript. This GC implementation is a stop-the-world
 * with mark and sweep style. We do compaction as well. Currently implementation,
 * for simplicity , we just do everything in one cycle. In the future , we will
 * optimize it to be generation based
 */

namespace gc {
/**
 * GCRefPool is a pool to track *ALL* the places that we store a heap pointer,
 * managed pointer. It is basically just a free list wrapper . We will walk
 * through the GCRefPool for compaction purpose
 */

class GCRefPool final {
  struct Ref {
    Ref* next;            // Pointer to the next Ref in RefList
    HeapObject* object;  // Actual references
  };
 public:
  // Size of all active/alive GCRef reference
  std::size_t size() const { return free_list_.size(); }

  // Create a new Ref and add it internally to the GCRefPool
  HeapObject** Grab() {
    Ref* new_ref = free_list_.Grab();
    new_ref->next = front_;
    front_ = new_ref;
    return &(new_ref->object);
  }

  // The deletion of GCRefPool happens when we want to iterate
  // through the whole GCRefPool and since it is a single linked
  // list and we cannot do move , typical trick for single linked
  // list deletion , we need to always keep track its *previous*
  // node.

  class Iterator {
   public:
     // Whether we has next available slot inside of the GCRefPool
    inline bool HasNext() const;

    // Get the current HeapObject in the current cursor/iterator
    inline HeapObject** heap_object();

    // Move to next available slot , if the next slot is available
    // then return true ; otherwise return false
    bool Move();

    // Remove the current iterator and move to next slot, if next
    // slot is available then return true ; otherwise return false
    inline bool Remove( GCRefPool* );

    inline Iterator( Ref* c );
    inline Iterator( const Iterator& );
    inline Iterator& operator = ( const Iterator& );
   private:
    Ref* previous_;
    Ref* current_;
  };

  // Get the iterator for this GCRefPool
  Iterator GetIterator() const { return Iterator(front_); }

 private:
  // Delete the target from the GCRefPool
  inline Ref* Delete( Ref* prev , Ref* target );

  // Front of the Ref object
  Ref* front_;

  // Free list pool for manipulating the GCRefPool object
  FreeList<Ref> free_list_;

  friend class Iterator;

  LAVA_DISALLOW_COPY_AND_ASSIGN(GCRefPool);
};


/**
 * A heap is the main managed places that stores all the heap based
 * objects. A heap contains several piece of *Chunk* . A Chunk is a
 * single allocation from the underlying allocation system. Currently
 * we directly use malloc to grab memory from underlying memory API.
 *
 *
 * A Chunk will contain all immutable GC object and each Chunk is linked
 * together to form a Heap object. Heap object is *owned* by GC object.
 *
 * During compaction phase , a new Heap object will be created and GC
 * will move the old object from old Heap to new Heap object.
 *
 * To walk a heap, an Iterator is provided for walking every objects
 * stay on heap
 */

class Heap final {
  static const std::size_t kAlignment = 8;

  // The actual owned memory of Chunk are starting from this + sizeof(Chunk).
  struct Chunk {
    Chunk* next;                   // Next chunk
    std::size_t size_in_bytes;     // What's the size of this chunk in bytes, exclude header size
    std::size_t size_in_objects;   // What's the number of objects allocated on this chunk
    std::size_t bytes_used;        // How many bytes are used
    void* previous;                // Where is the previous object's heap object header

    std::size_t bytes_left() const { return size_in_bytes - bytes_used; }
    inline void* start();
    inline void* available();
    inline void* Bump( std::size_t size );
    inline void SetEndOfBlock( void* header , bool flag );
  };

 public:
  // Initialize the Heap object via Context object
  Heap( std::size_t , double , std::size_t , std::size_t , HeapAllocator* );
  ~Heap();

  std::size_t threshold() const { return threshold_; }
  void set_threshold( std::size_t threshold ) { threshold_ = threshold; }

  std::size_t factor() const { return factor_; }
  void set_factor( std::size_t factor ) { factor_ = factor; }

  std::size_t alive_size() const { return alive_size_; }
  std::size_t allocated_bytes() const { return allocated_bytes_; }
  std::size_t chunk_size() const { return chunk_size_; }
  std::size_t chunk_capacity() const { return chunk_capacity_; }
  std::size_t total_bytes() const { return total_bytes_; }

 public: // Allocation functions

  /**
   * This is the main function to grab a chunk of memory from Heap.
   * Currently the heap implementation is really simple since it only implements a
   * bump allocator. It tries to allocate memory from the current Chunk list and if
   * it fails then it just create a new Chunk and then create the object from it.
   *
   * It will return an *OBJECT*'s starting address , not the address point to its
   * heap object header. If it cannot perform the allocation due to OOM , it returns
   * NULL.
   *
   */
  void* Grab( std::size_t object_size ,             // Size of object , not include header
              ValueType type ,                      // Type of the object
              GCState gc_state = GC_WHITE,          // State of the GC
              bool is_long_str = false);            // Whether it is a long string

  // This API is used to during compaction phase. It allow user to copy an raw memory
  // buffer pointer at ptr with size of (raw_size) to this Heap object. This API will
  // not check whether the ptr has a valid header and caller should ensure that the ptr
  // points at the header not the object
  //
  // Also the CopyObject will assume that we have enough spaces for such operations. Since it
  // is used in compaction phase , at that point we should already know how much memory
  // we need at least for the new heap so no need to verify that whether we have enough
  // spaces or not.
  //
  // It will return address where points to the HeapObjectHeader basically the start of a certain
  // allocation
  void* RawCopyObject( const void* ptr , std::size_t raw_size );

  // This API is same as RawCopyObject expect it checks for whether we have enough spaces for
  // copy operation.
  void* CopyObject   ( const void* ptr , std::size_t raw_size );

  // Swap another heap with *this* heap
  void Swap( Heap* );
 public:

  // Iterator for walking through the whole Heap object's internal Chunk
  class Iterator {
   public:
    inline Iterator( Chunk* chunk );
    inline Iterator();
    inline Iterator( const Iterator& that );
    inline Iterator& operator = ( const Iterator& );

    // Check if we have next available object on the Heap
    inline bool HasNext() const;

    // Move the iterator to next
    bool Move();

    // Dereference an object from current position
    inline HeapObject* heap_object() const;

    // Get the HeapObjectHeader from the current position
    inline HeapObjectHeader hoh() const;

    // Set the HeapObjectHeader to the current position
    inline void set_hoh( const HeapObjectHeader& header ) const;

   private:
    Chunk* current_chunk_;                   // Current chunk
    std::size_t current_cursor_;             // Current cursor in bytes
    bool end_of_chunk_;                      // Flag to indicate EndOfChunk
  };

  // Get the Iterator for the Heap
  Iterator GetIterator() const { return Iterator(chunk_current_); }

 public: // DEBUG related code , may not suitable for production
  enum {
    HEAP_DUMP_NORMAL = 0,
    HEAP_DUMP_VERBOSE= 1,
    HEAP_DUMP_CRAZY  = 2        // Very expansive since it dumps every objects at all
                                // and store them into a dump file
  };

  // Dump the heap into a filename or use normal lava_info to dump
  void HeapDump( int verbose , const char* filename = NULL ); // Debug

 private:
  bool RefillChunk( size_t size );

  // Find the raw_bytes_length chunk from Walking the whole list of chunk
  // this is a slow path and it will only triggers when we 1) bypass threshold
  // 2) cannot get memory (OOM)
  void* FindInChunk( std::size_t raw_bytes_length );

  inline void* SetHeapObjectHeader( void* , std::size_t size , ValueType , GCState , bool );

 private:
  std::size_t threshold_;                // The single threshold to tell whether we should fail the
                                         // Grab operation

  double factor_;                        // If we saturated this much of memory then we prefer walking
                                         // the heap

  std::size_t alive_size_;               // Total allocated object's size

  std::size_t chunk_size_;               // Total memory chunk's size

  std::size_t allocated_bytes_;          // Totall allocated memory in bytes

  std::size_t total_bytes_;              // Total bytes that is used for the chunk list

  std::size_t chunk_capacity_;           // The capacity of the Chunk allocation. The capacity will not
                                         // change throughout the Heap

  Chunk* chunk_current_;                 // Current chunk

  Chunk* fall_back_;                     // Fallback allocation starting chunk, this will perform a
                                         // heap walk until it cannot find any chunk

  HeapAllocator* allocator_;             // Allocator

  LAVA_DISALLOW_COPY_AND_ASSIGN(Heap);
};

/**
 * SSO pool is a pool for holding all the SSO strings.
 *
 * A SSO stands for Small String Object and will be optimized in different phases.
 * A SSO will not be GCed which leads to potential leak of memory , but currently
 * that's our design. A SSO is managed via a hash table internally with hash value
 * embedded inside of the SSO string. This hash value is critical since it may help
 * us to bypass the normal Hash process for getting object from Object/Map without
 * the need of *hidden class* implementation. This idea is brought from LuaJIT.
 *
 * SSO is also de-duplicated , so a comparison between SSO only compares the pointer,
 * this makes SSO super good for hash key since the lookup process for hash is really
 * performant.
 */

class SSOPool {
  static const std::size_t kDefaultSSOPoolSlotSize = 1024; // 1KB slots
  struct Entry {
    Entry* next;
    SSO* sso;
    Entry():next(NULL),sso(NULL){}
  };
 public:
  inline SSOPool( size_t init_capacity ,
                  size_t maximum_size  ,
                  HeapAllocator* allocator = NULL );
 public:
  SSO* Get( const char* , std::size_t );
  SSO* Get( const void* data , std::size_t size ) {
    return Get(static_cast<const char*>(data),size);
  }
  SSO* Get( const char* str ) { return Get(str,std::strlen(str)); }
  SSO* Get( const std::string& str ) { return Get(str.c_str(),str.size()); }

  std::size_t capacity() const { return entry_.size(); }
  std::size_t size    () const { return size_ ; }

 private:
  void Rehash();
  Entry* FindOrInsert( std::vector<Entry>* ,
                       const char* , std::size_t , std::uint32_t hash );

  std::vector<Entry> entry_;
  std::size_t size_;
  BumpAllocator allocator_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(SSOPool);
};

/* ===========================================================================
 *
 * Inline functions
 *
 * ==========================================================================*/

inline GCRefPool::Ref* GCRefPool::Delete( Ref* prev , Ref* target ) {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify( prev->next == target );
#endif // LAVASCRIPT_CHECK_OBJECTS
  Ref* ret = target->next;
  prev->next = target->next;
  free_list_.Drop(target);
  return ret;
}

inline bool GCRefPool::Iterator::HasNext() const {
  return current_ != NULL;
}

inline HeapObject** GCRefPool::Iterator::heap_object() {
  return &(current_->object);
}

inline bool GCRefPool::Iterator::Move() {
  previous_ = current_;
  current_ = current_->next;
  return current_ != NULL;
}

inline bool GCRefPool::Iterator::Remove( GCRefPool* pool ) {
  current_ = pool->Delete( previous_ , current_ );
  return current_ != NULL;
}

inline GCRefPool::Iterator::Iterator( GCRefPool::Ref* ref ):
  previous_(ref),
  current_ (ref)
{}

inline GCRefPool::Iterator::Iterator( const Iterator& that ):
  previous_( that.previous_ ),
  current_ ( that.current_  )
{}

inline GCRefPool::Iterator&
GCRefPool::Iterator::operator = ( const GCRefPool::Iterator& that ) {
  if(this != &that) {
    previous_ = that.previous_;
    current_  = that.current_ ;
  }
  return *this;
}

inline void Heap::Chunk::SetEndOfBlock( void* header , bool flag ) {
  HeapObjectHeader hdr(*reinterpret_cast<std::uint64_t*>(header));
  if(flag) hdr.set_end_of_chunk();
  else hdr.set_not_end_of_chunk();
  (*reinterpret_cast<std::uint64_t*>(header)) = hdr.raw();
}

inline void* Heap::Chunk::start() {
  return static_cast<void*>(
      reinterpret_cast<char*>(this) + sizeof(Chunk));
}

inline void* Heap::Chunk::available() {
  return static_cast<void*>(
      static_cast<char*>(start()) + bytes_used);
}

inline void* Heap::Chunk::Bump( std::size_t size ) {
  void* ret = available();
  if(previous) {
    SetEndOfBlock(previous,false);
  }
  SetEndOfBlock(ret,true);
  previous = ret;
  bytes_used += size;
  return ret;
}

inline Heap::Iterator::Iterator( Chunk* chunk ):
  current_chunk_(chunk),
  current_cursor_(0)
{
  Move();
}

inline Heap::Iterator::Iterator():
  current_chunk_(NULL),
  current_cursor_(0)
{}

inline Heap::Iterator::Iterator( const Iterator& that ):
  current_chunk_(that.current_chunk_),
  current_cursor_(that.current_cursor_),
  end_of_chunk_(that.end_of_chunk_)
{}

inline Heap::Iterator& Heap::Iterator::operator = ( const Iterator& that ){
  if(this != &that) {
    current_chunk_ = that.current_chunk_;
    current_cursor_ = that.current_cursor_;
    end_of_chunk_ = that.end_of_chunk_;
  }
  return *this;
}

inline bool Heap::Iterator::HasNext() const {
  return current_chunk_ != NULL;
}

inline HeapObject* Heap::Iterator::heap_object() const {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(HasNext());
#endif // LAVASCRIPT_CHECK_OBJECTS
  return reinterpret_cast<HeapObject*>(
      static_cast<char*>(current_chunk_->start()) + current_cursor_ +
                                                    HeapObjectHeader::kHeapObjectHeaderSize);
}

inline HeapObjectHeader Heap::Iterator::hoh() const {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(HasNext());
#endif // LAVASCRIPT_CHECK_OBJECTS
  return HeapObjectHeader(static_cast<char*>(current_chunk_->start()) + current_cursor_);
}

inline void Heap::Iterator::set_hoh(
    const HeapObjectHeader& hdr ) const {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(HasNext());
#endif // LAVASCRIPT_CHECK_OBJECTS
  *reinterpret_cast<std::uint64_t*>(
      static_cast<char*>(current_chunk_->start()) + current_cursor_) = hdr.raw();
}

inline void* Heap::SetHeapObjectHeader( void* ptr , size_t size , ValueType type,
                                                                  GCState gc_state,
                                                                  bool is_long_str ) {
  HeapObjectHeader hdr(ptr);
  hdr.set_size(size);
  hdr.set_type(type);
  hdr.set_gc_state(gc_state);
  if(is_long_str) hdr.set_long_string();
  return static_cast<char*>(ptr) + HeapObjectHeader::kHeapObjectHeaderSize;
}

inline void* Heap::RawCopyObject( const void* ptr , std::size_t length ) {
  void* ret = chunk_current_->Bump(length);
  std::memcpy(ret,ptr,length);

  ++allocated_bytes_ += length;
  alive_size_++;
  return ret;
}

inline SSOPool::SSOPool( std::size_t init_capacity ,
                         std::size_t maximum ,
                         HeapAllocator* allocator ):
  entry_(),
  size_ (0),
  allocator_(init_capacity,maximum,allocator)
{}


} // namespace gc


/**
 *
 * GC represents the whole GC interfaces. Currently we support a simple GC algorithm.
 * We just do a mark and swap in one GC cycle. In the future we will add a generational
 * GC system.
 *
 *
 * GC wrapes most of the internal states and allow people to allocate stuff from it.
 * The typical allocation for an object must happened at two phase :
 *  1) Allocate the object on the *Heap*
 *  2) Allocate the GCRef from GCRefPool
 *
 * The GCRef traces where the pointer is stored and it is basically a HeapObject**.
 *
 */

class GC : AllStatic {
 public:
  /**
   * GC tunable argument
   * 1. Minimum Gap    , what is the minimum gap for triggering the GC
   * 2. Factor         , dynamic adjust the next gc trigger number
   */

  struct GCConfig {
    size_t minimum_gap;
    double factor;
    size_t heap_threshold;
    double heap_factor;
    size_t heap_capacity;
    HeapAllocator* allocator;
  };

  GC( const GCConfig& , Context* , HeapAllocator* allocator = NULL );

 public:
  std::size_t cycle() const { return cycle_; }
  std::size_t alive_size() const { return heap_.alive_size(); }
  std::size_t allocated_bytes() const { return heap_.allocated_bytes(); }
  std::size_t total_bytes() const { return heap_.total_bytes(); }
  std::size_t ref_size() const { return ref_pool_.size(); }
  std::size_t minimum_gap() const { return minimum_gap_; }
  std::size_t previous_alive_size() const { return previous_alive_size_; }
  std::size_t previous_dead_size()  const { return previous_dead_size_; }
  double factor() const { return factor_; }
  Context* context() const { return context_; }

 public:
  /**
   * Function to create a new object with type T , the input arguments are
   * ARGS ...args.
   *
   * The return value is a T** which represented the HeapObject reference.
   * Each factory function inside of each Type could easily wrape this into
   * a static factory function that returns a Handle<T> wrapper instead of
   * directly return the GCRef pointer
   */
  template< typename T , typename ... ARGS >
  T** New( ARGS ...args );

  /*
   * This API call allow user to provide a holder and reuse that holder
   * to store the newly created object. Mostly we use this API to implement
   * Extend/Rehash for Slice and Map object
   */
  template< typename T , typename ... ARGS >
  T** New( T** holder , ARGS ...args );

  /**
   * Specialized New for String creation since String is handled specially
   * internally.
   *
   * A String object is an empty object ( though C++ doens't allow ), it is
   * basically a union of SSO or LongString.
   *
   * Then we just reinterpret_cast this memory to String object. String object
   * is implemented with such assumption
   */
  String** NewString( const void* str , size_t len );

  /**
   * Create an empty string , though the above API can take care of this case
   * as well.
   */
  String** NewString() { return NewString( static_cast<const void*>("") , 0 ); }

  /**
   * Specialized New for Slice creation . It will properly construct the
   * slice array
   */
  Slice** NewSlice( std::size_t capacity );
  Slice** NewSlice() { return NewSlice(0); }

  /**
   * Specialized New for Map creation . It will properly construct all the
   * entry
   */
  Map** NewMap( std::size_t capacity );
  Map** NewMap() { return NewMap(0); }


 public:
  // Force a GC cycle to happen
  void ForceGC();

  // Try a GC cycle to happen
  bool TryGC();

 private: // GC related code

  /**
   * API to start the marking phase. This function will start mark from all
   * possible root nodes which is listed as following :
   *
   *   1) Stack
   *   2) Global Varible Table
   *   3) C++ LocalScope
   */
  struct MarkResult {
    std::size_t alive_size;
    std::size_t dead_size ;
    std::size_t new_heap_size;         // Total , include heap object header
    MarkResult() : alive_size(0),dead_size(0),new_heap_size(0) {}
  };

  void PhaseMark( MarkResult* );

  /**
   * API to do the Swap phase which basically move all alive object from old
   * heap to new heap and patch all the reference to the new heap
   */
  void PhaseSwap( std::size_t new_heap_size );

 private:
  std::size_t cycle_;                                 // How many GC cycles are performed
  std::size_t minimum_gap_;                           // Minimum gap
  std::size_t previous_alive_size_;                   // Previous marks active size
  std::size_t previous_dead_size_ ;                   // Previous dead size
  double factor_;                                     // Tunable factor
  gc::Heap heap_;                                     // Current active heap
  gc::GCRefPool ref_pool_;                            // Ref pool
  gc::SSOPool sso_pool_;                              // SSO pool
  Context* context_;                                  // Context object
  HeapAllocator* allocator_;                          // Allocator
};

template< typename T , typename ...ARGS >
T** GC::New( ARGS ...args ) {
  return New( reinterpret_cast<T**>(ref_pool_.Grab()) , args... );
}

template< typename T , typename ...ARGS >
T** GC::New( T** holder , ARGS ...args ) {
  TryGC(); // Try to perform GC if we need to
  *holder = ConstructFromBuffer<T>( heap_.Grab( sizeof(T),
                                                GetObjectType<T>::value,
                                                GC_WHITE,
                                                false ) , args... );
  return holder;
}

} // namespace lavascript
#endif // GC_H_
