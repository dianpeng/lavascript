#include "gc.h"
#include "os.h"
#include "objects.h"
#include "hash.h"

#include <iostream>

namespace lavascript {
namespace gc {

Heap::Heap( size_t threshold , double factor ,
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

bool Heap::Iterator::Move() {
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

void Heap::Swap( Heap* that ) {
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

bool Heap::RefillChunk( size_t size ) {
  size_t raw_size;
  if( chunk_capacity_ < size ) {
    raw_size = size;
  } else {
    raw_size = chunk_capacity_;
  }
  void* new_buf = Malloc( allocator_ , raw_size + sizeof(Chunk) );
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

void* Heap::Grab( size_t object_size , ValueType type, GCState gc_state ,
                                                       bool is_long_str ) {
  object_size = Align(object_size,kAlignment);
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
  lava_verify( is_long_str ? type == TYPE_STRING : true );
#endif // LAVASCRIPT_CHECK_OBJECTS

  allocated_bytes_ += size;
  alive_size_++;

  return SetHeapObjectHeader(chunk_current->Bump(size),object_size,type,
                                                                   gc_state,
                                                                   is_long_str);
}

void* Heap::CopyObject( const void* ptr , std::size_t length ) {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify( Align(length,kAlignment) == length );
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

void* Heap::FindInChunk( std::size_t raw_bytes_length ) {
  lava_bench("Heap::FindInChunk()");

  if(fall_back_ == NULL) {
    fall_back_ = chunk_current_;
  }

  Chunk* ck = fall_back_;

  while(ck) {
    if(ck->bytes_left() >= raw_bytes_length) {
      fall_back_ = ck;
      return ck->Bump(raw_bytes_length);
    }
    ck = ck->next;
  }

  fall_back_ = NULL;
  return NULL;
}

Heap::~Heap() {
  while(chunk_current_) {
    Chunk* ck = chunk_current_->next;
    Free( allocator_ , chunk_current_ );
    chunk_current_ = ck;
  }
}

void SSOPool::Rehash() {
  std::vector<Entry> new_entry;
  new_entry.resize( entry_.size() * 2 );

  for( auto &e : entry_ ) {
    if(e.sso) {
      Entry* new_entry = FindOrInsert(&new_entry,
                                      e.sso->data(),
                                      e.sso->size(),
                                      e.sso->hash());
#ifdef LAVASCRIPT_CHECK_OBJECTS
      lava_verify(new_entry->sso == NULL);
      lava_verify(new_entry->next== NULL);
#endif // LAVASCRIPT_CHECK_OBJECTS

      new_entry->sso = e.sso;
    }
  }

  entry_.swap(new_entry);
}

namespace {

bool Equal( const char* l , std::size_t lh , const SSO& right ) {
  if(lh != right.size())
    return false;
  else {
    int r = memcmp(l,right.data(),lh);
    return r == 0;
  }
}

} // namespace

SSOPool::Entry* SSOPool::FindOrInsert( std::vector<Entry>* entry ,
                                       const char* str,
                                       std::size_t length ,
                                       std::uint32_t hash ) {
  std::size_t index = (hash & (entry->size()-1));
  Entry* e = &(entry->at(index));
  if(!e->sso) { return e; }
  else {
    do {
      if(e->sso->hash() == hash && Equal(str,length,*e->sso)) {
        return &e;
      }
      if(e->next) e = e->next;
      else break;
    } while(true);

    int h = hash;
    Entry* prev = e;
    while( (e = &(entry->at(++h &(entry->size()-1))))->sso )
      ;
    prev->next = e;
    return e;
  }
}

SSO* SSOPool::Get( const char* str , std::size_t length ) {
  std::uint32_t hash = Hasher::Hash(str,length);
  Entry* e = FindOrInsert(&entry_,str,length,hash);
  if(e->sso)
    return e->sso;
  else {
    SSO* sso = ConstructFromBuffer<SSO>( allocator_.Grab( sizeof(SSO) + length ) ,
                                         length,
                                         hash );
    memcpy( reinterpret_cast<char*>(sso) + sizeof(SSO) , str , length );
    e->sso = sso;
    return sso;
  }
}

namespace {

class DumpWriter {
 public:
  DumpWriter( const char* filename ):
    file_(),
    use_file_(filename != NULL)
  {
    if(use_file_)
      file_.open(filename,std::ios::in|std::ios::out|std::ios::trunc);
    if(!file_) use_file_ = false;
  }

  void Write( const char* fmt , ... ) {
    va_list vl;
    va_start(vl,fmt);

    if(use_file_) {
      std::string buf(FormatV(fmt,vl));
      buf.push_back('\n');
      file_.write(buf.c_str(),buf.size());
    } else {
      lava_info("%s",FormatV(fmt,vl).c_str());
    }
  }

 private:
  std::fstream file_;
  bool use_file_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(DumpWriter);
};

} // namespace

void Heap::HeapDump( int verbose , const char* filename ) {
  DumpWriter writer(filename);

  writer.Write("********************* Heap Dump ****************************");
  writer.Write("Threshold:%zu;Factor:%d;AliveSize:%zu;"
               "ChunkSize:%zu;AllocatedBytes:%zu;"
               "TotalBytes:%zu;ChunkCapacity:%zu",
               threshold_,
               factor_,
               alive_size_,
               chunk_size_,
               allocated_bytes_,
               total_bytes_,
               chunk_capacity_);

  if(verbose != HEAP_DUMP_NORMAL) {

    Chunk* ck = chunk_current_;
    while(ck) {
      writer.Write("********************* Chunk ******************************");
      writer.Write("SizeInBytes:%zu;SizeInObjects:%zu;BytesUsed:%zu;",
          ck->size_in_bytes,
          ck->size_in_objects,
          ck->bytes_used);

      if(verbose == HEAP_DUMP_CRAZY) {
        char* start = static_cast<char*>(ck.start());
        do  {
          HeapObjectHeader hdr(*reinterpret_cast<std::uint64_t*>(start));
          writer.Write("Type:%s;GCState:%s;IsSSO:%d;EOC:%d:Size:%zu;",
              GetValueTypeName(hdr.type()),
              GetGCStateName(hdr.gc_state()),
              hdr.IsSSO(),
              hdr.IsEndOfChunk(),
              hdr.size());

          // go to next object
          if(hdr.IsEndOfChunk()) break;

          start = (start + hdr.total_size());
        } while(true);
      }

      writer.Write("**********************************************************");
      ck = ck->next;
    }
  }
  writer.Write("**********************************************************");
}

} // namespace gc

String** GC::NewString( const char* str , std::size_t length ) {
  /**
   * String is an object without any data member all it needs
   * to do is to place correct object at the pointer of String.
   * Internally String will cast *this* to correct type based
   * on the heap header tag
   */
  if(length > kSSOMaxSize) {
    LongString* long_string = ConstructFromBuffer(
        heap_.Grab( sizeof(LongString) + length , /* The string is stored right after LongString object */
                    TYPE_STRING,
                    GC_WHITE,
                    true ) , length );

    /** Copy the content from str to the end of long_string */
    memcpy( reinterpret_cast<char*>(long_string) + sizeof(LongString) ,
            str, length );

    String** ref = reinterpret_cast<String**>(ref_pool_.Grab());
    *ref = reinterpret_cast<String*>(long_string);
    return ref;
  } else {
    /** Okay short string stays here */
    SSO* sso = sso_pool_.Get( str , length );
    SSO** sso_string = ConstructFromBuffer(
        heap_.Grab( sizeof(SSO*),
                    TYPE_STRING ,
                    GC_WHITE,
                    false ) , str, length );
    *sso_string = sso;

    String** ref = reinterpret_cast<String**>(ref_pool_.Grab());
    *ref = reinterpret_cast<String*>(sso_string);
    return ref;
  }
}

/**
 * Marking phase for our GC
 *
 *
 * Marking phase will start the marking from the following root :
 *  1. Stack
 *  2. Global State
 *  3. C++ Local (LocalScope/GlobalScope)
 */

void GC::PhaseMark( MarkResult* result ) {
  /*
   * TODO:: Add implementation when we finish our stuff
   */
}

void GC::PhaseSwap( std::size_t new_heap_size ) {
  gc::Heap new_heap(heap_.threshold(),
                    heap_.factor(),
                    heap_.chunk_capacity(),
                    new_heap_size,
                    allocator_);

  // Get the iterator for all reference slot
  gc::GCRefPool::Iterator itr( ref_pool_.GetIterator() );

  while(itr.HasNext()) {
    HeapObject** ref = itr.heap_object();

#ifdef LAVASCRIPT_CHECK_OBJECTS
    lava_verify(*ref);
    lava_verify(!(*ref)->IsGCGray());
#endif // LAVASCRIPT_CHECK_OBJECTS

    if((*ref)->IsGCWhite()) {
      // This object is alive , move to the new_heap
      void* raw_address = reinterpret_cast<void*>(
          (*ref)->hoh_address());

      // Copy the alive object into the new_heap
      new_heap.RawCopyObject( raw_address , (*ref)->hoh().total_size() );

      // Patch the reference pointer address
      *ref = reinterpret_cast<HeapObject*>( static_cast<char*>(raw_address) +
          HeapObjectHeader::kHeapObjectHeaderSize );

      // Move to the next slot
      itr.Move();
    } else {
      // Release the reference object since the pointed object are dead
      itr.Remove(&ref_pool_);
    }
  }

  // Swap the heap and free the old heap
  heap_.Swap(&new_heap);
}

void GC::ForceGC() {
  MarkResult reuslt;
  PhaseMark(&result);
  if(result.dead_size >0) {
    PhaseSwap(result.new_heap_size);
  }
  ++cycle_;
}

bool GC::TryGC() {
  /**
   * TODO:: Implement GC trigger
   */
  ForceGC();
  return true;
}

} // namespace lavascript
