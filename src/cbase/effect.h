#ifndef CBASE_EFFECT_H_
#define CBASE_EFFECT_H_
#include "hir.h"
#include "src/zone/zone.h"

#include <functional>

/**
 * Helper module to do effect analyzing during graph construction.
 *
 * We use effect group object to track the dependency while generating nodes inside of our HIR
 * graph. The dependency type we track are as follow : 1) true dependency (read-after-write)
 * and 2) anti dependency (write-after-read). To track the dependency, for each write happened
 * at a certain effect group it is served as a barrier basically it flushes all the read happened
 * before this write by explicit mark this write depend on all the previous happened write ; for
 * each read , it will adds this node into the effect group and also mark this read depend on
 * previously happened write operation.
 *
 * The effect group can alias with other effect group due to assignment happened. To represent
 * this we must define what types of memory alias can happen inside of lavascript:
 *
 * 1) none alias , basically the memory is *not* overlapped with each other
 * 2) contain    , basically a memory contains a reference points to another memory
 *
 * To effectively represent effect order, we need to address contain.
 * Each effect group represents *all* side effect happened wrt a specific memory. If a memory is
 * assigned to be contained by another memory, suppose S1 is contained with S2 , then we maintain
 * such effect group invariants :
 *
 * 1. any modification happened at S2 can be observed by S1
 * 2. any modification happened at S1 can be observed by S2
 * 3. Effect(S2) >= Effect(S1)  (S2 can have other effect already presents but not modified via S1)
 *
 * But if name is explicitly states that the effect happen at S2 , then this name can only observe
 * side effect happened on S2's side effect but not S1 ; but if a name is states that the effect
 * should be taken at S1 then effect happened at S1 and S2 can be observed.
 *
 * One thing to note that the effect group is not a *tree* but a *graph*, the following example
 * forms a cyclic effect group graph:
 *
 * var a = { "b" : { "c" : 10 } };
 * a.b.c = a; // forms a effect group cycle
 *
 */

namespace lavascript {
namespace cbase      {

class EffectGroup;
class BasicEffectGroup;
class COWEffectGroup;
class RootEffectGroup;
class MergeEffectGroup;

// Callback function to get the latest root effect group. Used by RootEffectGroup and EffectGroupFactory
typedef std::function<BasicEffectGroup*()> RootEffectGroupGetter;

// Visitor for visiting an expression's effect group in an optimal order
typedef std::function<void (EffectGroup*)> ExprEffectVisitor;

// A helper class to manage the creation of each types of effect group and maintain a internal
// counter to number each created effect group and mark them uniquely
class EffectGroupFactory {
 public:
  static const std::uint32_t kRootEffectGroupID = 0; // only root effect group has this number
  // Make sure the getter callback outlive the life span of EffectGroupFactory
  inline EffectGroupFactory( ::lavascript::zone::Zone* , const RootEffectGroupGetter& );
 public:
  std::uint32_t          counter() const { return counter_; }
  ::lavascript::zone::Zone* zone() const { return zone_; }

 public:
  inline BasicEffectGroup* NewBasicEffectGroup( MemoryWrite* );
  inline COWEffectGroup*   NewCOWEffectGroup  ( const EffectGroup* );
  inline COWEffectGroup*   NewCOWEffectGroup  ( MmeoryWrite* );
  inline MergeEffectGroup* NewMergeEffectGroup( NoWriteEffect* , EffectGroup* , EffectGroup* );
  inline RootEffectGroup*  NewRootEffectGroup () { return root_effect_group_; }

 private:
  std::uint32_t counter_;                            // used to mark each effect group
  ::lavascript::zone::Zone* zone_;                   // zone used to allocate each group
  RootEffectGroup* root_effect_group_;               // cached root effect group, we only need to maintain one
                                                     // root effect group during the IR graph construction

  LAVA_DISALLOW_COPY_AND_ASSIGN(EffectGroupFactory)
};

// EffectGroup represents a simple node in program's effect analyzing. It can have *Slice* object
// internally which represents partially some of the memory points to another memory effect group,
// basically a *contain* relationship. A modification happened on the higher level effect group will
// propogate to lower level effect group but not vise versa.
class EffectGroup : public ::lavascript::zone::ZoneObject {
 public:
  // Numbering effect group, used to track visiting status of each EffectGroup object
  std::uint32_t id() const { return id_; }

 public:
  // Add a read effect for this memory read operation. This operation should typically
  // properly configure the input read node's dependency and also add it into internal
  // read list to ensure later anti-dependency is setup correctly
  virtual void AddReadEffect    ( MemoryRead*  ) = 0;
  // Update the bounded write effect internally
  virtual void UpdateWriteEffect( MemoryWrite* ) = 0;
 public: // Chaining effect group
  // Used when an assignment to a sub field of this effect group represented memory
  virtual void AssignEffectGroup( Expr* , EffectGroup* ) = 0;
  // Used to resolve an effect group based on the key to select a Slice object
  virtual EffectGroup* Resolve( Expr* ) const = 0;
 public: // raw setters/getters , help to do clone when COW is used
  // Slice and Children list. The Slice represents a cleared sub region of the current
  // memory region which is used to enable finer grained effect analysis. Put it in plain
  // english, it is always safe to return Parental node for a certain key for effect,
  // since parental node will cover all child node's effect in a serialized fashion, but
  // if we can return child node wrt certain key node, we will have simplier effect chain
  // for dependency list and have more potentail for optimization.
  struct Slice : public ::lavascript::zone::ZoneObject {
    Expr* key;        // key used to check whether a correct slice can be matched, to compare
                      // a key , we use IsEqual inside of Expr function which is used for GVN,
                      // this function is fine here because it will automatically fails when
                      // node has side effect
    EffectGroup* grp; // attached effect group object

    Slice( Expr* k , EffectGroup* g ) : key(k) , grp(g) {}
    Slice(): key(NULL), grp(NULL) {}
  };

  virtual ::lavascript::zone::Zone* zone()         const = 0;
  virtual MemoryWrite*              write_effect() const = 0;
  virtual std::size_t               read_size()    const = 0;
  virtual std::size_t               children_size()const = 0;

  typedef std::function<void (MemoryRead*)> ReadVisitor;
  typedef std::function<void (Slice*)     > ChildrenVisitor;

  // Here the interface is exposed as callback style instead of returning the
  // internal list due to the fact that this is more abstracted and easier to
  // implement efficient merged effect group. Another way will be expose the
  // list as iterator.
  virtual void VisitRead    ( const ReadVisitor&     ) const = 0;
  virtual void VisitChildren( const ChildrenVisitor& ) const = 0;

 public:
  EffectGroup ( std::uint32_t id ) : id_(id) {}
  virtual ~EffectGroup() {}

 public: // Helper function for visiting each children starting from certain EffectGroup
  template< typename T >
  void DoVisit( ::lavascript::zone::Zone* , ::lavascript::zone::OOLVector<bool>* , EffectGroup* , const T& );

 private:
  std::uint32_t id_;
};

// The most basic effect group and it doesn't support effcient copy mechanism which is required
// to implement effecient environment setup during graph construction. The actual used effect group
// is always wrapper around this BasicEffectGroup object
class BasicEffectGroup : public EffectGroup {
 public:
  BasicEffectGroup( std::uint32_t , ::lavascript::zone::Zone* , MemoryWrite* );
 public:
  virtual ::lavascript::zone::Zone* zone()         const { return zone_; }
  virtual MemoryWrite*              write_effect() const { return write_effect_; }
  virtual std::size_t               read_size()    const { return read_list_.size(); }
  virtual std::size_t               children_size()const { return children_.size(); }

  virtual void VisitRead    ( const ReadVisitor&     ) const;
  virtual void VisitChildren( const ChildrenVisitor& ) const;
 public:
  virtual void AddReadEffect    ( MemoryRead*  );
  virtual void UpdateWriteEffect( MemoryWrite* );
 public:
  virtual void AssignEffectGroup( Expr* , EffectGroup* );
  virtual EffectGroup* Resolve  ( Expr* ) const;
 public:
  // Copy an effect group to this effect group
  void CopyFrom( const EffectGroup& );
 private:
  typedef std::function<void (EffectGroup*)> Visitor;
  // Recursively visiting effect group start from *this* , this function will track cycle to
  // avoid dead loop
  void Visit  ( const Visitor& );
 private:
  MemoryWrite*                            write_effect_;
  ::lavascript::zone::Vector<MemoryRead*> read_list_;
  ::lavascript::zone::Vector<Slice>       children_;
  ::lavascript::zone::Zone*               zone_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(BasicEffectGroup)
};

// A wrapper around effect group object which supports COW semantic. This object is used to
// track any none global effect group , basically any IRList and IRObject node created memory's
// effect is tracked via COWEffectGroup
class COWEffectGroup : public EffectGroup {
 public:
  COWEffectGroup( std::uint32_t id , ::lavascript::zone::Zone* , MemoryWrite* );

  // COW style copy constructor , needs to be called explicitly
  COWEffectGroup( std::uint32_t id , const EffectGroup* );

 public:
  bool               copy         () const { return copy_; }
  const EffectGroup* prev         () const { return prev_; }
  const EffectGroup* effective_ptr() const { return read_ptr(); }

  virtual ::lavascript::zone::Zone* zone()          const { return read_ptr()->zone(); }
  virtual MemoryWrite*              write_effect()  const { return read_ptr()->write_effect(); }
  virtual std::size_t               read_size()     const { return read_ptr()->read_size(); }
  virtual std::size_t               children_size() const { return read_ptr()->children_size(); }

  virtual void VisitRead( const ReadVisitor& visitor ) const {
    read_ptr()->VisitRead(visitor);
  }
  virtual void VisitChildren( const ChildrenVisitor& visitor ) const {
    read_ptr()->VisitChildren(visitor);
  }
 public:
  virtual void AddReadEffect    ( MemoryRead* effect )           { write_ptr()->AddReadEffect(effect); }
  virtual void UpdateWriteEffect( MemoryWrite*effect )           { write_ptr()->UpdateWriteEffect(effect); }
  virtual void AssignEffectGroup( Expr* key , EffectGroup* grp ) { write_ptr()->AssignEffectGroup(key,grp); }
  virtual EffectGroup* Resolve  ( Expr* key ) const              { return read_ptr()->Resolve(key); }
 private:
  const EffectGroup* read_ptr () const { return copy_ ? &(native_) : prev_; }
  // lazily copy the prev into the native effect group
  EffectGroup*       write_ptr() {
    if(!copy_) { copy_ = true; native_.CopyFrom(*prev_); }
    return &native_;
  }
 private:
  BasicEffectGroup   native_;
  const EffectGroup* prev_ ;
  bool               copy_ ;

  LAVA_DISALLOW_COPY_AND_ASSIGN(COWEffectGroup)
};

// A wrapper around the *root* effect group. Root effect group will be switched when a new environment
// is created. This wrapper takes a callback function from GraphBuilder to get the correct root effect
// group every time certain function is invoked internally.
class RootEffectGroup : public EffectGroup {
 public:
  RootEffectGroup( const RootEffectGroupGetter& getter ):
    EffectGroup( EffectGroupFactory::kRootEffectGroupID ) , getter_(getter) {}

 public: // public interface
  virtual ::lavascript::zone::Zone* zone() const { return getter_()->zone(); }
  virtual MemoryWrite*      write_effect() const { return getter_()->write_effect(); }
  virtual std::size_t       read_size   () const { return getter_()->read_size(); }
  virtual std::size_t       children_size()const { return getter_()->children_size(); }

  virtual void VisitRead( const ReadVisitor& visitor ) const {
    return getter_()->VisitRead(visitor);
  }
  vritual void VisitChildren( const ChildrenVisitor& visitor ) const {
    return getter_()->VisitChildren(visitor);
  }
  virtual void AddReadEffect    ( MemoryRead* effect )           { getter_()->AddReadEffect(effect); }
  virtual void UpdateWriteEffect( MemoryWrite* effect )          { getter_()->UpdateWriteEffect(effect); }
  virtual void AssignEffectGroup( Expr* key , EffectGroup* grp ) { getter()->AssignEffectGroup(key,grp); }
  virtual EffectGroup* Resolve  ( Expr* key ) const              { return getter_()->Resolve(key); }
 private:
  RootEffectGroupGetter getter_; // callback function

  LAVA_DISALLOW_COPY_AND_ASSIGN(RootEffectGroup)
};

// Used to represent a merged effect group at certain point. The merged group represent following
// example :
//
// var a = [];
// if(b) {
//   a[1] = {};
// } else {
//   a[1] = [];
// }
//
// a[1] = MergeEffectGroup({},[])
//
// This merged effect group represent an effect group equals the union of input effect groups. It
// means any read/write happened at MergeEffectGroup should be proplery linearlized against both
// 2 input effect groups ; any read/write happened at one of the input effect group should also
// impact any read/write at the merged effect group object. The 2 input effect groups are not the
// contain relationship but essentially they are composed together to form this effect group. And
// since the merge effect group represents something that unknown at compiled time due to the branch
// so all the resolve and assign effect group (alias) will not work. This is the most conservative
// assumption.
//
// NOTES:
// Currently we don't generate effect phi for any read/write due to the complexity and heavy memory
// usage. The phi node is good because it participate in any optimization pass and can be resolved
// during phase like DCE , so we will have reduction in effect list for each node. But it requires
// us to generate a Read/WriteEffectPhi node for any read/write happened at MergeEffectGroup, this
// is kind of complicated, basically we must collect lhs and rhs effect group's effect recursively
// and then merge these 2 end result into a Phi node and attach it back. This is definitly doable but
// just make our dependency list becomes a list of list recursively , which doesn't sound very simple
// in terms of IR graph.
//
// So instead of manually remove those effect phi node when DCE kicks in , we will run a pass to detect
// all the reachable expression by 1)data dependency 2) statement list and for any node appear in
// effect list of node as long as it is not reachable , it is treated as dead dependency and silently
// ignored.
class MergeEffectGroup : public EffectGroup {
 public:
  MergeEffectGroup( ::lavascript::zone::Zone* , NoWriteEffect* , EffectGroup* , EffectGroup* );

 public:
  virtual ::lavascript::zone::Zone* zone() const { return zone_; }
  // This returns an empty write effect simply beacuse we don't care the write_effect generated by
  // this effect group. This effect group does nothing except forwarding any read/write to its
  // contained node. So we don't need to track anything happened at this node, it is not a TOP
  // effect node but a joined node and make the lhs and rhs looks like one memory region.
  virtual MemoryWrite*      write_effect() const { return no_write_effect_; }
  virtual std::size_t       read_size   () const { return lhs_->read_size() + rhs_->read_size(); }
  virtual std::size_t       children_size()const { return 0; }
  virtual void VisitRead    ( const ReadVisitor& ) const;
  virtual void VisitChildren( const ChildrenVisitor& visitor ) const { (void)visitor; }
 public:
  virtual void AddReadEffect    ( MemoryRead* );
  virtual void UpdateWriteEffect( MemoryWrite* );
  virtual void AssignEffectGroup( Expr* key , EffectGroup* grp ) { (void)key;(void)grp; }
  virtual EffectGroup* Resolve  ( Expr* key ) { (void)key; return this; }

 private:
  typedef std::function<void (EffectGroup*)> Visitor;
  void Visit  ( const Visitor& );
 private:
  NoWriteEffect* no_write_effect_;
  EffectGroup* lhs_;
  EffectGroup* rhs_;
  ::lavascript::zone::Zone* zone_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(MergeEffectGroup);
}

// An object that is used to track *all* effect group object efficiently. Basically it is just an
// OOLVector wrapper and it provides explicit copy mechanism to copy effect group list to another
// group using COW semantic. The issue is the previous EffectGroupList must exists or outlive the
// new EffectGroupList. This can be guaranteed by using Environment object since all Environment
// object are on stack which is naturally nested.
class EffectGroupList {
 public:
  EffectGroupList( EffectGroupFactory* factory ) : factory_(factory) , ool_(factory->zone()) {}
  // Do a COW style copy, assume the EffectGroupList exists or outlive this EffectGroupList object
  EffectGroupList( const EffectGroupList& );
 public:
  COWEffectGroup* Get( std::size_t index                       ) const { return ool_.Get(zone(),index); }
  void            Set( std::size_t index , COWEffectGroup* grp )       { ool_.Set(index,grp);           }
 private:
  EffectGroupFactory* factory_;
  ::lavascript::zone::OOLVector<COWEffectGroup*> ool_;

  LAVA_DISALLOW_ASSIGN(EffectGroupList)
};

// Function that visit an expression in a sense that visit all required EffectGroup in a correct order
// This function guarantees that for any expression, if it has certain effect group, then the visiting
// order maintains correct effect serialization.
void VisitEffect( Expr* , EffectGroupList* , BasicEffectGroup* , const ExprEffectVisitor& );

// ============================================================================================
//
// Inline Definition
//
// ============================================================================================
inline EffectGroupFactory::EffectGroupFactory( ::lavascript::zone::Zone* zone ,
                                               const RootEffectGroupGetter& getter ):
  counter_          (1),
  zone_             (zone),
  root_effect_group_(zone->New<RootEffectGroup>(getter))
{}

inline BasicEffectGroup* EffectGroupFactory::NewBasicEffectGroup( MemoryWrite* write ) {
  return zone_->New<BasicEffectGroup>(counter_++,zone_,write);
}

inline COWEffectGroup*   EffectGroupFactory::NewCOWEffectGroup( MemoryWrite* write ) {
  return zone_->New<COWEffectGroup>(counter_++,zone_,write);
}

inline COWEffectGroup*   EffectGroupFactory::NewCOWEffectGroup( const EffectGroup* prev ) {
  return zone_->New<COWEffectGroup>(counter_++,prev);
}

inline MergeEffectGroup* EffectGroupFactory::NewMergeEffectGroup( MemoryWrite* write ) {
  return zone_->New<MergeEffectGroup>(counter_++,zone_,write);
}

template< typename T >
void EffectGroup::DoVisit( zone::Zone* zone , zone::OOLVector<bool>* visited , EffectGroup* grp ,
                                                                               const T& visitor ) {
  visited->Set(zone,grp->id(),true); // mark it as visited before
  visitor(this);
  // recursively visit all children inside of this EffectGroup
  for( auto itr(children_.GetForwardIterator()); itr.HasNext(); itr.Move() ) {
    auto grp = itr.value().grp;
    if(!visited->Get(grp->id())) {
      DoVisit(zone,visited,grp,visitor);
    }
  }
}

} // namespace cbase
} // namespace lavascript

#endif // CBASE_EFFECT_H_
