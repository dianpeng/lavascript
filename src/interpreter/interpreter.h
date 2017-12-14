#ifndef INTERPRETER_H_
#define INTERPRETER_H_
#include "src/objects.h"

#include <string>

namespace lavascript {

class Context;
namespace interpreter{

// Abstract interface for interpreter which allow user to run a speicific code/script
class Interpreter {
 public:

  /**
   * Execute a script object inside of interpreter
   */
  virtual bool Run( Context* , const Handle<Script>& , const Handle<Object>& ,
                                                       Value* ,
                                                       std::string* ) = 0;

  /**
   * Execute a closure object inside of interpreter
   */
  virtual bool Run( Context* , const Handle<Closure>& , const Handle<Object>& ,
                                                        Value* ,
                                                        std::string* ) = 0;
 public:
  virtual ~Interpreter() {}
};

} // namespace interpreter
} // namespace lavascript


#endif // INTERPRETER_H_
