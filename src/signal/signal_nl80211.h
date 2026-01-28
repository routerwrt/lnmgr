#ifndef LNMGR_SIGNAL_NL80211_H
#define LNMGR_SIGNAL_NL80211_H

#include "graph.h"

/*
 * nl80211 signal producer
 *
 * Produces graph signals (per ifname):
 *   - "associated"
 *   - "authenticated"
 *   - "connected"
 *
 * Lifecycle:
 *   - signal_nl80211_fd() opens socket and subscribes
 *   - signal_nl80211_handle() processes readable events
 *   - signal_nl80211_close() releases resources
 */

int  signal_nl80211_fd(void);
void signal_nl80211_handle(struct graph *g);
void signal_nl80211_close(void);

#endif /* LNMGR_SIGNAL_NL80211_H */