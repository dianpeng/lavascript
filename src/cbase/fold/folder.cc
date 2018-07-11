#include "folder.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

void FolderFactory::RegisterFactory( const char* name , std::unique_ptr<FolderFactory>&& ff ) {
  auto &l = GetFolderFactoryEntryList();
  l.emplace_back(name,std::move(ff));
}

FolderFactory::FolderFactoryEntryList& FolderFactory::GetFolderFactoryEntryList() {
  // local variable are thread safe by C++11 standard
  static FolderFactoryEntryList kList;
  return kList;
}

FolderChain::FolderChain( zone::Zone* zone ) : zone_(zone) , chain_() {
  for( auto &e : FolderFactory::GetFolderFactoryEntryList() ) {
    lava_debug(NORMAL,lava_info("Folder algorithm %s registered",e.name.c_str()););
    chain_.push_back(e.factory->Create(zone_));
  }
}

Expr* FolderChain::Fold( Graph* graph , const FolderData& data ) {
  // Walk the folder chain until one of them can fold it or we cannot
  // fold the input data, then just return a NULL
  //
  // TODO:: allow recursive folding ?
  for( auto &e : chain_ ) {
    if(e->CanFold(data)) {
      if(auto expr = e->Fold(graph,data); expr) {
        return expr;
      }
    }
  }
  return NULL;
}

} // namespace hir
} // namespace cbase
} // namespace lavascript
