#include "dominators.h"
#include "hir.h"
#include "src/ool-vector.h"

#include <sstream>
#include <utility>
#include <algorithm>

namespace lavascript {
namespace cbase      {
namespace hir        {

using ::lavascript::OOLVector;

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
  dominators_.clear();
  imm_dominators_.clear();

  OOLVector<std::int32_t>   ts(graph.MaxID());
  std::int32_t cur_ts = 0;

  // do a timestamp mark using a DFS iteration algorithm
  for( ControlFlowPOIterator itr(graph); itr.HasNext() ; itr.Move() ) {
    auto n = itr.value();
    lava_debug(NORMAL,lava_verify(ts[n->id()] == 0););
    ts[n->id()] = ++cur_ts;
  }

  // do a dominator set generation using data flow algorithm
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

  // mark the immediate dominator
  for( auto &e : dominators_ ) {
    auto n = e.first;
    auto&s = e.second;
    if(n == graph.start()) continue;

    ControlFlow* imm = NULL;
    for( auto &dom : s ) {
      if(dom == n) continue;

      // mark immediate dominator to be dominator node that has smallest
      // timestamp number , ie it is the closest node to the dominated node
      if(imm == NULL) imm = dom;
      else if(ts[dom->id()] < ts[imm->id()]) {
        imm = dom;
      }
    }

    if(imm) imm_dominators_[n] = imm;
  }
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

ControlFlow* Dominators::GetImmDominator( ControlFlow* node ) const {
  auto n = imm_dominators_.find(node);
  return n == imm_dominators_.end() ? NULL : n->second;
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
  {
    formatter << "digraph domset {\n";
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
  }

  {
    formatter << "digraph idom {\n";

    for( auto &e : imm_dominators_ ) {
      auto fn = GetNodeName(e.first);
      auto sn = GetNodeName(e.second);

      formatter << "  " << GetNodeName(e.first) << "[color=red]\n";
      formatter << "  " << fn << " -> " << sn <<"[color=grey style=dashed]\n";
    }

    formatter << "}\n";
  }

  return formatter.str();
}

} // namespace hir
} // namespace cbase
} // namespace lavascript
