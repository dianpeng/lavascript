#ifndef CBASE_IR_STL_HELPER_H_
#define CBASE_IR_STL_HELPER_H_
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

#endif // CBASE_IR_STL_HELPER_H_
