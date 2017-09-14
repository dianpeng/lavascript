#include "gc.h"
#include "os.h"

#include <iostream>

namespace lavascript {
namespace gc {

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
    allocator ? allocator->Free(chunk_current_) : ::free(chunk_current_);
    chunk_current_ = ck;
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
      std::string buf(core::FormatV(fmt,vl));
      buf.push_back('\n');
      file_.write(buf.c_str(),buf.size());
    } else {
      lava_info("%s",core::FormatV(fmt,vl).c_str());
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

          start = (start + hdr.size() + HeapObjectHeader::kHeapObjectHeaderSize);
        } while(true);
      }

      writer.Write("**********************************************************");
      ck = ck->next;
    }
  }
  writer.Write("**********************************************************");
}

} // namespace gc
} // namespace lavascript
