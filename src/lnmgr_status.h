#include "protocol.h"

struct lnmgr_explain {
    lnmgr_status_t status;
    lnmgr_code_t   code;
};

struct lnmgr_explain
lnmgr_status_from_graph(const struct explain *gex, bool admin_up);