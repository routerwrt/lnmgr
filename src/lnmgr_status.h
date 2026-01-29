#ifndef LNMGR_STATUS_H
#define LNMGR_STATUS_H

#include "protocol.h"

struct lnmgr_explain {
    lnmgr_status_t status;
    lnmgr_code_t   code;
};

struct lnmgr_explain
lnmgr_status_from_graph(const struct explain *gex, bool admin_up);

struct lnmgr_explain
lnmgr_status_for_node(struct graph *g,
                      struct node *n,
                      bool admin_up);

bool lnmgr_explain_equal(const struct lnmgr_explain *a,
                         const struct lnmgr_explain *b);

struct lnmgr_explain current_status(struct graph *g,
                            struct node *n, bool admin_up);

#endif /* LNMGR_STATUS_H */