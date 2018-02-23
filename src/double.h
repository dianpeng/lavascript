#ifndef DOUBLE_H_
#define DOUBLE_H_

#include <limits>

/**
 * Helper functions or wrapper around awesome std::numeric_limits to provide
 * specific requirements of double precision floating point number
 */

namespace lavascript {

// we need the double to conform IEEE754 standard which has well defined
// positive infinity and negative infinity

static_assert( std::numeric_limits<double>::is_iec559 );

class Double {
 public:
  static constexpr double PosInf() { return std::numeric_limits<double>::infinity(); }
  static constexpr double NegInf() { return -PosInf(); }

  static bool IsPosInf( double value ) { return value == PosInf(); }
  static bool IsNegInf( double value ) { return value == NegInf(); }
  static bool IsInf   ( double value ) { return IsPosInf(value) || IsNegInf(value); }

  // the standard's min() for double is error prone which represents the smallest
  // positive number can be represented by double precision floating point
  static constexpr double Min()    { return std::numeric_limits<double>::lowest();  }
  static constexpr double Max()    { return std::numeric_limits<double>::max();     }
};

} // namespace lavascript

#endif // DOUBLE_H_
