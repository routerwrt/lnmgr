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

/*
 * Manager-level explain (semantic, user/API facing).
 * `code` must be a frozen identifier (carrier, assoc, discovery, ...)
 */
struct lnmgr_explain {
    lnmgr_status_t status;
    const char    *code;   /* NULL if none */
};

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

/* String helpers (UI only, never parsed) */
const char *lnmgr_status_str(lnmgr_status_t st);

#endif