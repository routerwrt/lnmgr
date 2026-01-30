#include "lnmgr_status.h"
#include "graph.h"

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
static struct lnmgr_explain
lnmgr_status_from_graph(const struct explain *gex, bool admin_up)
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

struct lnmgr_explain
lnmgr_status_for_node(struct graph *g,
                      struct node *n,
                      bool admin_up)
{
    struct explain ex = graph_explain_node(g, n->id);
    return lnmgr_status_from_graph(&ex, admin_up);
}

bool lnmgr_explain_equal(const struct lnmgr_explain *a,
                         const struct lnmgr_explain *b)
{
    if (!a || !b)
        return false;

    return a->status == b->status && a->code == b->code;
}