#include "graph.h"

const char *node_type_to_str(node_type_t t)
{
    switch (t) {
    case NODE_DEVICE:      return "device";
    case NODE_BRIDGE:      return "bridge";
    case NODE_TRANSFORMER: return "transformer";
    case NODE_SERVICE:     return "service";
    default:               return "unknown";
    }
}

const char *node_state_to_str(node_state_t s)
{
    switch (s) {
    case NODE_INACTIVE: return "inactive";
    case NODE_WAITING:  return "waiting";
    case NODE_ACTIVE:   return "active";
    case NODE_FAILED:   return "failed";
    default:            return "unknown";
    }
}

const char *explain_type_to_str(explain_type_t e)
{
    switch (e) {
    case EXPLAIN_NONE:     return "none";
    case EXPLAIN_DISABLED: return "disabled";
    case EXPLAIN_BLOCKED:  return "blocked";
    case EXPLAIN_SIGNAL:   return "signal";
    case EXPLAIN_FAILED:   return "failed";
    default:               return "unknown";
    }
}