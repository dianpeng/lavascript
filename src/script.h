#ifndef SCRIPT_H_
#define SCRIPT_H_
#include "common.h"
#include "objects.h"
#include "trace.h"


namespace lavascript {
class Context;

/**
 * Script object is an object that holds all the compiled code or
 * function object. Each script object will match a certain file
 * name to avoid recompiling everytime when a script is loaded
 */
class Script {
 public:
  inline Script( Context* context, const std::string& filename ,
                                   const std::string& source );

 public:
  const std::string& source() const { return source_; }
  const std::string& filename() const { return filename_; }
  Context* context() const { return context_; }

 public:
  /**
   * Add a main function into the script object. A script can
   * only have one main function and its index is always 0.
   */
  inline void set_main( const Handle<Prototype>& main );
  inline const Handle<Prototype>& main() const { return main_; }

  /**
   * Add a certain function into the Script object. Each function
   * added into Script object will be assigned an index which is
   * used to mark where the function is. Later on if a loading
   * happened, use this index to reference a function.
   */
  inline std::int32_t AddFunction( const Handle<Prototype>& function );
  inline const Handle<Prototype>& IndexFunction( std::int32_t ) const;

 private:
  std::string filename_;
  std::string source_;
  Handle<Prototype> main_;
  Handle<Object> function_table_;
  Context* context_;
};

inline Script::Script( Context* context , const std::string& filename ,
                                          const std::string& source ):
  filename_(filename),
  source_  (source),
  main_    (),
  function_table_ (),
  context_ (context)
{}

inline void Script::set_main( const Handle<Prototype>& main ) {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(main_.IsNull());
  lava_verify(!main.IsNull());
#endif // LAVASCRIPT_CHECK_OBJECTS
  main_ = main;
  function_table_.insert( function_table_.begin() , main );
}

inline std::int32_t AddFunction( const Handle<Prototype>& function ) {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(!function.IsNull());
#endif // LAVASCRIPT_CHECK_OBJECTS
  if(function_table_.size() == kMaxPrototypeCount)
    return -1;
  else {
    function_table_.push_back(function);
    return static_cast<std::int32_t>(function_table_.size()-1);
  }
}

inline const Handle<Prototype>& Script::IndexFunction( std::int32_t index ) const {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(index >= 0 && static_cast<std::size_t>(index) < function_table_.size());
#endif // LAVASCRIPT_CHECK_OBJECTS
  return function_table_[index];
}

} // namespace lavascript

#endif // SCRIPT_H_
