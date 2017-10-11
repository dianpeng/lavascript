#include "objects.h"
#include "gc.h"
#include "script-builder.h"
#include "error-report.h"
#include "interpreter/bytecode-builder.h"

#include <sstream>

namespace lavascript {

/* ---------------------------------------------------------------
 * String
 * --------------------------------------------------------------*/
Handle<String> String::New( GC* gc ) {
  return Handle<String>(gc->NewString());
}

Handle<String> String::New( GC* gc , const char* str , std::size_t length ) {
  return Handle<String>(gc->NewString(str,length));
}

/* ---------------------------------------------------------------
 * List
 * --------------------------------------------------------------*/
Handle<List> List::New( GC* gc ) {
  Handle<Slice> slice(gc->NewSlice());
  return Handle<List>(gc->New<List>(slice));
}

Handle<List> List::New( GC* gc , size_t capacity ) {
  Handle<Slice> slice(gc->NewSlice(capacity));
  return Handle<List>(gc->New<List>(slice));
}

Handle<List> List::New( GC* gc , const Handle<Slice>& slice ) {
  return Handle<List>(gc->New<List>(slice));
}

/* ---------------------------------------------------------------
 * Slice
 * -------------------------------------------------------------*/
Handle<Slice> Slice::Extend( GC* gc, const Handle<Slice>& old ) {
  std::size_t new_cap = (old->capacity()) * 2;
  if(!new_cap) new_cap = kDefaultListSize;

  Handle<Slice> new_slice(gc->NewSlice(new_cap));
  memcpy(new_slice->data(),old->data(),old->capacity()*sizeof(Value));
  return new_slice;
}

Handle<Slice> Slice::New( GC* gc ) {
  return Handle<Slice>(gc->NewSlice());
}

Handle<Slice> Slice::New( GC* gc , std::size_t cap ) {
  return Handle<Slice>(gc->NewSlice(cap));
}

/* ---------------------------------------------------------------
 * Object
 * --------------------------------------------------------------*/
Handle<Object> Object::New( GC* gc ) {
  return Handle<Object>(gc->New<Object>(gc->NewMap()));
}

Handle<Object> Object::New( GC* gc , std::size_t capacity ) {
  return Handle<Object>(gc->New<Object>(gc->NewMap(capacity)));
}

Handle<Object> Object::New( GC* gc , const Handle<Map>& map ) {
  return Handle<Object>(gc->New<Object>(map));
}

/* ---------------------------------------------------------------
 * Map
 * --------------------------------------------------------------*/
Handle<Map> Map::New( GC* gc ) {
  return Handle<Map>(gc->NewMap());
}

Handle<Map> Map::New( GC* gc , std::size_t capacity ) {
  return Handle<Map>(gc->NewMap(capacity));
}

Handle<Map> Map::Rehash( GC* gc , const Handle<Map>& old_map ) {
  std::size_t new_cap = old_map->capacity() * 2;
  if(!new_cap) new_cap = kDefaultObjectSize;

  Handle<Map> new_map(gc->NewMap(new_cap));
  const std::size_t capacity = old_map->capacity();

  for( std::size_t i = 0 ; i < capacity ; ++i ) {
    const Entry* e = old_map->data()+i;
    if(e->active()) {
      Entry* new_entry = new_map->FindEntry(e->key.GetString(),
                                            e->hash,
                                            INSERT);
      lava_debug(NORMAL,lava_verify(new_entry && !new_entry->use););

      new_entry->use = 1;
      new_entry->value = e->value;
      new_entry->key = e->key;
      new_entry->hash = e->hash;
      ++new_map->size_;
      ++new_map->slot_size_;
    }
  }
  return new_map;
}

/* ---------------------------------------------------------------
 * Prototype
 * --------------------------------------------------------------*/
Prototype::Prototype( const Handle<String>& pp , std::size_t argument_size ,
                                                 std::size_t int_table_size,
                                                 std::size_t real_table_size,
                                                 std::size_t string_table_size,
                                                 std::size_t upvalue_size,
                                                 std::size_t code_buffer_size ,
                                                 std::int32_t* itable ,
                                                 double* rtable,
                                                 String*** stable,
                                                 std::uint32_t* utable,
                                                 std::uint32_t* cb,
                                                 SourceCodeInfo* sci ):
  proto_string_(pp),
  argument_size_(argument_size),
  int_table_size_(int_table_size),
  real_table_size_(real_table_size),
  string_table_size_(string_table_size),
  upvalue_size_(upvalue_size),
  code_buffer_size_(code_buffer_size),
  int_table_(itable),
  real_table_(rtable),
  string_table_(stable),
  upvalue_table_(utable),
  code_buffer_(cb),
  sci_buffer_(sci)
{}

std::uint16_t Prototype::GetUpValue( std::size_t index ,
                                     interpreter::UpValueState* state ) const {
  const std::uint32_t* upvalue = upvalue_table();
  lava_debug(NORMAL,lava_verify(upvalue && index < upvalue_size_););
  std::uint32_t v = upvalue[index];
  std::uint16_t ret;
  interpreter::BytecodeBuilder::DecodeUpValue(v,&ret,state);
  return ret;
}

void Prototype::Dump( DumpWriter* writer , const std::string& source ) const {

  { // prototype
    DumpWriter::Section(writer,"Prototype:%s",proto_string_->ToStdString().c_str());
  }

  { // integer table
    DumpWriter::Section section(writer,"Integer Table");
    for( std::size_t i = 0 ; i < int_table_size() ; ++i ) {
      writer->WriteL("%zu.     %d",i,GetInteger(i));
    }
  }

  { // real table
    DumpWriter::Section section(writer,"Real Table");
    for( std::size_t i = 0 ; i < real_table_size(); ++i ) {
      writer->WriteL("%zu.     %f",i,GetReal(i));
    }
  }

  { // string table
    DumpWriter::Section section(writer,"String Table");
    for( std::size_t i = 0 ; i < string_table_size(); ++i ) {
      writer->WriteL("%zu.     %s",i,GetString(i)->ToStdString().c_str());
    }
  }

  { // upvalue table
    DumpWriter::Section section(writer,"UpValue Table");
    for( std::size_t i = 0 ; i < upvalue_size(); ++i ) {
      interpreter::UpValueState st;
      std::uint16_t idx = GetUpValue(i,&st);
      writer->WriteL("%zu.     %d(%s)",i,idx,interpreter::GetUpValueStateName(st));
    }
  }

  { // dump the bytecode area
    DumpWriter::Section section(writer,"Bytecode");
    auto bi = GetBytecodeIterator();
    std::size_t count = 0;
    std::uint8_t a1_8, a2_8, a3_8;
    std::uint16_t a1_16 , a2_16;
    for( ; bi.HasNext() ; bi.Next() ) {
      const SourceCodeInfo& sci = GetSci(count);
      switch(bi.type()) {
        case interpreter::TYPE_B:
          bi.GetOperand(&a1_8,&a2_16);
          writer->WriteL("%zu. %s %d %d  | <%d,%d> %s",count,bi.opcode_name(),a1_8,a2_16,
              sci.start, sci.end, GetSourceSnippetInOneLine(source,sci).c_str());
          break;
        case interpreter::TYPE_C:
          bi.GetOperand(&a1_16,&a2_8);
          writer->WriteL("%zu. %s %d %d  | <%d,%d> %s",count,bi.opcode_name(),a1_16,a2_8,sci.start,
              sci.end, GetSourceSnippetInOneLine(source,sci).c_str());
          break;
        case interpreter::TYPE_D:
          bi.GetOperand(&a1_8,&a2_8,&a3_8);
          writer->WriteL("%zu. %s %d %d %d  | <%d,%d> %s",count,bi.opcode_name(),a1_8,a2_8,a3_8,sci.start,
              sci.end,GetSourceSnippetInOneLine(source,sci).c_str());
          break;
        case interpreter::TYPE_E:
          bi.GetOperand(&a1_8,&a2_8);
          writer->WriteL("%zu. %s %d %d  | <%d,%d> %s",count,bi.opcode_name(),a1_8,a2_8,sci.start,
              sci.end,GetSourceSnippetInOneLine(source,sci).c_str());
          break;
        case interpreter::TYPE_F:
          bi.GetOperand(&a1_8);
          writer->WriteL("%zu. %s %d  | <%d,%d> %s",count,bi.opcode_name(),a1_8,sci.start,
              sci.end,GetSourceSnippetInOneLine(source,sci).c_str());
          break;
        case interpreter::TYPE_G:
          bi.GetOperand(&a1_16);
          writer->WriteL("%zu. %s %d  | <%d,%d> %s",count,bi.opcode_name(),a1_16,sci.start,
              sci.end,GetSourceSnippetInOneLine(source,sci).c_str());
          break;
        case interpreter::TYPE_N:
          {
            // need to format it here since writer doesn't support dumpping the vector type
            // maybe a better formatter designed specifically for C++ constructs ??
            std::stringstream formatter;
            std::vector<std::uint8_t> vec;
            bi.GetOperand(&a1_8,&a2_8);
            bi.GetNArg(&vec);

            formatter<<count<<". "<<bi.opcode_name()<<' '<<static_cast<int>(a1_8)<<' '
                                                         <<static_cast<int>(a2_8)<<" ( ";
            for( auto &e : vec )
              formatter<< static_cast<int>(e) <<' ';

            formatter<<") | <"<<sci.start<<','<<sci.end<<"> "<<GetSourceSnippetInOneLine(source,sci);

            writer->WriteL(formatter.str().c_str());
          }
          break;
        default:
          writer->WriteL("%zu. %s  | <%d,%d> %s",count,bi.opcode_name(),sci.start,sci.end,
              GetSourceSnippetInOneLine(source,sci).c_str());
          break;
      }
      count += bi.offset();
    }
  }
}

/* ---------------------------------------------------------------
 * Script
 * --------------------------------------------------------------*/
Handle<Script> Script::New( GC* gc , Context* context , const ScriptBuilder& sb ) {
  Handle<String> source = String::New(gc,sb.source());
  Handle<String> filename = String::New(gc,sb.source());
  const std::size_t reserve = sb.function_table_size() * sizeof(FunctionTableEntry);

  Script** ref = gc->NewScript(context,source.ref(),
                                       filename.ref(),
                                       sb.main().ref(),
                                       sb.function_table_size(),
                                       reserve);
  Script* script = *ref;
  FunctionTableEntry* start = script->fte_array();
  // Copy the function table in the script object
  MemCopy(start,sb.function_table());
  return Handle<Script>(ref);
}

} // namespace lavascript
