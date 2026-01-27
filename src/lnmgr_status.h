#ifndef LNMGR_STATUS_H
#define LNMGR_STATUS_H

enum lnmgr_status {
    LNMGR_STATUS_UNKNOWN = 0,
    LNMGR_STATUS_DISABLED,
    LNMGR_STATUS_ADMIN_DOWN,
    LNMGR_STATUS_WAITING,
    LNMGR_STATUS_UP,
    LNMGR_STATUS_FAILED,
};

const char *lnmgr_status_str(enum lnmgr_status s);

#endif