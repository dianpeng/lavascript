#ifndef CBASE_IR_HELPER_H_
#define CBASE_IR_HELPER_H_
#include "hir.h"

namespace std {

// Helper template to make hir::Expr to be able to be used with std::unordered_map
template<> struct hash<::lavascript::cbase::hir::Expr*> {
  typedef ::lavascript::cbase::hir::Expr* type;
  std::size_t operator () ( const type& expr ) const {
    return static_cast<std::size_t>(expr->GVNHash());
  }
};

template<> struct equal<::lavascript::cbase::hir::Expr*> {
  typedef ::lavascript::cbase::hir::Expr* type;
  bool operator ( const type& lhs , const type& rhs ) const {
    return lhs->Equal(rhs);
  }
};

} // namespace std

namespace lavascript {
namespace cbase {
namespace hir   {

/**
 * Several type traits check for HIR node
 */
inline bool HIRIsPrimitive( const Expr* node ) {
  switch(node->type()) {
    case IRTYPE_FLOAT64:
    case IRTYPE_SSTRING:
    case IRTYPE_LSTRING:
    case IRTYPE_BOOLEAN:
    case IRTYPE_NIL:
      return true;
    default:
      return false;
  }
}

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_IR_HELPER_H_
