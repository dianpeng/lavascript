#ifndef CALL_FRAME_H_
#define CALL_FRAME_H_
#include "util.h"
#include "objects.h"

// interpreter
#include "interpreter/interpreter-runtime.h"
#include "interpreter/interpreter-frame.h"


namespace lavascript {

// Types of the call frame
enum CallFrameType {
  INTERPRETER_FRAME,
  JITTED_FRAME
};

// CallFrame object is an object that abstracts out the underlying frame of the function call
// when C++ side code interacts with interpreter/jitted code. Examples are like a script calls
// into C++ function or a jitted native code calls into C++ function. To correctly invoke C++
// function, we can either 1) use correct ABI to pass argument 2) use some method to transfer
// the underlying ABI to C++ ABI. The CallFrame is an object for the latter method.

class CallFrame {
 public:
  /** How many arguments are passed in from caller. */
  std::size_t GetArgumentSize() const {
    if(frame_type() == INTERPRETER_FRAME)
      return GetArgumentSize_Interp();
    else
      lava_die();
  }

  /**
   * Get argument for this CallFrame. The correct way to use it
   * is not letting index be out of the CallerArgumentSize()'s
   * return value
   */
  Value GetArgument( std::size_t index ) const {
    if(frame_type() == INTERPRETER_FRAME)
      return GetArgument_Interp(index);
    else
      lava_die();
  }

  /** Set the return value */
  void SetReturn( const Value& v ) {
    if(frame_type() == INTERPRETER_FRAME)
      return SetReturn_Interp(v);
    else
      lava_die();
  }

 public:
  // interpreter's runtime object. everything starts from interpretation
  interpreter::Runtime* interp_runtime() const { return interp_runtime_; }

  // caller's frame type
  CallFrameType frame_type() const { return frame_type_; }

  // the actual frame object's pointer, you should never use it directly
  void* frame() const { return frame_; }

  interpreter::IFrame* interp_frame() const {
    lava_debug(NORMAL,lava_verify(frame_type_ == INTERPRETER_FRAME););
    return reinterpret_cast<interpreter::IFrame*>(frame());
  }

 public:

  CallFrame( interpreter::Runtime* interp_runtime ,
             CallFrameType frame_type ,
             void* frame ):
    interp_runtime_(interp_runtime),
    frame_type     (frame_type),
    frame          (frame)
  {}

 private:
  // Implementation detail for interpreter frame
  inline std::size_t GetArgumentSize_Interp() const;
  inline Value       GetArgument_Interp    ( std::size_t ) const;
  inline void        SetReturn_Interp      ( const Value& );

  // TODO:: Add more when jit is implemented

 private:
  /** pointer to the interpreter's runtime object **/
  interpreter::Runtime* interp_runtime_;

  /** types of the frame **/
  CallFrameType frame_type_;

  /** pointer points to the start of the frame **/
  void* frame_   ;

  LAVA_DISALLOW_COPY_AND_ASSIGN(CallFrame)
};

inline std::size_t CallFrame::GetArgumentSize_Interp() const {
  return interp_frame()->narg();
}

inline Value CallFrame::GetArgument_Interp( std::size_t index ) const {
  lava_debug(NORMAL,lava_verify(index < GetArgumentSize_Interp()););
  return interp_runtime_->cur_stk[index];
}

inline void CallFrame::SetReturn_Interp( const Value& v ) {
  // interpreter pass return value via acc register
  static const std::size_t kAccIndex = 255;
  interp_runtime_->cur_stk[kAccIndex] = v;
}

} // namespace lavascript

#endif // CALL_FRAME_H_
