#ifndef LNMGR_SIGNAL_NETLINK_H
#define LNMGR_SIGNAL_NETLINK_H

#include "graph.h"

/*
 * Netlink (RTM_NEWLINK) signal producer
 *
 * Produces graph signals:
 *   - "carrier"  (IFF_LOWER_UP)
 *
 * Lifecycle:
 *   - signal_netlink_fd() opens socket and performs initial dump
 *   - signal_netlink_handle() processes readable events
 *   - signal_netlink_close() releases resources
 */

/* Returns netlink fd, opening and syncing on first call */
int  signal_netlink_fd(void);

int signal_netlink_sync(struct graph *g);

/* Handle one readable netlink event */
bool signal_netlink_handle(struct graph *g);

/* Close netlink socket */
void signal_netlink_close(void);

#endif /* LNMGR_SIGNAL_NETLINK_H */