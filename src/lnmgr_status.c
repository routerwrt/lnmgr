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
        .code   = LNMGR_CODE_NONE,
    };

    if (!gex)
        return out;

    if (gex->type == EXPLAIN_DISABLED) {
        out.status = LNMGR_STATUS_DISABLED;
        return out;
    }

    if (!admin_up) {
        out.status = LNMGR_STATUS_ADMIN_DOWN;
        out.code   = LNMGR_CODE_ADMIN;
        return out;
    }

    if (gex->type == EXPLAIN_FAILED) {
        out.status = LNMGR_STATUS_FAILED;
        out.code   = LNMGR_CODE_FAILED;
        return out;
    }

    if (gex->type != EXPLAIN_NONE) {
        out.status = LNMGR_STATUS_WAITING;
        out.code   = LNMGR_CODE_SIGNAL;
        return out;
    }

    out.status = LNMGR_STATUS_UP;
    return out;
}