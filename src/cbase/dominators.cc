#include "dominators.h"
#include "hir.h"

#include <sstream>
#include <utility>
#include <algorithm>

namespace lavascript {
namespace cbase      {
namespace hir        {

void Dominators::AddSet( DominatorSet* set , ControlFlow* node ) const {
  auto itr = std::upper_bound(set->begin(),set->end(),node);
  set->insert(itr,node);
}

void Dominators::IntersectSet( DominatorSet* set , const DominatorSet& another ) const {
  DominatorSet temp;
  std::set_intersection(set->begin(),set->end(),another.begin(),another.end(),
                                                                std::back_inserter(temp));
  *set = std::move(temp);
}

void Dominators::IntersectSet( DominatorSet* set , const DominatorSet& l ,
                                               const DominatorSet& r ) const {
  std::set_intersection(l.begin(),l.end(),r.begin(),r.end(),std::back_inserter(*set));
}

Dominators::DominatorSet* Dominators::GetDomSet( const Graph& graph ,
                                                 const std::vector<ControlFlow*>& cf ,
                                                 ControlFlow* node ) {
  auto i = dominators_.find(node);
  if(i == dominators_.end()) {
    auto ret = dominators_.insert(std::make_pair(node,DominatorSet()));
    auto set = &(ret.first->second);
    if(node == graph.start()) {
      AddSet(set,node);
    } else {
      for( auto &e : cf ) AddSet(set,e);
    }
    return set;
  } else {
    return &(i->second);
  }
}

void Dominators::Build( const Graph& graph ) {
  dominators_.clear(); // reset the dominators

  std::vector<ControlFlow*> all_cf;
  bool has_change = false;
  DominatorSet temp;
  temp.reserve(64);

  all_cf.reserve(64);
  graph.GetControlFlowNode(&all_cf);

  do {
    has_change = false;
    for( ControlFlowRPOIterator itr(graph) ; itr.HasNext() ; itr.Move() ) {
      ControlFlow* n = itr.value();
      DominatorSet* set = GetDomSet(graph,all_cf,n);

      // iterate against this node's predecessor's
      {
        temp.clear();
        for( auto pitr(n->backward_edge()->GetForwardIterator());
             pitr.HasNext() ; pitr.Move() ) {
          if(temp.empty()) {
            temp = *GetDomSet(graph,all_cf,pitr.value());
          } else {
            IntersectSet(&temp,*GetDomSet(graph,all_cf,pitr.value()));
          }
        }
        AddSet(&temp,n);

        // check dominator set is same or not
        bool c = (temp != *set);
        if(c) has_change = true;
        if(c) set->swap(temp);
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
  IntersectSet(&temp,l,r);
  return std::move(temp);
}

bool Dominators::IsDominator( ControlFlow* node , ControlFlow* dom ) const {
  auto set = GetDominatorSet(node);
  return std::binary_search(set.begin(),set.end(),dom);
}

std::string Dominators::GetNodeName(ControlFlow* node) const {
  return ::lavascript::Format("%s_%d", node->type_name(), node->id());
}

std::string Dominators::PrintToDotFormat() const {
  std::stringstream formatter;

  formatter << "digraph dom {\n";
  // 1. this pass generate all the *node* of the graph
  for( auto &e : dominators_ ) {
    formatter << "  " << GetNodeName(e.first) << "[color=red]\n";
  }

  // 2. this pass generate dominator relationship
  for( auto &e : dominators_ ) {
    auto &dset = e.second;
    auto name  = GetNodeName(e.first);

    for( auto &dom : dset ) {
      formatter << "  " << name << " -> "
                                << GetNodeName(dom)
                                << "[color=grey style=dashed]\n";
    }
  }
  formatter << "}\n";
  return formatter.str();
}

} // namespace hir
} // namespace cbase
} // namespace lavascript
