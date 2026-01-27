#include "lnmgr_status.h"

/*
 * Map graph explain + admin intent to user-visible status.
 *
 * Rules (frozen):
 * - disabled always wins
 * - admin-down always wins over graph readiness
 * - failed is sticky
 * - any explain != NONE => WAITING
 * - NONE => UP
 */
struct lnmgr_explain lnmgr_status_from_graph(const struct explain *gex,
                        bool admin_up)
{
    struct lnmgr_explain out = {
        .status = LNMGR_STATUS_UNKNOWN,
        .code   = NULL,
    };

    if (!gex) {
        return out;
    }

    if (gex->type == EXPLAIN_DISABLED) {
        out.status = LNMGR_STATUS_DISABLED;
        out.code   = "disabled";
        return out;
    }

    if (!admin_up) {
        out.status = LNMGR_STATUS_ADMIN_DOWN;
        out.code   = "admin";
        return out;
    }

    if (gex->type == EXPLAIN_FAILED) {
        out.status = LNMGR_STATUS_FAILED;
        out.code   = gex->detail ? gex->detail : "failed";
        return out;
    }

    if (gex->type != EXPLAIN_NONE) {
        out.status = LNMGR_STATUS_WAITING;
        out.code   = gex->detail;
        return out;
    }

    out.status = LNMGR_STATUS_UP;
    return out;
}

const char * lnmgr_status_str(lnmgr_status_t st)
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
}