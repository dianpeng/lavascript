#ifndef CBASE_CONDITION_GROUP_H_
#define CBASE_CONDITION_GROUP_H_
#include "sparse-map.h"
#include <memory>

namespace lavascript {
namespace cbase      {
namespace hir        {

class Expr;
class ValueRange;

/**
 * A condition group is a object that contains all the branch's condition's value range
 * correspondingly. It is revolve around a bunch variables that involve inside of the
 * condition construction.
 *
 * For any unique none constant part of the sub expression that involves inside of a
 * branch condition , it will have a corresponding ValueRange object to be used to do
 * ineference.
 *
 * 1) If the none constant part of the sub expression *only* has one/multiple constant part
 *    and the constant part is a float64 number, then it will have a Float64ValueRange to
 *    be used for inference.
 *
 * 2) If the none constant part of the sub expression *only* has one/multiple boolean part
 *    then it will have a BooleanValueRange to be used for inference
 *
 * 3) Any other types of expression will be marked to use *UnknownValueRange* which simply
 *    doesn't know anything at all
 *
 */

class ConditionGroup {
 public:
  ConditionGroup();
  ~ConditionGroup();

  // Indicate that this condition group object is not initialized yet
  bool IsEmpty() const { return range_.empty(); }

  // Size of the sub expression pair inside of this group
  std::size_t size() const { return range_.size(); }

 private:
  SparseMap<Expr*,std::unique_ptr<ValueRange>> range_;
};

} // namespace hir
} // namespace cbase
} // namespace lavascript


#endif // CBASE_CONDITION_GROUP_H_
