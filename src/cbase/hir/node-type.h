#ifndef CBASE_HIR_NODE_TYPE_H_
#define CBASE_HIR_NODE_TYPE_H_

#include "node-type.expr.generated.h"
#include "node-type.cf.generated.h"

#define CBASE_HIR_LIST(__)                                            \
  CBASE_HIR_EXPRESSION(__)                                            \
  CBASE_HIR_CONTROL_FLOW(__)

#endif // CBASE_HIR_NODE_TYPE_H_
