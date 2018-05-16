#ifndef CBASE_MEMORY_H_
#define CBASE_MEMORY_H_
#include "src/zone/zone.h"
#include "src/zone/stl.h"
#include "src/cbase/hir.h"

#include <unordered_set>

namespace lavascript {
namespace cbase      {
namespace hir        {


// Helper class to do memory related optimization during HIR construction. It do
// simple alias analyzing,dedup (gvn cannot do this since it has side effect) and
// also store forwarding.
class MemoryOpt{
 public:
  // Alias Analyzer
  class AA;

  MemoryOpt( zone::Zone* zone );

  enum {
    DEAD ,    // this operation is dead and has been optimized out, do nothing
    FOLD ,    // this operation turns into another node already existed
    FOLD_REF, // this operation's reference turns into antoher folded reference
    FAILED    // this operation cannot be performed
  };

  // Optimization result
  struct Result {
    int   state;
    Expr* value;
    Result( int s , Expr* v ) : state(s) , value(v) {}
    static Result kDead;
    static Result kFailed;
  };

  // Optimize for a object set operation. The input object must be in guarded mode
  // with a type guard against TPKIND_OBJECT. The optimizer may fold two consecutive
  Result OptObjectSet( Graph* , Expr* , Expr* , Expr* , WriteEffect* ,
                                                        Checkpoint*  );
 private:
  static const char* kObjectRef;
  static const char* kListRef;
  // reference key
  struct RefKey {
    Expr*        object;
    Expr*        key   ;
    WriteEffect* effect;
    Checkpoint*  checkpoint;
    MemoryRef*   ref;
    const char*  ref_type;
    RefKey(MemoryRef* );
    RefKey(Expr* , Expr* , WriteEffect* , Checkpoint* , const char* );
  };

  struct RefKeyHasher {
    std::size_t operator() ( const RefKey& rk ) const;
  };

  struct RefKeyEqual {
    bool operator () ( const RefKey& l , const RefKey& r ) const;
  };
  typedef std::unordered_set<RefKey,RefKeyHasher,RefKeyEqual> NumberTable;

  struct RefPos {
    MemoryRef* ref; WriteEffect* effect;
    RefPos(MemoryRef* r, WriteEffect* e):ref(r),effect(e) {}
    RefPos():ref(),effect() {}
  };

  RefPos FindRef( Expr* , Expr* , WriteEffect* , Checkpoint* , AA* );
 private:
  NumberTable ref_table_;
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_MEMORY_H_
