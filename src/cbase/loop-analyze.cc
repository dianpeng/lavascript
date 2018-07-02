#include "loop-analyze.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

class LoopAnalyze::Impl {
 public:
  Impl( LoopAnalyze* la ): la_(la) , current_node_(NULL) {}
  void Construct( const Graph& graph );
 private:
  zone::Zone*       zone() { return la_->zone_; }
  std::uint32_t AssignID() { return la_->max_id_++; }
 private:
  LoopAnalyze* la_;
  LoopNode*    current_node_; // points to TOS
};

void LoopAnalyze::Impl::Construct( const Graph& graph ) {
  zone::stl::ZoneStack<LoopNode*> nested(zone());

  lava_foreach( auto node , ControlFlowRPOIterator(la_->zone_,graph) ) {
    switch(node->type()) {
      case HIR_LOOP_HEADER:
        {
          // starts of the loop nodes
          auto cnode          = zone()->New<LoopAnalyze::LoopNode>(zone(),AssignID(),current_node_);
          cnode->loop_header_ = node->As<LoopHeader>();
          cnode->depth_       = nested.size() + 1;

          if(current_node_) {
            current_node_->children_.push_back(cnode);
          } else {
            // this node doesn't have parent so it is the outer most loop
            la_->parent_list_.push_back(cnode);
          }
          nested.push(cnode);
          current_node_ = cnode;
        }

        current_node_->block_count_++;
        la_->node_to_loop_[node->id()] = current_node_;
        break;
      case HIR_LOOP:
        lava_verify(current_node_);
        current_node_->loop_body_ = node->As<Loop>();

        current_node_->block_count_++;
        la_->node_to_loop_[node->id()] = current_node_;
        break;
      case HIR_LOOP_EXIT:
        lava_verify(current_node_);
        current_node_->loop_exit_ = node->As<LoopExit>();

        current_node_->block_count_++;
        la_->node_to_loop_[node->id()] = current_node_;
        break;
      case HIR_LOOP_MERGE:
        lava_verify(current_node_);
        current_node_->loop_merge_ = node->As<LoopMerge>();
        current_node_->block_count_++;
        la_->node_to_loop_[node->id()] = current_node_;

        nested.pop();
        current_node_ = nested.empty() ? NULL : nested.top();
        break;
      default:
        if(current_node_) {
          current_node_->block_count_++;
          la_->node_to_loop_[node->id()] = current_node_;
        }
        break;
    }
  }
}

LoopAnalyze::LoopNodeROIterator::LoopNodeROIterator( LoopAnalyze::LoopNode* start ,
                                                     const LoopAnalyze*        la ):
  stk_ (la->zone()),
  la_  (la),
  next_(NULL)
{
  MoveNode(start);
}

LoopAnalyze::LoopNode* LoopAnalyze::LoopNodeROIterator::MoveNode( LoopAnalyze::LoopNode* node ) {
  if(node->IsLeaf()) {
    return (next_ = node);
  }

  // start to visit this node in DFS order
  {
    auto first = node;
    do {
      stk_.push(Record(first));
      first = first->children().at(0);
    } while(first->IsInternal());;
    return (next_ = first);
  }
}

bool LoopAnalyze::LoopNodeROIterator::Move() {
  while(!stk_.empty()) {
    auto &top = stk_.top();
    if(top.node->children().size() == top.pos + 1) {
      next_ = top.node; // visit the parental node
      stk_.pop();
      return true;
    } else {
      return MoveNode(top.node->children().at(++top.pos));
    }
  }

  next_ = NULL;
  return false;
}

LoopAnalyze::LoopNodeRDIterator::LoopNodeRDIterator( LoopAnalyze::LoopNode* start ,
                                                     const LoopAnalyze*        la ):
  q_   (la->zone()),
  la_  (la),
  next_(NULL)
{
  for(auto &k : start->children() )
    q_.push(k);
  next_ = start;
}

bool LoopAnalyze::LoopNodeRDIterator::Move() {
  while(!q_.empty()) {
    auto &top = q_.front();
    q_.pop();
    // push all the children into the queue
    for( auto &k : top->children() ) {
      q_.push(k);
    }
    next_ = top;
    return true;
  }

  next_ = NULL;
  return false;
}

LoopAnalyze::LoopAnalyze( zone::Zone* zone , const Graph& graph ):
  zone_        (zone),
  parent_list_ (zone),
  node_to_loop_(zone),
  max_id_      (0)
{
  node_to_loop_.resize( graph.MaxID() );

  Impl impl(this);
  impl.Construct(graph);
}

void LoopAnalyze::Dump( DumpWriter* output ) const {
  for( auto e : parent_list_ ) {
    DumpWriter::Section section(output,"loop cluster starts at %d",e->id());
    lava_foreach( auto ln , LoopNodeROIterator(e,this) ) {
      if(ln->parent()) {
        output->WriteL("loop:%d(%zu,%zu); --> loop:%d",ln->id(),ln->depth       (),
                                                                ln->block_count (),
                                                                ln->parent()->id());
      } else {
        output->WriteL("loop:%d(%zu,%zu); --> <null>" ,ln->id(),ln->depth(),ln->block_count());
      }
    }
  }
}

} // namespace hir
} // namespace cbase
} // namespace lavascript
