#include "gc.h"
#include "os.h"
#include "objects.h"
#include "hash.h"

#include <iostream>
#include <fstream>

namespace lavascript {

LAVA_DEFINE_INT64(GC,minimum_gap,"minimum gap between each GC cycle",0);
LAVA_DEFINE_DOUBLE(GC,factor,"tunable factor for triggering GC cycle",0.0);
LAVA_DEFINE_INT64(GC,heap_init_capacity,"heap initialize capacity",10240);
LAVA_DEFINE_INT64(GC,heap_capacity,"heap's max capacity",40960);
LAVA_DEFINE_INT64(GC,gcref_init_capacity,"gcref initialized capacity",1024);
LAVA_DEFINE_INT64(GC,gcref_capacity,"gcref maximum capacity",2048);
LAVA_DEFINE_INT64(GC,sso_init_slot,"sso initialize slot size",1024);
LAVA_DEFINE_INT64(GC,sso_init_capacity,"sso initialize capacity",2048);
LAVA_DEFINE_INT64(GC,sso_capacity,"sso maximum capacity",4096);

namespace gc {

Heap::Heap( size_t chunk_capacity , size_t init_size ,
                                    HeapAllocator* allocator ):
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
  lava_debug(NORMAL,lava_verify(HasNext()););

  HeapObjectHeader hdr(
      static_cast<char*>(current_chunk_->start()) + current_cursor_ );
  if(hdr.IsEndOfChunk()) {
    current_chunk_ = current_chunk_->next;
    current_cursor_ = 0;
    // The following test for bytes_used > 0 is *NEEDED* for
    // very corner case (highly unlikely) that the very first
    // chunk cannot be used for satisfying the very first Grab
    //
    // The we may have a chunk that doesn't contain any useful
    // object and it can only be the *first* chunk which is in
    // the *last of our chunk queue.
    if(current_chunk_ && current_chunk_->bytes_used > 0) {
      return true;
    } else {
      current_chunk_ = NULL;
    }
  } else {
    current_cursor_ += hdr.total_size();
    return true;
  }
  lava_debug(NORMAL,lava_verify(current_chunk_ == NULL););

  return false;
}

void Heap::Swap( Heap* that ) {
  std::swap( alive_size_, that->alive_size_);
  std::swap( chunk_size_, that->chunk_size_);
  std::swap( allocated_bytes_ , that->allocated_bytes_ );
  std::swap( total_bytes_ , that->total_bytes_ );
  std::swap( chunk_capacity_ , that->chunk_capacity_ );
  std::swap( chunk_current_ , that->chunk_current_ );
  std::swap( fall_back_ , that->fall_back_ );
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
  object_size = Align(object_size,kMemoryAlignment);
  std::size_t size = object_size + HeapObjectHeader::kHeapObjectHeaderSize;
  void* buf = NULL;

  if(chunk_current_->bytes_left() < size) {
    buf = FindInChunk(size);
    if(!buf) {
      if(!RefillChunk(size)) return NULL;
      lava_debug(NORMAL,lava_verify(chunk_current_->bytes_left() >= size););
      buf = chunk_current_->Bump(size);
    }
  } else {
    buf = chunk_current_->Bump(size);
  }

  // Now try to allocate it from the newly created chunk
  lava_debug(NORMAL,lava_verify( is_long_str ? type == TYPE_STRING : true ););

  allocated_bytes_ += size;
  alive_size_++;

  return SetHeapObjectHeader(buf,object_size,type, gc_state,is_long_str);
}

void* Heap::CopyObject( const void* ptr , std::size_t length ) {
  lava_debug(NORMAL,
      lava_verify(!length);
      lava_verify( Align(length,kMemoryAlignment) == length );
    );

  void* buf = NULL;
  if(chunk_current_->bytes_left() < length) {
    buf = FindInChunk(length);
    if(!buf) {
      if(!RefillChunk(length)) return NULL;
      lava_debug(NORMAL,lava_verify(chunk_current_->bytes_left() >= length););

      buf = chunk_current_->Bump(length);
    }
  } else {
    buf = chunk_current_->Bump(length);
  }
  std::memcpy(buf,ptr,length);
  allocated_bytes_ += length;
  alive_size_++;
  return buf;
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
      Entry* entry  = FindOrInsert(&new_entry,
                                   static_cast<const char*>(e.sso->data()),
                                   e.sso->size(),
                                   e.sso->hash());
      lava_debug(NORMAL,
          lava_verify(entry->sso == NULL);
          lava_verify(entry->next== NULL);
        );

      entry->sso = e.sso;
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
        return e;
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
  if(size() == entry_.size()) {
    Rehash();
  }

  std::uint32_t hash = Hasher::Hash(str,length);
  Entry* e = FindOrInsert(&entry_,str,length,hash);
  if(e->sso)
    return e->sso;
  else {
    SSO* sso = ConstructFromBuffer<SSO>(
        allocator_.Grab(Align(sizeof(SSO)+length,kMemoryAlignment)), length, hash );
    if(length)
      std::memcpy( reinterpret_cast<char*>(sso) + sizeof(SSO) , str , length );
    e->sso = sso;
    ++size_;
    return sso;
  }
}

bool SSOPool::Iterator::Move() {
  for( ; index_ < entry_->size() ; ++index_ ) {
    const Entry& e = entry_->at(index_);
    if(e.sso) {
      return true;
    }
  }
  return false;
}

void Heap::Dump( int verbose , DumpWriter* dw ) {
  DumpWriter& writer = *dw;

  writer.Write("********************* Heap Dump ****************************");
  writer.Write("AliveSize:%zu;ChunkSize:%zu;AllocatedBytes:%zu;"
               "TotalBytes:%zu;ChunkCapacity:%zu",
               alive_size_, chunk_size_, allocated_bytes_,
               total_bytes_, chunk_capacity_);

  if(verbose != DUMP_NORMAL) {

    Chunk* ck = chunk_current_;
    while(ck) {
      writer.Write("********************* Chunk ******************************");
      writer.Write("SizeInBytes:%zu;SizeInObjects:%zu;BytesUsed:%zu;",
          ck->size_in_bytes,
          ck->size_in_objects,
          ck->bytes_used);

      if(verbose == DUMP_CRAZY) {
        char* start = static_cast<char*>(ck->start());
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

String** GC::NewString( const void* str , std::size_t length ) {
  lava_debug(NORMAL,lava_verify(str););
  /**
   * String is an object without any data member all it needs
   * to do is to place correct object at the pointer of String.
   * Internally String will cast *this* to correct type based
   * on the heap header tag
   */
  if(length > kSSOMaxSize) {
    LongString* long_string = ConstructFromBuffer<LongString>(
        heap_.Grab( sizeof(LongString) + length , /* The string is stored right after LongString object */
                    TYPE_STRING,
                    GC_WHITE,
                    true ) , length );

    /** Copy the content from str to the end of long_string */
    if(length)
      std::memcpy( reinterpret_cast<char*>(long_string) + sizeof(LongString) ,
          str, length );

    String** ref = reinterpret_cast<String**>(ref_pool_.Grab());

    *ref = reinterpret_cast<String*>(long_string);

    return ref;
  } else {
    // Allocate a SSO from the SSOPool
    SSO* sso = sso_pool_.Get( str , length );

    // Allocate the reference/holder for sso
    SSO** sso_string = reinterpret_cast<SSO**>( heap_.Grab( sizeof(void*),
                                                            TYPE_STRING ,
                                                            GC_WHITE,
                                                            false ) );
    *sso_string = sso;

    // Allocate the reference from ref_pool_
    String** ref = reinterpret_cast<String**>(ref_pool_.Grab());

    // Store the sso_string/String* into the original String**
    *ref = reinterpret_cast<String*>(sso_string);

    return ref;
  }
}

Slice** GC::NewSlice( std::size_t capacity ) {
  Slice* slice = ConstructFromBuffer<Slice>(
      heap_.Grab( sizeof(Slice) + capacity * sizeof(Value) ,
                  TYPE_SLICE,
                  GC_WHITE,
                  false ) , capacity );

  for( size_t i = 0 ; i < capacity ; ++i ) {
    ConstructFromBuffer<Value>( slice->data() + i );
  }

  Slice** ref = reinterpret_cast<Slice**>(ref_pool_.Grab());

  *ref = slice;

  return ref;
}

Map** GC::NewMap( std::size_t capacity ) {
  lava_debug(NORMAL,lava_verify(capacity && (!(capacity & (capacity-1)))););

  Map* map = ConstructFromBuffer<Map>(
      heap_.Grab( sizeof(Map) + capacity * sizeof(Map::Entry) ,
                  TYPE_MAP,
                  GC_WHITE,
                  false ) , capacity );
  if(capacity) {
    std::memset( map->data() , 0 , sizeof(Map::Entry)*capacity );
  }

  Map** ref = reinterpret_cast<Map**>(ref_pool_.Grab());

  *ref = map;

  return ref;
}

Prototype** GC::NewPrototype( String** proto,
                              std::uint8_t argument_size ,
                              std::uint8_t max_local_var_size,
                              std::uint8_t real_table_size,
                              std::uint8_t string_table_size,
                              std::uint8_t sso_table_size,
                              std::uint8_t upvalue_size,
                              std::uint32_t code_buffer_size ) {

  // Highly sensitive to the layout of the Prototype object
  std::size_t rtable_bytes = Align(real_table_size*sizeof(double),kMemoryAlignment);
  std::size_t stable_bytes = Align(string_table_size*sizeof(String**),kMemoryAlignment);
  std::size_t ssotable_bytes = Align(sso_table_size*sizeof(Prototype::SSOTableEntry),kMemoryAlignment);
  std::size_t utable_bytes = Align(upvalue_size*sizeof(std::uint32_t),kMemoryAlignment);
  std::size_t cb_bytes     = Align(code_buffer_size*sizeof(std::uint32_t),kMemoryAlignment);
  std::size_t sci_bytes    = Align(code_buffer_size*sizeof(SourceCodeInfo),kMemoryAlignment);
  std::size_t roff_bytes   = Align(code_buffer_size*sizeof(std::uint8_t),kMemoryAlignment);

  void* proto_buffer = heap_.Grab( sizeof(Prototype) + rtable_bytes +
                                                       stable_bytes +
                                                       ssotable_bytes +
                                                       utable_bytes +
                                                       cb_bytes     +
                                                       sci_bytes    +
                                                       roff_bytes , TYPE_PROTOTYPE, GC_WHITE, false );

  // now , figure out each buffer's starting address
  std::size_t acc = 0;
  void* base = BufferOffset<Prototype>(proto_buffer,1);

  // NOTES: always put rtable at very first since it is always used at very first
  void* rtable = rtable_bytes ? base : NULL; acc += rtable_bytes;
  void* stable = stable_bytes ? BufferOffset<char>(base,acc) : NULL; acc += stable_bytes;
  void* ssotable= ssotable_bytes ? BufferOffset<char>(base,acc) : NULL; acc += ssotable_bytes;
  void* utable = utable_bytes ? BufferOffset<char>(base,acc) : NULL; acc += utable_bytes;
  void* cb     = cb_bytes     ? BufferOffset<char>(base,acc) : NULL; acc += cb_bytes;
  void* sci    = sci_bytes    ? BufferOffset<char>(base,acc) : NULL; acc += sci_bytes;
  void* roff   = roff_bytes   ? BufferOffset<char>(base,acc) : NULL; acc += roff_bytes;

  // construct the Prototype object right on the buffer
  Prototype* p = ConstructFromBuffer<Prototype>(proto_buffer,
                                                Handle<String>(proto),
                                                argument_size,
                                                max_local_var_size,
                                                real_table_size,
                                                string_table_size,
                                                sso_table_size,
                                                upvalue_size,
                                                code_buffer_size,
                                                static_cast<double*>(rtable),
                                                static_cast<String***>(stable),
                                                static_cast<Prototype::SSOTableEntry*>(ssotable),
                                                static_cast<std::uint32_t*>(utable),
                                                static_cast<std::uint32_t*>(cb),
                                                static_cast<SourceCodeInfo*>(sci),
                                                static_cast<std::uint8_t*>(roff)
                                                );

  Prototype** ref = reinterpret_cast<Prototype**>(ref_pool_.Grab());
  *ref = p;
  return ref;
}

Closure** GC::NewClosure( Prototype** proto ) {
  // Get memory from *heap_buffer* object
  void* heap_buffer = heap_.Grab( sizeof(Closure) +sizeof(Value)*((*proto)->upvalue_size()),
                                  TYPE_CLOSURE,
                                  GC_WHITE,
                                  false );

  Closure* cls = ConstructFromBuffer<Closure>(heap_buffer,Handle<Prototype>(proto));
  Closure** ref = reinterpret_cast<Closure**>(ref_pool_.Grab());
  *ref = cls;
  return ref;
}

Script** GC::NewScript( Context* context ,
                        String** source ,
                        String** filename,
                        Prototype** proto,
                        std::size_t function_table_size,
                        std::size_t reserve ) {

  Script* p = ConstructFromBuffer<Script>(
      heap_.Grab( sizeof(Script) + reserve ,
                  TYPE_SCRIPT,
                  GC_WHITE,
                  false ) , context , Handle<String>(source),
                                      Handle<String>(filename),
                                      Handle<Prototype>(proto),
                                      function_table_size );

  Script** ref = reinterpret_cast<Script**>(ref_pool_.Grab());
  *ref = p;
  return ref;
}

bool GC::GrowInterpreterStack( interpreter::Runtime* runtime ) {
  // Get max_stack_size of interpreter
  const std::size_t max_stack_size = LAVA_OPTION(Interpreter,max_stack_size);

  if(interpreter_stack_size() >= max_stack_size) return false;

  std::size_t nsize = interpreter_stack_size() * 2;

  if(nsize > max_stack_size) nsize = max_stack_size;

  if(!nsize) nsize = LAVA_OPTION(Interpreter,init_stack_size);

  /**
   * this loop figures out the diff for each runtime object created while
   * doing the intepretation. Because after the realloc, we gonna lose the
   * diff information due to the old pointer is not valid anymore
   */
  std::vector<std::uint64_t> diff_array;
  diff_array.reserve(16);

  {
    auto temp = runtime;
    do {
      auto diff = (char*)(temp->cur_stk) - (char*)(interp_stack_start_);
      diff_array.push_back(diff);
      temp = temp->previous;
    } while(temp);
  }

  // do the reallcation
  void* data = Realloc( allocator_ , interp_stack_start_ , nsize * sizeof(Value) );

  interp_stack_start_ = reinterpret_cast<Value*>(data);
  interp_stack_end_   = reinterpret_cast<Value*>(data) + nsize;

  // setup the test field for indicating the stack is overflow
  Value* test_field = interp_stack_end_ - interpreter::kRegisterSize;


  /**
   * Restore each runtime's field due to the nested call or other stuff
   */
  {
    std::size_t idx = 0;
    do {
      runtime->cur_stk = interp_stack_start_ + diff_array[idx];
      runtime->stack_test = test_field;
      runtime = runtime->previous;
    } while(runtime);
  }

  return true;
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
}

void GC::PhaseSwap( std::size_t new_heap_size ) {
  gc::Heap new_heap(heap_.chunk_capacity(),
                    new_heap_size,
                    allocator_);

  // Get the iterator for all reference slot
  gc::GCRefPool::Iterator itr( ref_pool_.GetIterator() );

  while(itr.HasNext()) {
    HeapObject** ref = itr.heap_object();
    lava_debug(NORMAL,
        lava_verify(*ref);
        lava_verify(!((*ref)->hoh().IsGCGray()));
      );

    if((*ref)->hoh().IsGCWhite()) {
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
  MarkResult result;
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
