#ifndef GC_H_
#define GC_H_

#include "common.h"
#include "heap-object-header.h"

#include "core/trace.h"
#include "core/free-list.h"

namespace lavascript {

class Instance;

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
 * Heap allocator
 *
 * Used to allocate chunk memory used by Heap
 */

class HeapAllocator {
 public:
  virtual void* Malloc( size_t size ) = 0;
  virtual void  Free  ( void* ) = 0;
  virtual ~HeapAllocator() {}
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
  // Initialize the Heap object via Instance object
  inline Heap( std::size_t , double , std::size_t , HeapAllocator* );
  ~Heap();

  std::size_t threshold() const { return threshold_; }
  void set_threshold( std::size_t threshold ) { threshold_ = threshold; }

  std::size_t factor() const { return factor_; }
  void set_factor( std::size_t factor ) { factor_ = factor; }

  std::size_t alive_size() const { return alive_size_; }
  std::size_t allocated_size() const { return allocated_size_; }
  std::size_t chunk_size() const { return chunk_size_; }
  std::size_t total_bytes() const { return total_bytes_; }

 public: // Allocation functions

  /**
   * This is the main function to grab a chunk of memory from Heap.
   * Currently the heap implementation is really simple since it only implements a
   * bump allocator. It tries to allocate memory from the current Chunk list and if
   * it fails then it just create a new Chunk and then create the object from it.
   *
   * If the current Heap bypass the threshold set, then the Heap will return NULL
   * in this cases. The upper wrapper code in GC will launch the GC cycle and then
   * retry the API again
   *
   * Also there's another possibilities that it returns NULL when it cannot allocate
   * any memory from malloc/free or underlying memory allocator
   */

  inline
  void* Grab( std::size_t object_size ,             // Size of object , not include header
              ValueType type ,                      // Type of the object
              GCState gc_state = GC_WHITE,          // State of the GC
              bool is_long_str = false);            // Whether it is a long string

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
  RefillChunk(0);
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
      current_cursor_ += HeapObjectHeader::kHeapObjectHeaderSize + hdr.size();
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

inline void* Heap::Grow( size_t object_size , ValueType type, GCState gc_state ,
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
   };

   GC( const GCConfig& , Instance* );

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
};


} // namespace lavascript
#endif // GC_H_
