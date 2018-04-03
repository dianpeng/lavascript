#ifndef CBASE_EFFECT_H_
#define CBASE_EFFECT_H_
#include "src/zone/zone.h"
#include "hir.h"

/**
 * Helper module to do effect analyzing during graph construction.
 *
 * The main point of effect analyzing is to figure out a *correct* order for each statement when
 * represented inside of the HIR graph. Due to the Sea-of-Nodes style SSA, the code is out of
 * order fundementally. We need to express correct dependency while generating the graph and this
 * analyze must be performed *on the fly*.
 *
 * The basically idea is as follow, each node that creates a memory , basically following node :
 * 1. function call
 * 2. irlist
 * 3. irobject
 * 4. arg/gget/uget
 *
 * will have an effect object called EffectGroup attach to this node since each node has its
 * identity (id()) , so we can use this number to reference internally. Each EffectGroup can
 * be chained to represent effect transfer or aliasing during corresponding IR is generated
 * when graph is constructed.
 */

namespace lavascript {
namespace cbase      {

// EffectGroup, represents a node's attached effect chain. Each effect group generally
// represent :
// 1) write effect , previously generated
// 2) all read effect which should happen *after* this write operation
// 3) a list of *chained* effect group that can be observed from this memory position due to
//    alias
//
// The implementation of this object uses COW(copy-on-write) techinique to avoid too much
// copy due to the fact that when environment is backed up , most of the effect group should
// remain untouched most of the time due to write is very rare to happen.
class EffectGroup : public ::lavascript::zone::ZoneObject {
 public:
  // normal initialization without COW semantic
  EffectGroup( MemoryWrite* w );
  // COW style operations
  EffectGroup( const EffectGroup& that );
 public:
  bool copy() const { return copy_; }
  const EffectGroup* prev() const { return prev_; }
  // get the current write effect
  MemoryWrite* write_effect() const { return read_ptr()->write_effect_; }
  // set the current write effect , this is a raw set , please consider using
  // UpdateWriteEffect which correctly update the write after read dependency
  // ie , anti dependency.
  void set_write_effect( MemoryWrite* effect ) { return write_ptr()->write_effect_ = effect; }
  // get a list of read operation happened currently
  const ::lavascript::zone::Vector<MemoryRead*>& read_list() const { return read_ptr()->read_list_; }
  // get a list of chained effect group starting from here
  const ::lavascript::zone::Vector<EffectGroup*>& children() const { return read_ptr()->children_ ; }
 public:
  // add a read effect and configured it to be true dependency
  void AddReadEffect    ( MemoryRead*  );
  // update the write effect and configure it to be anti dependency
  void UpdateWriteEffect( MemoryWrite* );
  // test whether two effect group are the same or not , it is used to avoid
  // merge effect group when we need to join 2 environments and insert Phi node
  bool IsEqual( const EffectGroup& grp ) const;
  // merge the effect group and store the result to dest effect group. The dest
  // group can be *this* group and it handles it correctly
  void Merge  ( const EffectGroup& grp , EffectGroup* dest ) const;
 public:
  // add a effect group as chianed child
  void AddEffectGroup( EffectGroup* grp ) { write_ptr()->children_.Add(zone_,grp); }
 private:
  // return a effective pointer that is used for reading
  const EffectGroup* read_ptr() const { return copy_ ? this : prev_; }
  // return a effective pointer that is used for writing
  EffectGroup* write_ptr();
 private:
  MemoryWrite* write_effect_;
  ::lavascript::zone::Vector<MemoryRead*> read_list_;
  ::lavascript::zone::Vector<EffectGroup*> children_;
  ::lavascript::zone::Zone* zone_;
  const EffectGroup* prev_;
  bool               copy_;
};

} // namespace cbase
} // namespace lavascript

#endif // CBASE_EFFECT_H_
