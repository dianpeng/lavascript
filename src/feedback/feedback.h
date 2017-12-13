#ifndef FEEDBACK_H_
#define FEEDBACK_H_
#include "src/util.h"

namespace lavascript {
class HeapObject;
namespace feedback   {

/** represent type for feedback **/
enum TypeFeedback {
  FB_GUESS_REAL,
  FB_GUESS_BOOLEAN,
  FB_GUESS_NULL,
  FB_GUESS_SSO,
  FB_GUESS_LIST,
  FB_GUESS_OBJECT,

  /** fixed means this is not a guess but must be. this are used
   *  when recording partially typed arithmetic/comparison BCs */
  FB_FIXED_REAL,
  FB_FIXED_STRING,  // this type means a general string, it can be SSO or not
  FB_FIXED_SSO,

  FB_DONT_CARE
};

struct BinaryFeedback {
  TypeFeedback lhs , rhs;
};

struct UnaryFeedback {
  TypeFeedback operand;
};

struct PropertyFeedback {
  TypeFeedback object_type;
  TypeFeedback index_type;
  HeapObject** object;
  HeapObject** index;
};

struct ForFeedback {
  TypeFeedback induct;
  TypeFeedback step  ;
  TypeFeedback cond  ;
};

struct CallFeedback {
  TypeFeedback call_type;
  HeapObject** call;
};

class Feedback {
 public:
  enum {
    BINARY_FEEDBACK,   // binary operation feedback
    UNARY_FEEDBACK ,   // unary operation feedback
    PROPERTY_FEEDBACK, // property operation feedback
    FOR_FEEDBACK,      // for loop feedback
    CALL_FEEDBACK      // call feedback
  };

 private:
  int type_;
  union {
    BinaryFeedback binary_;
    UnaryFeedback  unary_;
    PropertyFeedback property_;
    ForFeedback    forloop_;
    CallFeedback   call_;
  };
};


} // namespace feedback
} // namespace lavascript

#endif // FEEDBACK_H_
