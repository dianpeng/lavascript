#ifndef FEEDBACK_MANAGER_H_
#define FEEDBACK_MANAGER_H_

#include "src/util.h"
#include "src/trace.h"


namespace lavascript {
namespace feedback   {

/**
 * Feedback manager
 *
 * It manages the feedback slots used to do profiling when interpreter enters into the
 * profiling/record mode. Its whole purpose is to provide routine to let the interpreter
 * calls into to do the feedback collection.
 *
 * Each BC may need to collect some feedback . The collected data are stored inside of
 * FeedbackManager's slots. The slot is just hash table, the key is the address of the PC,
 * the value is data structure records the needed information for that PC. The feedback
 * cannot be collected *cross* calling boundary and we should never try to JIT code cross
 * call boundary.
 *
 */

class FeedbackManager {
 public:
};

} // namespace feedback
} // namespace lavascript

#endif // FEEDBACK_MANAGER_H_
