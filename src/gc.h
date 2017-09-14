#ifndef GC_H_
#define GC_H_

#include <algorithm>

#include "common.h"
#include "heap-object-header.h"

#include "core/util.h"
#include "core/trace.h"
#include "core/free-list.h"

namespace lavascript {

class Context;

/**
 * Heap allocator
 *
 * Used to allocate chunk memory by Heap
 */

class HeapAllocator {
 public:
  virtual void* Malloc( size_t size ) = 0;
  virtual void  Free  ( void* ) = 0;
  virtual ~HeapAllocator() {}
};


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
    HeapObject** object;  // Actual references
  };
 public:
  // Size of all active/alive GCRef reference
  std::size_t size() const { return free_list_.size(); }

  // Create a new Ref and add it internally to the GCRefPool
  inline HeapObject** AddRef();

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
    inline HeapObject** heap_object() const;

    // Move to next available slot , if the next slot is available
    // then return true ; otherwise return false
    inline bool Move() const;

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
  core::FreeList<Ref> free_list_;

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
    inline void* start() const;
    inline void* available() const;
    inline void* Bump( std::size_t size );
    inline void SetEndOfBlock( void* header , bool flag );
  };

 public:
  // Initialize the Heap object via Context object
  inline Heap( std::size_t , double , std::size_t , std::size_t , HeapAllocator* );
  ~Heap();

  std::size_t threshold() const { return threshold_; }
  void set_threshold( std::size_t threshold ) { threshold_ = threshold; }

  std::size_t factor() const { return factor_; }
  void set_factor( std::size_t factor ) { factor_ = factor; }

  std::size_t alive_size() const { return alive_size_; }
  std::size_t allocated_bytes() const { return allocated_bytes_; }
  std::size_t chunk_size() const { return chunk_size_; }
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

  inline
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
  inline
  void* RawCopyObject( const void* ptr , std::size_t raw_size );

  // This API is same as RawCopyObject expect it checks for whether we have enough spaces for
  // copy operation.
  inline
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
    inline bool Move() const;

    // Dereference an object from current position
    inline HeapObject* heap_object() const;

    // Get the HeapObjectHeader from the current position
    inline HeapObjectHeader heap_object_header() const;

    // Set the HeapObjectHeader to the current position
    inline void set_heap_object_header( const HeapObjectHeader& header ) const;

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
  inline bool RefillChunk( size_t size );

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

inline bool GCRefPool::Iterator::Move() const {
  previous_ = current_;
  current_ = curren_->next;
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
GCRefPool::Iterator::Iterator( const GCRefPool::Iterator& that ) {
  if(this != &that) {
    previous_ = that.previous_;
    current_  = that.current_ ;
  }
  return *this;
}

inline Heap::Heap( size_t threshold ,
                   double factor ,
                   size_t chunk_capacity ,
                   size_t init_size ,
                   HeapAllocator* allocator ):
  threshold_(threshold),
  factor_(factor),
  alive_size_(0),
  chunk_size_(0),
  allocated_bytes_(0),
  total_bytes_(0),
  chunk_capacity_(chunk_capacity),
  chunk_current_(NULL),
  fall_back_(NULL),
  allocator_(allocator)
{
  RefillChunk(init_size);
}

inline void Heap::Chunk::SetEndOfBlock( void* header , bool flag ) {
  HeapObjectHeader hdr(*reinterpret_cast<std::uint64_t*>(header));
  if(flag) hdr.set_end_of_block();
  else hdr.set_not_end_of_block();
  (*reinterpret_cast<std::uint64_t*>(ret)) = hdr.raw();
}

inline void* Heap::Chunk::start() const {
  return static_cast<void*>(
      reinterpret_cast<char*>(this) + sizeof(Chunk));
}

inline void* Heap::Chunk::available() const {
  return static_cast<void*>(
      static_cast<char*>(this) + sizeof(Chunk) + bytes_used);
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
  current_chunk_(chunk),
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

inline bool Heap::Iterator::Move() {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(HasNext());
#endif // LAVASCRIPT_CHECK_OBJECTS
  do {
    HeapObjectHeader hdr(
        static_cast<char*>(current_chunk_->start()) + current_cursor_ );
    if(hdr.IsEndOfChunk()) {
      current_chunk_ = current_chunk_->next();
      current_cursor_ = 0;
      continue;
    } else {
      current_cursor_ += hdr.total_size();
      return true;
    }
  } while(current_chunk_);
  return false;
}

inline HeapObject* Heap::Iterator::heap_object() const {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(HasNext());
#endif // LAVASCRIPT_CHECK_OBJECTS
  return reinterpret_cast<HeapObject*>(
      static_cast<char*>(current_chunk_->start()) + current_cursor_ +
                                                    HeapObjectHeader::kHeapObjectHeaderSize);
}

inline HeapObjectHeader Heap::Iterator::heap_object_header() const {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(HasNext());
#endif // LAVASCRIPT_CHECK_OBJECTS
  return HeapObjectHeader(static_cast<char*>(current_chunk_->start()) + current_cursor_);
}

inline void Heap::Iterator::set_heap_object_header(
    const HeapObjectHeader& hdr ) const {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(HasNext());
#endif // LAVASCRIPT_CHECK_OBJECTS
  *reinterpret_cast<std::uint64_t*>(
      static_cast<char*>(current_chunk_->start()) + current_cursor) = hdr.raw();
}

inline void Heap::Swap( Heap* that ) {
  std::swap( threshold_ , that->threshold_ );
  std::swap( factor_    , that->factor_    );
  std::swap( alive_size_, that->alive_size_);
  std::swap( chunk_size_, that->chunk_size_);
  std::swap( allocated_bytes_ , that->allocated_bytes_ );
  std::swap( total_bytes_ , that->total_bytes_ );
  std::swap( chunk_capacity_ , that->chunk_capacity_ );
  std::swap( chunk_current_ , that->chunk_current_ );
  std::swap( fail_back_ , that->fail_back_ );
  std::swap( allocator_ , that->allocator_ );
}

inline bool Heap::RefillChunk( size_t size ) {
  size_t raw_size;
  if( chunk_capacity_ < size ) {
    raw_size = size;
  } else {
    raw_size = chunk_capacity_;
  }

  void* new_buf = allocator ? allocator->Malloc( raw_size + sizeof(Chunk) ) :
                              ::malloc( raw_size + sizeof(Chunk) );
  if(!new_buf) return false;

  Chunk* ck = reinterpret_cast<Chunk*>(new_buf);
  ck->size_in_bytes = raw_size;
  ck->size_in_objects= 0;
  ck->bytes_used = 0;
  ck->previous = NULL;
  ck->next = chunk_current_;
  chunk_current_ = ck;
  chunk_size_++;
  total_bytes_+= raw_size;
  return true;
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

inline void* Heap::Grab( size_t object_size , ValueType type, GCState gc_state ,
                                                              bool is_long_str ) {
  object_size = core::Align(object_size,kAlignment);
  std::size_t size = object_size + kHeapObjectHeaderSize;

  // Try to use slow FindInChunk when we trigger it because of we use too much
  // memory now
  size_t find_in_chunk_trigger = threshold_ * factor_;
  if( find_in_chunk_trigger < allocated_bytes_ ) {
    void* ret = FindInChunk(size);
    if(ret) return ret;
  }

  if(chunk_current_->bytes_left() < size) {
    if(!RefillChunk(size)) return NULL;
  }

  // Now try to allocate it from the newly created chunk
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(chunk_current_->bytes_left() >= size);
  lava_verify( is_long_str ? type == VALUE_STRING : true );
#endif // LAVASCRIPT_CHECK_OBJECTS

  allocated_bytes_ += size;
  alive_size_++;

  return SetHeapObjectHeader(chunk_current->Bump(size),object_size,type,gc_state,
                                                                        is_long_str);
}

inline void* Heap::RawCopyObject( const void* ptr , std::size_t length ) {
  void* ret = chunk_current_->Bump(length);
  memcpy(ret,ptr,length);

  ++allocated_bytes_ += length;
  alive_size_++;
  return ret;
}

inline void* Heap::CopyObject( const void* ptr , std::size_t length ) {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify( core::Align(length,kAlignment) == length );
#endif // LAVASCRIPT_CHECK_OBJECTS

  size_t find_in_chunk_trigger = threshold_ * factor_;
  void* buf = NULL;
  if(find_in_chunk_trigger < allocated_bytes_) {
    buf = FindInChunk(length);
  }
  if(!buf) {
    if(chunk_current_->bytes_left() < length) {
      if(!RefillChunk(size)) return NULL;
    }
    ret = chunk_current_->Bump(length);
  }
  memcpy(ret,ptr,length);
  allocated_bytes_ += length;
  alive_size_++;
  return ret;
}

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
  std::size_t ref_size() const { return ref_pool.size(); }
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
  T** New( const HeapObjectHeader& hdr , ARGS ...args );

  /*
   * This API call allow user to provide a holder and reuse that holder
   * to store the newly created object. Mostly we use this API to implement
   * Extend/Rehash for Slice and Map object
   */
  template< typename T , typename ... ARGS >
  T** New( T** holder , const HeapObjectHeader& hdr , ARGS ...args );

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
  String** NewString();

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

  void Mark( MarkResult* );

  /**
   * API to do the Swap phase which basically move all alive object from old
   * heap to new heap and patch all the reference to the new heap
   */
  void Swap( std::size_t new_heap_size );

 private:
  std::size_t cycle_;                                 // How many GC cycles are performed
  std::size_t minimum_gap_;                           // Minimum gap
  std::size_t previous_alive_size_;                   // Previous marks active size
  std::size_t previous_dead_size_ ;                   // Previous dead size
  double factor_;                                     // Tunable factor
  gc::Heap heap_;                                     // Current active heap
  gc::GCRefPool ref_pool_;                            // Ref pool
  Context* context_;                                  // Context object
  HeapAllocator* allocator_;                          // Allocator
};

template< typename T , typename ...ARGS >
T** GC::New( const HeapObjectHeader& hdr , ARGS ...args ) {
  return New( reinterpret_cast<T**>(ref_pool_.Grab()) , hdr , args... );
}

template< typename T , typename ...ARGS >
T** GC::New( T** holder , const HeapObjectHeader& hdr , ARGS ...args ) {
  TryGC(); // Try to perform GC if we need to
  *holder = core::Construct( heap_.Grab( sizeof(T),
                                         hdr.type(),
                                         hdr.gc_state(),
                                         hdr.IsLongString() ) , args... );
  return holder;
}

} // namespace lavascript
#endif // GC_H_