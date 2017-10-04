#ifndef INTERPRETER_UPVALUE_H_
#define INTERPRETER_UPVALUE_H_

namespace lavascript {
namespace interpreter{

/**
 * UpValue management
 * UpValue is a value that is not inside of a function's lexical scope
 * but in its upper level enclosed function scope. We use a similar startegy
 * with Lua. Basically just collapse the value along with each closure's
 * upvalue slots
 *
 * UpValue has 2 states , if its state is *EMBED* , which means the UpValue
 * should be retrieved in register/stack slot ; other state is *DETACH* ,
 * which means the UpValue should be retrieved in its enclosed function's
 * upvalue slot
 */
enum UpValueState {
  UV_EMBED,
  UV_DETACH
};

inline const char* GetUpValueStateName( UpValueState st ) {
  if(st == UV_EMBED)
    return "embed";
  else
    return "detach";
}

} // namespace interpreter
} // namespace lavascript

#endif // INTERPRETER_UPVALUE_H_
