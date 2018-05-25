#ifndef CBASE_HIR_NODE_TYPE_H_
#define CBASE_HIR_NODE_TYPE_H_

#include "node-type.expr.generate.h"
#include "node-type.cf.generate.h"
#include "node-type.inode.generate.h"

#define CBASE_HIR_LIST(__)       \
  CBASE_HIR_EXPRESSION(__)       \
  CBASE_HIR_CONTROL_FLOW(__)

#endif // CBASE_HIR_NODE_TYPE_H_
