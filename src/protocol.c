#include <string.h>

#include "protocol.h"

/* ===== node type ===== */

const char *lnmgr_node_type_to_str(node_type_t t)
{
    switch (t) {
    case NODE_DEVICE:      return "device";
    case NODE_BRIDGE:      return "bridge";
    case NODE_TRANSFORMER: return "transformer";
    case NODE_SERVICE:     return "service";
    default:               return "unknown";
    }
}

node_type_t lnmgr_node_type_from_str(const char *s)
{
    if (!s) return NODE_DEVICE;
    if (!strcmp(s, "device"))      return NODE_DEVICE;
    if (!strcmp(s, "bridge"))      return NODE_BRIDGE;
    if (!strcmp(s, "transformer")) return NODE_TRANSFORMER;
    if (!strcmp(s, "service"))     return NODE_SERVICE;
    return NODE_DEVICE;
}

/* ===== explain ===== */

const char *lnmgr_explain_to_str(explain_type_t t)
{
    switch (t) {
    case EXPLAIN_DISABLED: return "disabled";
    case EXPLAIN_BLOCKED:  return "blocked";
    case EXPLAIN_SIGNAL:   return "signal";
    case EXPLAIN_FAILED:   return "failed";
    case EXPLAIN_NONE:
    default:
        return NULL;
    }
}

/* ===== status ===== */

const char *lnmgr_status_to_str(lnmgr_status_t st)
{
    switch (st) {
    case LNMGR_STATUS_DISABLED:   return "disabled";
    case LNMGR_STATUS_ADMIN_DOWN: return "admin-down";
    case LNMGR_STATUS_WAITING:    return "waiting";
    case LNMGR_STATUS_UP:         return "up";
    case LNMGR_STATUS_FAILED:     return "failed";
    default:
        return "unknown";
    }
}

/* ===== status code ===== */

const char *lnmgr_code_to_str(lnmgr_code_t code)
{
    switch (code) {
    case LNMGR_CODE_ADMIN:    return "admin";
    case LNMGR_CODE_DISABLED: return "disabled";
    case LNMGR_CODE_BLOCKED:  return "blocked";
    case LNMGR_CODE_SIGNAL:   return "signal";
    case LNMGR_CODE_FAILED:   return "failed";
    default:
        return NULL;
    }
}