#ifndef CBASE_LOOP_ANALYZE_H_
#define CBASE_LOOP_ANALYZE_H_
#include "hir.h"
#include "src/trace.h"
#include "src/zone/zone.h"
#include "src/zone/stl.h"

namespace lavascript {
class DumpWriter;
namespace cbase      {
namespace hir        {

// Analyze the HIR and find loop cluster
//
// Each loop itself forms a SCC and then nested together to forms a tree. Exmaple :
//
// for( var i = 0 ; 100 ; 1 ) {  -> loop A
//   for( var j = 1 ; 200 ; 1 ) { -> loop B
//   }
//   for( var q = 1 ; 200 ; 1 ) { -> loop C
//   }
// }
//
// The tree is like :
//     -----------
//     |    A    |
//     -----------
//      |       |
//      |       |
//  --------  --------
//  |   B   | |  C   |
//  --------  --------
//
// B and C are inner most loop, A is a parental loop of B and C.
// The above tree is called loop cluster or loop nest cluster

class LoopAnalyze {
 public:
  class Impl;
  // Represent a single loop inside of the a loop cluster
  class LoopNode {
   public:
    static const std::size_t kMinimumLoopBlockCount = 4; // loop_header,loop,loop_exit,loop_merge
    const zone::stl::ZoneVector<LoopNode*>& children() const { return children_; }

    LoopNode*     parent     () const { return parent_;      }
    LoopHeader*   loop_header() const { return loop_header_; }
    Loop*         loop_body  () const { return loop_body_;   }
    LoopExit*     loop_exit  () const { return loop_exit_;   }
    Merge*        loop_merge () const { return loop_merge_;  }
    std::size_t   depth      () const { return depth_;       }
    std::size_t   block_count() const { return block_count_; }
    std::uint32_t id         () const { return id_;          }
    bool          IsInternal () const { return !children_.empty(); }
    bool          IsLeaf     () const { return !IsInternal();      }
    bool          IsOuterMost() const { return parent_ == NULL;    }

    Node* GetPreHeader () const { return loop_header();}
    Node* GetPostHeader() const { return loop_exit (); }
    Node* GetPostExit  () const { return loop_merge(); }
   public:
    LoopNode( zone::Zone* zone , std::uint32_t id , LoopNode* p ):
      parent_     (p),
      children_   (zone),
      loop_header_(NULL),
      loop_body_  (NULL),
      loop_exit_  (NULL),
      loop_merge_ (NULL),
      depth_      (0),
      block_count_(0),
      id_         (id)
    {}
   private:
    LoopNode*                        parent_;
    zone::stl::ZoneVector<LoopNode*> children_;
    // information about the loop
    LoopHeader*                      loop_header_;
    Loop*                            loop_body_;
    LoopExit*                        loop_exit_;
    Merge*                           loop_merge_;
    // statistic information
    std::size_t                      depth_;
    std::size_t                      block_count_;
    std::uint32_t                    id_;

    friend class LoopAnalyze::Impl;
  };

  // LoopNode's reverse order iterator. It will visit inner most loop node at
  // very first and then its sibling and then its parent until hit the outer
  // most LoopNode object , ie which doesn't have parent LoopNode
  class LoopNodeROIterator {
   public:
    LoopNodeROIterator( LoopNode* , const LoopAnalyze* );
    bool       Move();
    bool    HasNext() const { return next_ != NULL; }
    LoopNode* value() const { return next_; }
   private:
    LoopNode*  MoveNode( LoopNode* );
   private:
    struct Record { LoopNode* node; std::size_t pos; Record(LoopNode* n):node(n),pos(0){} };

    zone::stl::ZoneStack<Record> stk_;
    const LoopAnalyze*           la_;
    LoopNode*                    next_;
  };

 public:
  // Initialize a LoopAnalyze and build a LoopAnalyze object
  LoopAnalyze( zone::Zone* , const Graph& );

  // Get all the paretal node list , ie starting LoopNode of loop nest cluster
  const zone::stl::ZoneVector<LoopNode*>& parent_list() const { return parent_list_; }

  // Zone of this object
  zone::Zone* zone() const { return zone_; }

  // Index a loop node from the parental list
  LoopNode* IndexOuterLoop( std::size_t idx ) const { return parent_list_[idx]; }

  // Get size of the parental loop list
  std::size_t SizeOfOuterLoop() const { return parent_list_.size(); }

  // Dump the internal loop forest as Dot graph
  void Dump( DumpWriter* ) const;

 private:
  zone::Zone* zone_;
  zone::stl::ZoneVector<LoopNode*> parent_list_;
  zone::stl::ZoneVector<LoopNode*> node_to_loop_;
  std::uint32_t max_id_;

  friend class Impl;
};


} // namespace hir
} // namespace cbase
} // namespace lavascript


#endif // CBASE_LOOP_ANALYZE_H_
