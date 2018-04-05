#ifndef CBASE_EFFECT_H_
#define CBASE_EFFECT_H_
#include "src/zone/zone.h"
#include "hir.h"

namespace lavascript {
namespace cbase      {

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
 * So the traversal of EffectGroup requires a marker to avoid dead loop internally.
 */

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
  virtual EffectGroup* Resolve( Expr*) const = 0;
 public: // raw setters/getters , help to do clone when COW is used
  virtual ::lavascript::zone::Zone*                 zone()         const = 0;
  virtual MemoryWrite*                              write_effect() const = 0;
  virtual ::lavascript::zone::Vector<MemoryRead*>&  read_list()    const = 0;
  // Slice represents a sub region of this effect group , basically a slice is *not*
  // aliced with its contained effect group unless the slice has exactly same effect
  // group (this is not recorded if so).
  struct Slice : public ::lavascript::zone::ZoneObject {
    Expr* key;        // key used to check whether a correct slice can be matched, to compare
                      // a key , we use IsEqual inside of Expr function which is used for GVN,
                      // this function is fine here because it will automatically fails when
                      // node has side effect
    EffectGroup* grp; // attached effect group object

    Slice( Expr* k , EffectGroup* g ) : key(k) , grp(g) {}
    Slice(): key(NULL), grp(NULL) {}
  };
  virtual ::lavascript::zone::Vector<Slice>& chilren  ()    const = 0;

 public:
  EffectGroup ( std::uint32_t id ) : id_(id) {}
  virtual ~EffectGroup() {}

 private:
  std::uint32_t id_;
};

// The most basica effect group and it doesn't support effcient copy mechanism which is required
// to implement effecient environment setup during graph construction. The actual used effect group
// is always wrapper around this NaiveEffectGroup object
class NaiveEffectGroup : public EffectGroup {
 public:
  NaiveEffectGroup( ::lavascript::zone::Zone* zone , MemoryWrite* w );
  NaiveEffectGroup( const NaiveEffectGroup& );
 public:
  virtual ::lavascript::zone::Zone* zone() const { return zone_; }
  virtual const ::lavascript::zone::Vector<MemoryRead*>&  read_list() const { return read_list_; }
  virtual const ::lavascript::zone::Vector<EffectGroup*>& children () const { return children_;  }
  virtual MemoryWrite* write_effect() const { return write_effect_; }
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
  void DoVisit( ::lavascript::zone::Zone* , ::lavascript::zone::OOLVector<bool>* , EffectGroup* ,
                                                                                   const Visitor& );
 private:
  MemoryWrite*                            write_effect_;
  ::lavascript::zone::Vector<MemoryRead*> read_list_;
  ::lavascript::zone::Vector<Slice>       children_;
  ::lavascript::zone::Zone*               zone_;
  LAVA_DISALLOW_ASSIGN(NaiveEffectGroup)
};

// A wrapper around effect group object which supports COW semantic. This object is used to
// track any none global effect group , basically any IRList and IRObject node created memory's
// effect is tracked via COWEffectGroup
class COWEffectGroup : public EffectGroup {
 public:
  COWEffectGroup( ::lavascript::zone::Zone* , MemoryWrite* );
  // COW style copy constructor , needs to be called explicitly
  explicit COWEffectGroup( const COWEffectGroup* );

 public:
  bool               copy         () const { return copy_; }
  const EffectGroup* prev         () const { return prev_; }
  const EffectGroup* effective_ptr() const { return read_ptr(); }

  virtual ::lavascript::zone::Zone* zone() const { return read_ptr()->zone(); }
  virtual const ::lavascript::zone::Vector<MemoryRead*>& read_list() const { return read_ptr()->read_list(); }
  virtual const ::lavascript::zone::Vector<EffectGroup*>& children() const { return read_ptr()->children (); }
  virtual MemoryWrite* write_effect() const { return read_ptr()->write_effect(); }
  virtual void  set_write_effect( MemoryWrite* effect ) { write_effect_ = effect; }
  virtual void  Copy            ( EffectGroup* ) const;
 public:
  virtual void AddReadEffect    ( MemoryRead* effect ) { write_ptr()->AddReadEffect(effect); }
  virtual void UpdateWriteEffect( MemoryWrite*effect ) { write_ptr()->UpdateWriteEffect(effect); }
  virtual void AssignEffectGroup( Expr* , EffectGroup* );
  virtual EffectGroup* Resolve  ( Expr* ) const;
 private:
  const EffectGroup* read_ptr () const { return copy_ ? &(native_) : prev_; }
  // lazily copy the prev into the native effect group
  EffectGroup*       write_ptr() {
    if(!copy_) { copy_ = true; native_.CopyFrom(*prev_); }
    return &native_;
  }
 private:
  NaiveEffectGroup  native_;
  const EffectGroup* prev_ ;
  bool               copy_ ;

  LAVA_DISALLOW_COPY_AND_ASSIGN(COWEffectGroup)
};

// An object that is used to track *all* effect group object efficiently. Basically it is just an
// OOLVector wrapper and it provides explicit copy mechanism to copy effect group list to another
// group using COW semantic. The issue is the previous EffectGroupList must exists or outlive the
// new EffectGroupList. This can be guaranteed by using Environment object since all Environment
// object are on stack which is naturally nested.
class EffectGroupList {
 public:
  EffectGroupList( ::lavascript::zone::Zone* zone ) : zone_(zone) , ool_(zone_) {}
  // Do a COW style copy, assume the EffectGroupList exists or outlive this EffectGroupList object
  EffectGroupList( const EffectGroupList& );
 public:
  COWEffectGroup* Get( std::size_t index                       ) const { return ool_.Get(zone_,index); }
  void            Set( std::size_t index , COWEffectGroup* grp )       { ool_.Set(index,grp); }
 private:
  ::lavascript::zone::Zone* zone_;
  ::lavascript::zone::OOLVector<COWEffectGroup*> ool_;

  LAVA_DISALLOW_ASSIGN(EffectGroupList)
};


} // namespace cbase
} // namespace lavascript

#endif // CBASE_EFFECT_H_
