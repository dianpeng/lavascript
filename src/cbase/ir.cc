#include "ir.h"

namespace lavascript {
namespace cbase {
namespace ir {

const char* IRTypeGetName( IRType type ) {
#define __(A,B,C) case IRTYPE_##B: return C;
  switch(type) {
    CBASE_IR_LIST(__)
    default: lava_die(); return NULL;
  }
#undef __ // __
}

void Expr::Replace( Expr* another ) {
  // 1. check all the operand_list and patch each operands reference
  //    list to be new one
  for( auto itr = ref_list_.GetForwardIterator(); itr.HasNext() ; itr.Move() ) {
    itr.value().id.set_value( another );
  }

  // 2. check whether this operation has side effect or not and update the
  //    side effect accordingly
  if(HasEffect()) {
    EffectEdge ee (effect());
    ee.iterator.set_value(another);
    another->set_effect(ee);
  }
}

void Graph::Initialize( Start* start , End* end ) {
  start_ = start;
  end_   = end;
}

void Graph::Initialize( OSRStart* start , OSREnd* end ) {
  start_ = start;
  end_   = end;
}

bool GraphDFSIterator::Move() {
  while(!stack_.empty()) {
recursion:
    ControlFlow* top = stack_.Top()->AsControlFlow();

    // iterate through all its predecessor / backward-edge
    for( std::size_t i = 0 ; i < top->backward_edge()->size() ; ++i ) {
      // check all its predecessor to see whether there're some not visited
      // and then do it recursively
      ControlFlow* pre = top->backward_edge()->Index(i);

      if(!visited_[pre->id()]) {
        // this node is not visited, so push it onto the top of the stack and
        // then visit it recursively
        stack_.Push(pre);
        goto recursion;
      }
    }

    // when we reach here it means we scan through all its predecessor nodes and
    // don't see any one not visited , or maybe this node is a singleton/leaf.
    next_ = top;
    stack_.Pop();
    visited_[top->id()] = true; // set it to be visited before
    return true;
  }

  next_ = NULL;
  return false;
}

bool GraphBFSIterator::Move() {
  if(!stack_.empty()) {
    ControlFlow* top = stack_.Top()->AsControlFlow();
    stack_.Pop(); // pop the top element
    lava_debug(NORMAL,lava_verify(!visited_[top->id()]););
    visited_[top->id()] = true;

    for( auto itr = top->backward_edge()->GetBackwardIterator() ;
              itr.HasNext() ; itr.Move() ) {
      ControlFlow* pre = itr.value();
      if(!visited_[pre->id()]) stack_.Push(pre);
    }

    next_ = top;
    return true;
  }
  next_ = NULL;
  return false;
}

bool GraphEdgeIterator::Move() {
  if(!stack_.empty()) {
    ControlFlow* top = stack_.back();
    stack_.pop_back(); // pop the TOP element
    lava_debug(NORMAL,lava_verify(visited_[top->id()]););

    for( auto itr = top->backward_edge()->GetBackwardIterator();
         itr.HasNext(); itr.Move() ) {
      ControlFlow* pre = itr.value();
      if(!visited_[pre->id()]) {
        visited_[pre->id()] = true;
        stack_.push_back(pre);
      }
      results_.push_back(Edge(top,pre));
    }
  }

  if(results_.empty()) {
    next_.Clear();
    return false;
  } else {
    next_ = results_.front();
    results_.pop_front();
    return true;
  }
}

} // namespace ir
} // namespace cbase
} // namespace lavascript
