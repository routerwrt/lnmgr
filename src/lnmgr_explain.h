#ifndef LNMGR_EXPLAIN_H
#define LNMGR_EXPLAIN_H

/*
 * lnmgr explain codes:
 * Stable semantic identifiers.
 * User / API visible.
 * Append-only.
 */
enum lnmgr_explain_type {
    LNMGR_EXPLAIN_NONE = 0,
    LNMGR_EXPLAIN_DISABLED,
    LNMGR_EXPLAIN_ADMIN_DOWN,
    LNMGR_EXPLAIN_SIGNAL,
    LNMGR_EXPLAIN_DEPENDENCY,
    LNMGR_EXPLAIN_FAILED,
};

/*
 * Explain codes are stable identifiers.
 * They are part of the public API.
 * Append-only. Never rename or remove.
 */
#define LNMGR_EXPLAIN_PRESENT     "present"
#define LNMGR_EXPLAIN_PARENT      "parent"
#define LNMGR_EXPLAIN_MEMBER      "member"
#define LNMGR_EXPLAIN_DEPENDENCY  "dependency"
#define LNMGR_EXPLAIN_ADMIN       "admin"
#define LNMGR_EXPLAIN_DISABLED    "disabled"
#define LNMGR_EXPLAIN_CARRIER     "carrier"
#define LNMGR_EXPLAIN_DISCOVERY   "discovery"
#define LNMGR_EXPLAIN_ASSOC       "assoc"
#define LNMGR_EXPLAIN_SESSION     "session"
#define LNMGR_EXPLAIN_HANDSHAKE   "handshake"
#define LNMGR_EXPLAIN_ADDRESS     "address"
#define LNMGR_EXPLAIN_ROUTE       "route"
#define LNMGR_EXPLAIN_DNS         "dns"
#define LNMGR_EXPLAIN_USABLE      "usable"
#define LNMGR_EXPLAIN_FAILED      "failed"

struct lnmgr_explain {
    enum lnmgr_explain_type type;
    const char *code;   /* one of the LNMGR_EXPLAIN_* identifiers */
};

#endif