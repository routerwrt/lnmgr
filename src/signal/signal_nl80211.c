#include <unistd.h>
#include <errno.h>
#include <stdbool.h>

#include "signal_nl80211.h"

/*
 * nl80211 signal producer (stub)
 *
 * Real implementation will:
 *  - open generic netlink socket
 *  - subscribe to nl80211 multicast groups
 *  - map events to graph signals
 */

static int nl_fd = -1;

int signal_nl80211_fd(void)
{
    /* not implemented yet */
    return -1;
}

bool signal_nl80211_handle(struct graph *g)
{
    (void)g;
    /* stub: nothing to do */
    return false;
}

void signal_nl80211_close(void)
{
    if (nl_fd >= 0) {
        close(nl_fd);
        nl_fd = -1;
    }
}