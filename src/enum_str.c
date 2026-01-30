#include "enum_str.h"
#include "lnmgr_status.h"


const char *node_kind_to_str(node_kind_t kind)
{
    const struct node_kind_desc *d = node_kind_lookup(kind);
    return d ? d->name : "unknown";
}

node_kind_t node_kind_from_str(const char *name)
{
    const struct node_kind_desc *d = node_kind_lookup_name(name);
    return d ? d->kind : KIND_LINK_GENERIC;
}

/* ===== node lifecycle state ===== */

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

/* ===== explain reason ===== */

const char *explain_type_to_str(explain_type_t e)
{
    switch (e) {
    case EXPLAIN_NONE:     return NULL;
    case EXPLAIN_DISABLED: return "disabled";
    case EXPLAIN_BLOCKED:  return "blocked";
    case EXPLAIN_SIGNAL:   return "signal";
    case EXPLAIN_FAILED:   return "failed";
    default:               return "unknown";
    }
}

/* ===== protocol-visible status ===== */

const char *lnmgr_status_to_str(lnmgr_status_t st)
{
    switch (st) {
    case LNMGR_STATUS_DISABLED:   return "disabled";
    case LNMGR_STATUS_ADMIN_DOWN: return "admin-down";
    case LNMGR_STATUS_WAITING:    return "waiting";
    case LNMGR_STATUS_UP:         return "up";
    case LNMGR_STATUS_FAILED:     return "failed";
    default:                      return "unknown";
    }
}

/* ===== protocol-visible code ===== */

const char *lnmgr_code_to_str(lnmgr_code_t code)
{
    switch (code) {
    case LNMGR_CODE_ADMIN:    return "admin";
    case LNMGR_CODE_DISABLED: return "disabled";
    case LNMGR_CODE_BLOCKED:  return "blocked";
    case LNMGR_CODE_SIGNAL:   return "signal";
    case LNMGR_CODE_FAILED:   return "failed";
    default:                  return NULL;
    }
}