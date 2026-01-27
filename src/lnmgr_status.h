/*
 * lnmgr status mapping rules (ABI-stable):
 *
 * - DISABLED overrides everything
 * - ADMIN_DOWN overrides graph readiness
 * - FAILED is sticky
 * - explain != NONE => WAITING
 * - explain == NONE => UP
 */
#ifndef LNMGR_STATUS_H
#define LNMGR_STATUS_H

#include "graph.h"

/*
 * User-visible link status.
 * Frozen API.
 */
typedef enum {
    LNMGR_STATUS_UNKNOWN = 0,
    LNMGR_STATUS_DISABLED,
    LNMGR_STATUS_ADMIN_DOWN,
    LNMGR_STATUS_WAITING,
    LNMGR_STATUS_UP,
    LNMGR_STATUS_FAILED,
} lnmgr_status_t;

const char * lnmgr_status_to_str(lnmgr_status_t st)
{
    switch (st) {
    case LNMGR_STATUS_DISABLED:   return "disabled";
    case LNMGR_STATUS_ADMIN_DOWN: return "admin-down";
    case LNMGR_STATUS_WAITING:    return "waiting";
    case LNMGR_STATUS_UP:         return "up";
    case LNMGR_STATUS_FAILED:     return "failed";
    case LNMGR_STATUS_UNKNOWN:
    default:
        return "unknown";
    }
};

typedef enum {
    LNMGR_CODE_NONE = 0,
    LNMGR_CODE_ADMIN,
    LNMGR_CODE_SIGNAL,
    LNMGR_CODE_BLOCKED,
    LNMGR_CODE_FAILED,
} lnmgr_code_t;

/*
 * Manager-level explain (semantic, user/API facing).
 * `code` must be a frozen identifier (carrier, assoc, discovery, ...)
 */
struct lnmgr_explain {
    lnmgr_status_t status;
    lnmgr_code_t   code;
};

const char *lnmgr_code_to_str(lnmgr_code_t c)
{
    switch (c) {
    case LNMGR_CODE_ADMIN:   return "admin";
    case LNMGR_CODE_SIGNAL:  return "signal";
    case LNMGR_CODE_BLOCKED: return "blocked";
    case LNMGR_CODE_FAILED:  return "failed";
    case LNMGR_CODE_NONE:
    default:
        return NULL;
    }
}

/*
 * Derive user-visible status + explain from graph explain
 * and manager-level facts.
 *
 * NOTE:
 * - admin_up is passed in explicitly
 * - graph remains policy-agnostic
 */
struct lnmgr_explain
lnmgr_status_from_graph(const struct explain *gex,
                        bool admin_up);

#endif