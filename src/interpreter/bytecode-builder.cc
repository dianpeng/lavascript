#include "bytecode-builder.h"

#include "src/zone/string.h"
#include "src/parser/ast/ast.h"
#include "src/util.h"
#include "src/gc.h"
#include "src/objects.h"

namespace lavascript {
namespace interpreter{

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

std::int32_t BytecodeBuilder::AddSSO( const ::lavascript::zone::String& sso ,
                                      GC* gc ) {
  lava_debug(NORMAL,lava_verify(sso.IsSSO()););

  auto ret = std::find_if(sso_table_.begin(),sso_table_.end(),
      [=](Prototype::SSOTableEntry& lhs) { return *(lhs.sso) == sso.data(); });

  if(ret == sso_table_.end()) {
    if(sso_table_.size() == kMaxLiteralSize) {
      return -1;
    }
    Handle<String> str(String::New(gc,sso.data(),sso.size()));
    sso_table_.push_back(Prototype::SSOTableEntry(&(str->sso()),str.ref()));
    return static_cast<std::int32_t>(sso_table_.size()-1);
  }

  return (static_cast<std::int32_t>(
        std::distance(sso_table_.begin(),ret)));
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
                                                 std::size_t max_local_var_size,
                                                 String** proto ) {

  Prototype** pp = gc->NewPrototype(proto ? proto : String::New(gc,"()",2).ref(),
                                    static_cast<std::uint8_t>(arg_size),
                                    static_cast<std::uint8_t>(max_local_var_size),
                                    static_cast<std::uint8_t>(bb.real_table_.size()),
                                    static_cast<std::uint8_t>(bb.string_table_.size()),
                                    static_cast<std::uint8_t>(bb.sso_table_.size()),
                                    static_cast<std::uint8_t>(bb.upvalue_slot_.size()),
                                    static_cast<std::uint32_t>(bb.code_buffer_.size()));
  Prototype* ret = *pp;

  // initialize each field
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
    Prototype::SSOTableEntry* arr = ret->sso_table();
    if(arr) MemCopy(arr,bb.sso_table_);
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

  {
    std::uint8_t* arr = const_cast<std::uint8_t*>(ret->reg_offset_table());
    if(arr) MemCopy(arr,bb.reg_offset_table_);
  }

  return Handle<Prototype>(pp);
}

Handle<Prototype> BytecodeBuilder::NewMain( GC* gc , const BytecodeBuilder& bb ,
                                                     std::size_t max_local_var_size ) {
  return New(gc,bb,0,max_local_var_size,NULL);
}

Handle<Prototype> BytecodeBuilder::New( GC* gc , const BytecodeBuilder& bb ,
                                                 const ::lavascript::parser::ast::Function& node ) {
  return New(gc,bb,node.proto->size(),node.lv_context->local_variable_count(),
                                      BuildFunctionPrototypeString(gc,node));
}

} // namespace interpreter
} // namespace lavascript
