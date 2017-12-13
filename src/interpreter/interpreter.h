#ifndef INTERPRETER_H_
#define INTERPRETER_H_
#include "src/objects.h"

#include <string>

namespace lavascript {
namespace feedback {
class FeedbackManager;
} // namespace feedback

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

  // returns the feedback manager, if this interpreter doesn't support feedback
  // or jit , then just return NULL
  virtual feedback::FeedbackManager* feedback_manager() const { return NULL; }

 public:
  virtual ~Interpreter() {}
};

} // namespace interpreter
} // namespace lavascript


#endif // INTERPRETER_H_
