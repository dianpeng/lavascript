#include "fold-arith.h" // fold ternary
#include "folder.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

namespace {

class PhiFolder : public Folder {
 public:
  PhiFolder( zone::Zone* zone ) { (void)zone; }

  virtual bool CanFold( const FolderData& data ) const;
  virtual Expr* Fold    ( Graph* , const FolderData& );
 private:
  Expr* Fold( Graph* , Expr* , Expr* , ControlFlow* );
  Expr* Fold( PhiNode* );
};

LAVA_REGISTER_FOLDER("phi-folder",PhiFolderFactory,PhiFolder);

Expr* PhiFolder::Fold( Graph* graph , const FolderData& data ) {
  if(data.fold_type() == FOLD_PHI) {
    auto d = static_cast<const PhiFolderData&>(data);
    return Fold(graph,d.lhs,d.rhs,d.region);
  } else {
    auto d = static_cast<const ExprFolderData&>(data);
    return Fold(d.node->As<PhiNode>());
  }
}

bool PhiFolder::CanFold( const FolderData& data ) const {
  if(data.fold_type() == FOLD_PHI) {
    return true;
  } else if(data.fold_type() == FOLD_EXPR) {
    auto d = static_cast<const ExprFolderData&>(data);
    return d.node->Is<PhiNode>();
  }
  return false;
}

Expr* PhiFolder::Fold( Graph* graph , Expr* lhs , Expr* rhs ,ControlFlow* region ) {
  // 1. if lhs and rhs are same, then just return lhs/rhs
  if(lhs->Equal(rhs)) {
    return lhs;
  }
  if(region->IsIf()) {
    auto inode = region->AsIf();
    // 2. try to fold it as a ternary if the cond is side effect free
    auto cond = inode->condition(); // get the condition
    auto    n = FoldTernary(graph,cond,lhs,rhs);
    if(n) return n;
  }
  return NULL;
}

Expr* PhiFolder::Fold( PhiNode* phi ) {
  if(phi->operand_list()->size() == 2) {
    auto lhs = phi->operand_list()->First();
    auto rhs = phi->operand_list()->Last();
    if(lhs->Equal(rhs)) return lhs;
  }
  return NULL;
}

} // namespace
} // namespace hir
} // namespace cbase
} // namespace lavascript
