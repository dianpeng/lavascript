#include "bytecode-builder.h"

#include "src/zone/string.h"
#include "src/parser/ast/ast.h"
#include "src/util.h"
#include "src/gc.h"
#include "src/objects.h"

namespace lavascript {
namespace interpreter{

bool BytecodeBuilder::EmitN( const SourceCodeInfo& sci , Bytecode bc ,
    std::uint8_t narg , std::uint8_t reg , std::uint8_t base ,
    const std::vector<std::uint8_t>& vec ) {
  std::uint32_t encode = static_cast<std::uint32_t>(bc);
  std::size_t before = code_buffer_.size();

  encode |= static_cast<std::uint32_t>(narg) << 8;
  encode |= static_cast<std::uint32_t>(reg)  <<16;
  encode |= static_cast<std::uint32_t>(base) <<24;
  code_buffer_.push_back(encode);

  // now pushing the rest *ARGUMENT* into the following slots of code buffer
  std::size_t i;
  std::size_t len = vec.size();

  for( i = 0 ; (i+3) < len ; i += 4 ) {
    code_buffer_.push_back(
        (static_cast<std::uint32_t>(vec[i]))        |
        (static_cast<std::uint32_t>(vec[i+1]) << 8) |
        (static_cast<std::uint32_t>(vec[i+2]) <<16) |
        (static_cast<std::uint32_t>(vec[i+3]) <<24));
  }

  lava_debug(NORMAL,lava_verify(i <= len););
  std::size_t left = len - i;
  switch(left) {
    case 1:
      code_buffer_.push_back(vec[i]);
      break;
    case 2:
      code_buffer_.push_back((static_cast<std::uint32_t>(vec[i]))     |
                             (static_cast<std::uint32_t>(vec[i+1])<<8));
      break;
    case 3:
      code_buffer_.push_back((static_cast<std::uint32_t>(vec[i]))     |
                             (static_cast<std::uint32_t>(vec[i+1])<<8)|
                             (static_cast<std::uint32_t>(vec[i+2])<<16));
      break;
    default:
      lava_debug(NORMAL,lava_verify(i == len););
      break;
  }

  // now pushing debug information to the debug_info_ buffer
  // since emitN will occupy more than one slot inside of the
  // code buffer, we generate same debug information inside of
  // each slot.
  {
    std::size_t len = code_buffer_.size() - before;
    for( std::size_t i = 0 ; i < len ; ++i ) {
      debug_info_.push_back(sci);
    }
  }
  return true;
}

std::int32_t BytecodeBuilder::Add( const ::lavascript::zone::String& str ,
                                   GC* gc ) {
  auto ret = std::find_if(string_table_.begin(),string_table_.end(),
  [=](const Handle<String> rhs) {
    return *rhs == str.data();
  });

  if(ret == string_table_.end()) {
    if(string_table_.size() == kMaxLiteralSize) {
      return -1;
    }
    Handle<String> hstr(String::New(gc,str.data(),str.size()));
    string_table_.push_back(hstr);
    return static_cast<std::int32_t>(string_table_.size()-1);
  }
  return (static_cast<std::int32_t>(
      std::distance(string_table_.begin(),ret)));
}

String** BytecodeBuilder::BuildFunctionPrototypeString( GC* gc ,
    const ::lavascript::parser::ast::Function& node ) {
  if(!node.proto->empty()) {
    std::string buffer;
    buffer.push_back('(');
    const std::size_t len = node.proto->size();
    for( std::size_t i = 0 ; i < len ; ++i ) {
      buffer.append(node.proto->Index(i)->name->data());
      if(i < len-1) buffer.push_back(',');
    }
    buffer.push_back(')');
    return String::New(gc,buffer).ref();
  } else {
    return String::New(gc,"()",2).ref();
  }
}

Handle<Prototype> BytecodeBuilder::New( GC* gc , const BytecodeBuilder& bb ,
                                                 std::size_t arg_size,
                                                 String** proto ) {

  Prototype** pp = gc->NewPrototype(proto ? proto : String::New(gc,"()",2).ref(),
                                    arg_size,
                                    bb.int_table_.size(),
                                    bb.real_table_.size(),
                                    bb.string_table_.size(),
                                    bb.upvalue_slot_.size(),
                                    bb.code_buffer_.size());
  Prototype* ret = *pp;

  // initialize each field
  {
    std::int32_t* arr = const_cast<std::int32_t*>(ret->int_table());
    if(arr) MemCopy(arr,bb.int_table_);
  }

  {
    double* arr = const_cast<double*>(ret->real_table());
    if(arr) MemCopy(arr,bb.real_table_);
  }

  {
    String*** arr = ret->string_table();
    for( std::size_t i = 0 ; i < bb.string_table_.size(); ++i ) {
      arr[i] = bb.string_table_[i].ref();
    }
  }

  {
    std::uint32_t* arr = const_cast<std::uint32_t*>(ret->upvalue_table());
    for( std::size_t i = 0 ; i < bb.upvalue_slot_.size(); ++i ) {
      arr[i] = bb.upvalue_slot_[i].Encode();
    }
  }

  {
    std::uint32_t* arr = const_cast<std::uint32_t*>(ret->code_buffer());
    if(arr) MemCopy(arr,bb.code_buffer_);
  }

  {
    SourceCodeInfo* arr = const_cast<SourceCodeInfo*>(ret->sci_buffer());
    if(arr) MemCopy(arr,bb.debug_info_);
  }

  return Handle<Prototype>(pp);
}

Handle<Prototype> BytecodeBuilder::NewMain( GC* gc , const BytecodeBuilder& bb ) {
  return New(gc,bb,0,NULL);
}

Handle<Prototype> BytecodeBuilder::New( GC* gc , const BytecodeBuilder& bb ,
                                                 const ::lavascript::parser::ast::Function& node ) {
  return New(gc,bb,node.proto->size(),BuildFunctionPrototypeString(gc,node));
}

} // namespace interpreter
} // namespace lavascript
