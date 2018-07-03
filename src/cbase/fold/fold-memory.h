#ifndef CBASE_FOLD_MEMORY_H_
#define CBASE_FOLD_MEMORY_H_
#include "folder.h"

#include "src/cbase/type.h"
#include "src/cbase/hir.h"
#include "src/cbase/aa.h"
#include "src/zone/zone.h"
#include "src/zone/stl.h"

#include "src/util.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

class MemoryFolder : public Folder {
 public:
  MemoryFolder( zone::Zone* zone ):ref_table_(zone) {}
  virtual bool CanFold( const FolderData& ) const;
  virtual Expr* Fold  ( Graph* , const FolderData& );
 private:
  // object
  Expr* Fold( Graph* , const ObjectFindFolderData&   );
  Expr* Fold( Graph* , const ObjectRefGetFolderData& );
  Expr* Fold( Graph* , const ObjectRefSetFolderData& );
  // list
  Expr* Fold( Graph* , const ListIndexFolderData&    );
  Expr* Fold( Graph* , const ListRefGetFolderData&   );
  Expr* Fold( Graph* , const ListRefSetFolderData&   );
  Expr* Fold( Graph* , const ExprFolderData& );

  template< typename Set , typename Get , typename T >
  Expr* StoreCollapse( Expr* , Expr* , WriteEffect* );

  template< typename Set , typename T >
  Expr* StoreForward ( Expr* , WriteEffect* );


  // helper function to do deep branch/split AA. The following function
  // will try to make AA cross branch generated EffectMerge node until hit
  // the outer most BranchStartEffect which is just a marker node. The
  // way it works is that it tries to do AA across *all* branches' effect
  // chain and return result :
  // 1) AA_MUST , all the branch start with the input EffectMerge has alias
  //    with the input memory reference , all branch is AA_MUST.
  //    To simplify problem, AA_MUST is treated same as AA_MAY, there're
  //    no aliased nodes been recored , so no forwarding and collapsing
  //    gonna be performed
  //
  // 2) AA_NOT  , all the branch start with the input EffectMerge doesn't
  //    have the alias with the input memory reference, ie all branch is
  //    AA_NOT
  //
  // 3) AA_MAY  , not 1) and not 2)
  //
  // This function help us to make our StoreCollapse and StoreForward work
  // cross the split , though it will not be as good as tracing JIT since
  // they essentially doesn't have branch at all
  struct Must {};

  struct BranchAA {
    int             result;             // normal AA result
    BranchStartEffect* end;

    BranchAA(                      ): result(AA::AA_MAY ),end(NULL) {}
    BranchAA( const Must&          ): result(AA::AA_MUST),end(NULL) {}
    BranchAA( BranchStartEffect* n ): result(AA::AA_NOT ),end(n)    {}
  };

  template< typename Set, typename Get, typename T >
  BranchAA StoreCollapseBranchAA( const FieldRefNode& , EffectMerge* );
  template< typename Set, typename Get, typename T >
  BranchAA StoreCollapseSingleBranchAA( const FieldRefNode& , WriteEffect* );

  template< typename Set, typename T >
  BranchAA StoreForwardBranchAA ( const FieldRefNode& , EffectMerge* );
  template< typename Set, typename T >
  BranchAA StoreForwardSingleBranchAA ( const FieldRefNode& , WriteEffect* );

 private:
  static const char* kObjectRef;
  static const char* kListRef;

  // For iterative value numbering of all the memory reference node. Memory
  // reference nodes will not participate GVN operations due to the side
  // effect it generates , ie any nodes inside of the effect chain will not
  // be part of GVN. But they can be numbering. Here the folder function will
  // do a special numbering of memory reference node and combine it with AA
  // to optimize out those redundant memory reference node.
  struct RefKey {
    Expr*           object;
    Expr*           key   ;
    EffectBarrier*  effect;
    StaticRef*      ref;
    const char*     ref_type;
    RefKey( StaticRef* , EffectBarrier* );
    RefKey( Expr* , Expr* , EffectBarrier* , const char* );
  };

  struct RefKeyHash {
    std::size_t operator() ( const RefKey& rk ) const;
  };

  struct RefKeyEqual {
    bool operator () ( const RefKey& l , const RefKey& r ) const;
  };

	typedef zone::stl::ZoneUnorderedSet<RefKey,RefKeyHash,RefKeyEqual> NumberTable;

  StaticRef* FindRef( Expr* , Expr* , WriteEffect* , TypeKind );
 private:
  NumberTable ref_table_;
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_FOLD_MEMORY_H_
