#include "dominators.h"
#include "hir.h"
#include "src/zone/zone.h"
#include "src/zone/vector.h"

#include <sstream>
#include <utility>
#include <algorithm>

namespace lavascript {
namespace cbase      {
namespace hir        {

Dominators::Dominators( zone::Zone* zone , const Graph& graph ):
  dominators_    (zone),
  imm_dominators_(zone),
  zone_          (zone)
{
  Build(graph);
}

void Dominators::AddSet( DominatorSet* set , ControlFlow* node ) const {
  auto itr = std::upper_bound(set->begin(),set->end(),node);
  set->insert(itr,node);
}

void Dominators::IntersectSet( DominatorSet* set , const DominatorSet& another ) const {
  DominatorSet temp(zone_);
  std::set_intersection(set->begin(),set->end(),another.begin(),another.end(),
                                                                std::back_inserter(temp));
  *set = std::move(temp);
}

void Dominators::IntersectSet( DominatorSet* set , const DominatorSet& l , const DominatorSet& r ) const {
  std::set_intersection(l.begin(),l.end(),r.begin(),r.end(),std::back_inserter(*set));
}

Dominators::DominatorSet* Dominators::GetDomSet( const Graph& graph , const DominatorSet& cf ,
                                                                      ControlFlow* node ) {
  auto i = dominators_.find(node);
  if(i == dominators_.end()) {
    auto ret = dominators_.insert(std::make_pair(node,DominatorSet(zone_)));
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
  // timestamp recorder
  zone::stl::ZoneVector<std::int32_t> ts(zone_,0,graph.MaxID());
  // current timestamp
  std::int32_t cur_ts = 0;
  // do a timestamp mark using a DFS iteration algorithm
  lava_foreach( auto n , ControlFlowPOIterator(graph) ) {
    lava_debug(NORMAL,lava_verify(ts[n->id()] == 0););
    ts[n->id()] = ++cur_ts;
  }
  // do a dominator set generation using data flow algorithm
  zone::stl::ZoneVector<ControlFlow*> all_cf(zone_);
  bool has_change = false;
  DominatorSet temp(zone_);
  temp.reserve  (64);
  all_cf.reserve(64);
  graph.GetControlFlowNode(&all_cf);

  do {
    has_change = false;
    lava_foreach( auto n , ControlFlowRPOIterator(graph) ) {
      DominatorSet* set = GetDomSet(graph,all_cf,n);
      // iterate against this node's predecessor's
      {
        temp.clear();
        lava_foreach( auto pn , n->backward_edge()->GetForwardIterator() ) {
          if(temp.empty()) {
            temp = *GetDomSet(graph,all_cf,pn);
          } else {
            IntersectSet(&temp,*GetDomSet(graph,all_cf,pn));
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

void Dominators::GetCommonDominatorSet( ControlFlow* n1 , ControlFlow* n2 ,
                                                          DominatorSet* output ) const {
  output->clear();
  auto l = GetDominatorSet(n1);
  auto r = GetDominatorSet(n2);
  IntersectSet(output,l,r);
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
