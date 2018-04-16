#ifndef CBASE_EFFECT_H_
#define CBASE_EFFECT_H_
#include "hir.h"
#include "src/zone/vector.h"

/**
 * Helper module to do effect analyzing during graph construction.
 *
 * After *many times of redesign*, I reach the conclusion. Simplicity wins! The side effect of
 * lavasript will be bound to only one effect . Basically it means any memory related read/write
 * will be assumed to write to a single memory which can be observed by rest of the program. We don't
 * slice the memory to allow finer grain analyze.
 *
 * The old design track *every memory allocation* with its side effect throughout the HIR construction
 * which lead to really complicated code and huge memory footprint due to the exponentially growing
 * state information which may not help too much for optimization. Now we only track one effect globally
 * and all the side effect related operation are all direct to this effect object.
 *
 */

namespace lavascript {
namespace cbase      {
namespace hir        {

// Effect object which represent observable side effect that tracks all the memory related opereation
// happened inside of a *single* lexical scope. When multiple branch starts to merge and Phi is generated,
// the proper effect merge operation should be performed.
//
// The tracked dependency are 1) true dependency ( read after write ) and 2) anti dependency ( write after read ).
// However many read operations is dependency free as long as they read the same writed node ; also multiple
// write operations are treated order free and we don't track them , ie output dependency.
class Effect {
 public:
  Effect( ::lavascript::zone::Zone* , MemoryWrite* );
  Effect( const Effect& );
 public:
  // update the write effect for this effect object
  void UpdateWriteEffect( MemoryWrite* );
  // add a new read effect
  void AddReadEffect    ( MemoryRead*  );
 public:
  // merge the input 2 effect object into another effect object, the input and output can be the same
  static void Merge( const Effect& , const Effect& , Effect* , Graph*  , ControlFlow* , IRInfo* );
 private:
  MemoryWrite*                            write_effect_;   // Write effect currently tracked
  ::lavascript::zone::Vector<MemoryRead*> read_list_;      // All the read happened *after* the write
  ::lavascript::zone::Zone*               zone_;           // Zone object for allocation of memory
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_EFFECT_H_
