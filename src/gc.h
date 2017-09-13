#ifndef GC_H_
#define GC_H_

#include "core/trace.h"
#include "core/free-list.h"

namespace lavascript {

/**
 * GC implemention for lavascript. This GC implementation is a stop-the-world
 * with mark and sweep style. We do compaction as well. Currently implementation,
 * for simplicity , we just do everything in one cycle. In the future , we will
 * optimize it to be generation based
 */


/**
 * GCRefPool is a pool to track *ALL* the places that we store a heap pointer,
 * managed pointer. It is basically just a free list wrapper . We will walk
 * through the GCRefPool for compaction purpose
 */

class GCRefPool {
  struct Ref {
    Ref* next;            // Pointer to the next Ref in RefList
    HeapObject** object;  // Actual references
  };
 public:
  // Size of all active/alive GCRef reference
  size_t size() const { return free_list_.size(); }

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























#endif // GC_H_
