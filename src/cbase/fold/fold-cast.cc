#include "folder.h"
#include "src/cbase/hir.h"

namespace lavascript {
namespace cbase      {
namespace hir        {
namespace            {

class CastFolder : public Folder {
 public:
  CastFolder( zone::Zone* zone ) { (void)zone; }
  virtual bool CanFold( const FolderData& ) const;
  virtual Expr* Fold  ( Graph* , const FolderData& );
};

bool CastFolder::CanFold( const FolderData& data ) const {
  if(data.fold_type() == FOLD_EXPR) {
    auto d = static_cast<const ExprFolderData&>(data);
    return d.node->Is<Float64ToInt64>();
  }
  return false;
}

Expr* CastFolder::Fold( Graph* graph , const FolderData& data ) {
  auto to_i32 = static_cast<const ExprFolderData&>(data).node->As<Float64ToInt64>();
  auto val    = to_i32->value();
  if(val->Is<Unbox>()) {
    val = val->As<Unbox>()->value();
  }
  if(val->Is<Float64>()) {
    std::int64_t output = 0;
    if(TryCastReal(val->As<Float64>()->value(),&output)) {
      return Int64::New(graph,output);
    }
  }
  return NULL;
}

LAVA_REGISTER_FOLDER("cast-folder",CastFolderFactory,CastFolder);

} // namespace
} // namespace hir
} // namespace cbase
} // namespace lavascript
