#ifndef LNMGR_STATUS_H
#define LNMGR_STATUS_H

#include <stdbool.h>

/* forward declarations */
struct graph;
struct node;

/* ========= Status ========= */

typedef enum {
    LNMGR_STATUS_UNKNOWN = 0,
    LNMGR_STATUS_DISABLED,
    LNMGR_STATUS_ADMIN_DOWN,
    LNMGR_STATUS_WAITING,
    LNMGR_STATUS_UP,
    LNMGR_STATUS_FAILED,
} lnmgr_status_t;

/* ========= Status code ========= */

typedef enum {
    LNMGR_CODE_NONE = 0,
    LNMGR_CODE_ADMIN,
    LNMGR_CODE_DISABLED,
    LNMGR_CODE_BLOCKED,
    LNMGR_CODE_SIGNAL,
    LNMGR_CODE_FAILED,
    LNMGR_CODE_UNKNOWN,
} lnmgr_code_t;

/* ========= Explain (runtime visible) ========= */

struct lnmgr_explain {
    lnmgr_status_t status;
    lnmgr_code_t   code;
};

/* ========= Public API ========= */

/* derive current status for a node */
struct lnmgr_explain
lnmgr_status_for_node(struct graph *g,
                      struct node *n,
                      bool admin_up);

/* compare explain objects */
bool
lnmgr_explain_equal(const struct lnmgr_explain *a,
                    const struct lnmgr_explain *b);

/* string helpers (implemented in enum_str.c) */
const char *lnmgr_status_to_str(lnmgr_status_t st);
const char *lnmgr_code_to_str(lnmgr_code_t code);

#endif /* LNMGR_STATUS_H */