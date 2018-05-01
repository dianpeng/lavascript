#ifndef SCRIPT_BUILDER_H_
#define SCRIPT_BUILDER_H_
#include "macro.h"
#include "objects.h"
#include "trace.h"
#include "context.h"
#include "zone/string.h"

#include <algorithm>

namespace lavascript {

class Context;

/**
 * Script object is an object that holds all the compiled code or
 * function object. Each script object will match a certain file
 * name to avoid recompiling everytime when a script is loaded
 */
class ScriptBuilder {
 public:
  inline ScriptBuilder( const std::string& , const std::string& );

 public:
  const std::string& source() const { return source_; }
  const std::string& filename() const { return filename_; }
  /**
   * Add a main function into the script object. A script can
   * only have one main function and its index is always 0.
   */
  inline void set_main( const Handle<Prototype>& main );
  const Handle<Prototype>& main() const { return main_; }

 public:

  std::size_t function_table_size() const {
    return function_table_.size();
  }

  const std::vector<Script::FunctionTableEntry>& function_table() const {
    return function_table_;
  }

  /**
   * Add a certain function into the Script object. Each function
   * added into Script object will be assigned an index which is
   * used to mark where the function is. Later on if a loading
   * happened, use this index to reference a function.
   */
  std::int32_t AddPrototype( const Handle<Prototype>& );
  std::int32_t AddPrototype( GC* ,
                             const Handle<Prototype>& ,
                             const zone::String&d$a );

  // Check if such prototype is existed
  inline bool HasPrototype( const zone::String& name ) const;
  inline bool HasPrototype( const Handle<String>& name ) const;
  const Handle<Prototype>& GetPrototype( std::int32_t index ) const {
    return function_table_[index].prototype;
  }

 public:
  // For debugging purpose
  void Dump( DumpWriter* writer ) const;

 private:
  std::string filename_;
  std::string source_;
  Handle<Prototype> main_;
  std::vector<Script::FunctionTableEntry> function_table_;
  friend class Script;
  LAVA_DISALLOW_COPY_AND_ASSIGN(ScriptBuilder);
};

inline ScriptBuilder::ScriptBuilder( const std::string& filename ,
                                     const std::string& source ):
  filename_(filename),
  source_  (source),
  main_    (),
  function_table_ ()
{}

inline void ScriptBuilder::set_main( const Handle<Prototype>& main ) {
  lava_debug(NORMAL,
      lava_verify(main_.IsNull());
      lava_verify(!main.IsNull());
    );
  main_ = main;
}

inline std::int32_t ScriptBuilder::AddPrototype( const Handle<Prototype>& handle ) {
  if(function_table_.size() == kMaxPrototypeSize)
    return -1;
  function_table_.push_back( Script::FunctionTableEntry(Handle<String>(),handle) );
  return static_cast<std::int32_t>(function_table_.size()-1);
}

inline std::int32_t ScriptBuilder::AddPrototype( GC* gc ,
                                                 const Handle<Prototype>& handle ,
                                                 const zone::String& name ) {
  if(function_table_.size() == kMaxPrototypeSize)
    return -1;
  lava_debug(NORMAL,lava_verify(!HasPrototype(name)););
  function_table_.push_back( Script::FunctionTableEntry(
        String::New(gc,name.data(),name.size()),handle) );
  return static_cast<std::int32_t>(function_table_.size()-1);
}

inline bool ScriptBuilder::HasPrototype( const zone::String& name ) const {
  return std::find_if(function_table_.begin(),function_table_.end(),
      [&]( const Script::FunctionTableEntry& e ) {
        if(e.name && *e.name == name.data()) return true;
        return false;
      }) != function_table_.end();
}

inline bool ScriptBuilder::HasPrototype( const Handle<String>& name ) const {
  return std::find_if(function_table_.begin(),function_table_.end(),
      [&]( const Script::FunctionTableEntry& e ) {
        if(e.name && *e.name == *name) {
          return true;
        }
        return false;
      }) != function_table_.end();
}

} // namespace lavascript

#endif // SCRIPT_BUILDER_H_
