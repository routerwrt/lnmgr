#ifndef LNMGR_PROTOCOL_H
#define LNMGR_PROTOCOL_H

#include "graph.h"

/* ========= Node type ========= */

const char *lnmgr_node_type_to_str(node_type_t t);
node_type_t lnmgr_node_type_from_str(const char *s);

/* ========= Explain ========= */

const char *lnmgr_explain_to_str(explain_type_t t);

/* ========= Status ========= */

typedef enum {
    LNMGR_STATUS_UNKNOWN = 0,
    LNMGR_STATUS_DISABLED,
    LNMGR_STATUS_ADMIN_DOWN,
    LNMGR_STATUS_WAITING,
    LNMGR_STATUS_UP,
    LNMGR_STATUS_FAILED,
} lnmgr_status_t;

const char *lnmgr_status_to_str(lnmgr_status_t st);

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

const char *lnmgr_code_to_str(lnmgr_code_t code);

#endif