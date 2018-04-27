#ifndef CBASE_EFFECT_H_
#define CBASE_EFFECT_H_
#include "hir.h"
#include "src/zone/vector.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

class RootEffectGroup;
class LeafEffectGroup;

/**
 * Helper module to do effect analyzing during graph construction.
 *
 * Currently , after many times of rewrite , we stick with this mode or a relative simpler mode.
 * We use type information to help us do alias analyzing.
 *
 *              |---> { root } <---|
 *              |                  |
 *             \|/                \|/
 *           { List }           { Object }
 *
 * As you can see , there're 3 effect group, list , object and root. User can get dependency
 * information at any site. The most conservative way is to get dependency information from the
 * root , which basically means the dependency is maintained globally as if every memory access
 * is accessing the same memory region. If user are sure about the memory type is List or Object,
 * then user can get dependency from List or Object group, which makes List and Object's memory
 * access totoally unorder since we are sure List type and Object type are not aliased. And modify
 * List and Object will propogate its dependency modification back to root which means List and
 * Object are aliased with root.
 *
 * This at least gives us some sort of alias analysis which enables some optimization during graph
 * construction and also maintain simplicity
 */
class EffectGroup {
 public:
  EffectGroup( ::lavascript::zone::Zone* , WriteEffect* );
  EffectGroup( const EffectGroup& );
 public:
  // update the write effect for this effect object
  virtual void UpdateWriteEffect( WriteEffect* ) = 0;
  // add a new read effect
  virtual void AddReadEffect    ( ReadEffect*  ) = 0;
 public:
  WriteEffect*                                write_effect() const { return write_effect_; }
  void                      set_write_effect( WriteEffect* );
  const ::lavascript::zone::Vector<ReadEffect*>& read_list() const { return read_list_; }
  ::lavascript::zone::Zone*                           zone() const { return zone_; }
 protected:
  // concrete implementation of Update/Add without propogation of the
  // effect into related group
  void DoUpdateWriteEffect( WriteEffect* );
  void DoAddReadEffect    ( ReadEffect * );

 private:
  WriteEffect*                             write_effect_;   // Write effect currently tracked
  ::lavascript::zone::Vector<ReadEffect*>  read_list_;      // All the read happened *after* the write
  ::lavascript::zone::Zone*                zone_;           // Zone object for allocation of memory

  friend class RootEffectGroup;
  friend class LeafEffectGroup;
};

class RootEffectGroup : public EffectGroup {
 public:
  RootEffectGroup( ::lavascript::zone::Zone* , WriteEffect* );
  RootEffectGroup( const RootEffectGroup& );

  void set_list  ( EffectGroup* eg ) { list_ = eg; }
  void set_object( EffectGroup* eg ) { object_ = eg; }

  virtual void UpdateWriteEffect( WriteEffect* );
  virtual void AddReadEffect    ( ReadEffect * );
 private:
  EffectGroup* list_ , *object_;   // list/object children
};

class LeafEffectGroup : public EffectGroup {
 public:
  LeafEffectGroup( ::lavascript::zone::Zone* , WriteEffect* );
  LeafEffectGroup( const LeafEffectGroup& );

  void set_parent( EffectGroup* eg ) { parent_ = eg; }

  virtual void UpdateWriteEffect( WriteEffect* );
  virtual void AddReadEffect    ( ReadEffect * );
 private:
  EffectGroup* parent_;
};

class Effect {
 public:
  Effect( ::lavascript::zone::Zone* , WriteEffect* );
  Effect( const Effect& );
  // accessor for different types of effect group
  EffectGroup*       root  ()       { return &root_;   }
  EffectGroup*       list  ()       { return &list_;   }
  EffectGroup*       object()       { return &object_; }
  const EffectGroup* root  () const { return &root_;   }
  const EffectGroup* list  () const { return &list_;   }
  const EffectGroup* object() const { return &object_; }
 public:
  // merge the input 2 effect object into another effect object, the input and output can be the same
  static void Merge( const Effect& , const Effect& , Effect* , Graph*  , ControlFlow* );
 private:
  RootEffectGroup root_;
  LeafEffectGroup list_;
  LeafEffectGroup object_;
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_EFFECT_H_
