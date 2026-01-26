#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if.h>

#include "graph.h"

/*
 * Netlink → signal bridge
 *
 * Currently:
 *  - DEVICE carrier only
 */

int signals_handle_netlink(struct graph *g, int nl_fd)
{
    char buf[4096];
    struct iovec iov = {
        .iov_base = buf,
        .iov_len = sizeof(buf),
    };
    struct sockaddr_nl sa;
    struct msghdr msg = {
        .msg_name = &sa,
        .msg_namelen = sizeof(sa),
        .msg_iov = &iov,
        .msg_iovlen = 1,
    };

    ssize_t len = recvmsg(nl_fd, &msg, 0);
    if (len < 0) {
        if (errno == EINTR)
                return 0;      /* interrupted, loop will exit */
        return -1;         /* EBADF or real error */
    }

    for (struct nlmsghdr *nh = (struct nlmsghdr *)buf;
         NLMSG_OK(nh, len);
         nh = NLMSG_NEXT(nh, len)) {

        if (nh->nlmsg_type != RTM_NEWLINK)
            continue;

        struct ifinfomsg *ifi = NLMSG_DATA(nh);
        int attrlen = nh->nlmsg_len - NLMSG_LENGTH(sizeof(*ifi));

        const char *ifname = NULL;

        for (struct rtattr *rta = IFLA_RTA(ifi);
             RTA_OK(rta, attrlen);
             rta = RTA_NEXT(rta, attrlen)) {

            if (rta->rta_type == IFLA_IFNAME) {
                ifname = RTA_DATA(rta);
                break;
            }
        }

        if (!ifname)
            continue;

        bool carrier = (ifi->ifi_flags & IFF_RUNNING);

        graph_set_signal(g, ifname, "carrier", carrier);
        graph_evaluate(g);
    }

    return 0;
}