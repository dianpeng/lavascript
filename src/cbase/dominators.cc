#include "dominators.h"
#include "hir.h"

#include <utility>
#include <algorithm>

namespace lavascript {
namespace cbase      {
namespace hir        {

void Dominators::AddSet( DominatorSet* set , ControlFlow* node ) const {
  auto itr = std::upper_bound(set->begin(),set->end(),node);
  set->insert(itr,node);
}

void Dominators::UnionSet( DominatorSet* set , const DominatorSet& another ) const {
  DominatorSet temp;
  std::set_intersection(set->begin(),set->end(),another.begin(),another.end(),
                                                                std::back_inserter(temp));
  *set = std::move(temp);
}

void Dominators::UnionSet( DominatorSet* set , const DominatorSet& l ,
                                               const DominatorSet& r ) const {
  std::set_intersection(l.begin(),l.end(),r.begin(),r.end(),std::back_inserter(*set));
}

Dominators::DominatorSet* Dominators::GetDomSet( ControlFlow* node ) {
  auto i = dominators_.find(node);
  if(i == dominators_.end()) {
    auto ret = dominators_.insert(std::make_pair(node,DominatorSet()));
    auto set = &(ret.first->second);
    AddSet(set,node); // add itself
    return set;
  } else {
    return &(i->second);
  }
}

void Dominators::Build( const Graph& graph ) {
  dominators_.clear(); // reset the dominators

  bool has_change = false;
  DominatorSet temp;
  temp.reserve(64);
  do {
    has_change = false;
    for( ControlFlowRPOIterator itr(graph) ; itr.HasNext() ; itr.Move() ) {
      ControlFlow* n = itr.value();
      DominatorSet* set = GetDomSet(n);

      // iterate against this node's predecessor's
      {
        temp.clear();
        for( auto pitr(n->backward_edge()->GetForwardIterator());
             pitr.HasNext() ; pitr.Move() ) {
          UnionSet(&temp,*GetDomSet(pitr.value()));
        }
        AddSet(&temp,n);

        // check dominator set is same or not
        has_change = (temp != *set);
        if(has_change) set->swap(temp);
      }
    }
  } while(has_change);
}

const Dominators::DominatorSet& Dominators::GetDominatorSet( ControlFlow* node ) const {
  auto itr = dominators_.find(node);
  lava_debug(NORMAL,lava_verify(itr != dominators_.end()););
  return itr->second;
}

Dominators::DominatorSet Dominators::GetCommonDominatorSet( ControlFlow* n1 ,
                                                            ControlFlow* n2 ) const {
  DominatorSet temp;
  auto l = GetDominatorSet(n1);
  auto r = GetDominatorSet(n2);
  UnionSet(&temp,l,r);
  return std::move(temp);
}

bool Dominators::IsDominator( ControlFlow* node , ControlFlow* dom ) const {
  auto set = GetDominatorSet(node);
  return std::binary_search(set.begin(),set.end(),dom);
}

} // namespace hir
} // namespace cbase
} // namespace lavascript
