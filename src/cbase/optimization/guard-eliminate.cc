#include "guard-eliminate.h"
#include "src/cbase/dominators.h"
#include "src/cbase/type.h"
#include "src/zone/zone.h"
#include "src/trace.h"


namespace lavascript {
namespace cbase      {
namespace hir        {
using namespace ::lavascript;

#if 0
namespace            {
// Guard elimination is a pass to eliminate unneeded guard operation. The elimination doesn't
// do any infer operation but just check whether its dominator node has same guard there or not.
// If it has , then this guard is useless and we will just remove this guard.
//
// This algorithm does a forward scan with RPO for the graph , the result depends on the RPO
// order and since the algorithm is monoton so it is guaranteed to be correct.

// TODO:: Add implementation for category size object
class SizeData {};

// Guard annotation object represents/annotate a certain guard node's value. This value can
// contain different category like type assert and size of builtin types
class GuardAnnotation : public zone::ZoneObject {
 public:
  // order matters
  enum { CAT_TYPE = 0 , CAT_SIZE };
  int category () const { return category_; }
  bool is_type () const { return category() == CAT_TYPE; }
  bool is_size () const { return category() == CAT_SIZE; }
 public:
  TypeKind type_kind() const {
    lava_debug(NORMAL,lava_verify(is_type()););
    return type_kind_;
  }

  const SizeData& size_data() const {
    lava_debug(NORMAL,lava_verify(is_size()););
    return *size_data_;
  }

 public:
  GuardAnnotation( TypeKind type  ) : size_data_() , category_(CAT_TYPE) { type_kind_ = type; }
  GuardAnnotation( SizeData* size ) : size_data_() , category_(CAT_SIZE) { size_data_ = size; }

  // Factory method , if it cannot derive a GuardAnnotation from the condition input, then it
  // just returns NULL. Regardless of what you can store the result back
  static GuardAnnotation* Create( zone::Zone* , Expr* );

 public: // comparison operation
  inline bool operator == ( const GuardAnnotation& ) const;
  inline bool operator != ( const GuardAnnotation& ) const;

 private:
  union {
    SizeData* size_data_;
    TypeKind  type_kind_;
  };
  int category_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(GuardAnnotation);
};

GuardAnnotation* GuardAnnotation::Create( zone::Zone* zone , Expr* tester ) {
  if(tester->IsTestType()) {
    auto tt = tester->AsTestType();
    return zone->New<GuardAnnotation>(tt->type_kind());
  }
  return NULL;
}

inline bool GuardAnnotation::operator == ( const GuardAnnotation& that ) const {
  if(this == &that ) return true;

  if(that.category_ == category_) {
    if(is_type()) return type_kind_ == that.type_kind_;
    // TODO:: add is_size() implementation
  }
  return false;
}

inline bool GuardAnnotation::operator != ( const GuardAnnotation& that ) const {
  return !(*this == that);
}

} // namespace

bool GuardEliminate::Perform( Graph* graph , HIRPass::Flag flag ) {
  (void)flag;
  zone::Zone zone;
  zone::OOLVector<GuardAnnotation*> vec(&zone,graph->MaxID());
  zone::Vector<GuardAnnotation*>    del(&zone);
  Dominators dom; dom.Build(*graph);

  // 1. pass to mark guard/if redundant -------------------------------------------
  // RPO traversal
  for( ControlFLowRPOIterator itr(*graph); itr.HasNext(); itr.Move() ) {
    auto cf = itr.value(); // control flow , check whether it can be added
    if(cf->IsIf() || cf->IsGuard()) {
      auto idom = dom.GetImmDominator(cf); // get the dominator
      auto cond = cf->IsIf() ? cf->AsIf()->condition() : cf->AsGuard()->test();
      // create an annotation
      auto ann  = GuardAnnotation::Create(&zone,cond);
      if(ann) {
        auto pann = vec.Get(&zone,idom->id());
        if(pann && *ann == *pann) {
          del.Add(&zone,cf); // should be removed
        }
      }
    }
  }


}

#endif


} // namespace hir
} // namespace cbase
} // namespace lavascript
