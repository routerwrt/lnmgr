#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if.h>

#include "signal_netlink.h"
#include "graph.h"

/* private netlink socket */
static int nl_fd = -1;

/* ------------------------------------------------------------ */

static int open_rtnetlink(void)
{
    int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (fd < 0)
        return -1;

    struct sockaddr_nl sa = {
        .nl_family = AF_NETLINK,
        .nl_groups = RTMGRP_LINK,
    };

    if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static int request_getlink(int fd)
{
    struct {
        struct nlmsghdr  nh;
        struct ifinfomsg ifm;
    } req = {
        .nh = {
            .nlmsg_len   = NLMSG_LENGTH(sizeof(struct ifinfomsg)),
            .nlmsg_type  = RTM_GETLINK,
            .nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP,
            .nlmsg_seq   = 1,
        },
        .ifm = {
            .ifi_family = AF_UNSPEC,
        },
    };

    return send(fd, &req, req.nh.nlmsg_len, 0);
}

/* ------------------------------------------------------------ */
/* link type detection                                          */

static bool link_is_bridge(struct ifinfomsg *ifi, int attrlen)
{
    struct rtattr *rta;

    for (rta = IFLA_RTA(ifi);
         RTA_OK(rta, attrlen);
         rta = RTA_NEXT(rta, attrlen)) {

        if (rta->rta_type != IFLA_LINKINFO)
            continue;

        struct rtattr *li;
        int rem = RTA_PAYLOAD(rta);

        for (li = RTA_DATA(rta);
             RTA_OK(li, rem);
             li = RTA_NEXT(li, rem)) {

            if (li->rta_type != IFLA_INFO_KIND)
                continue;

            const char *kind = RTA_DATA(li);
            return strcmp(kind, "bridge") == 0;
        }
    }

    return false;
}

/* ------------------------------------------------------------ */
/* common link → signal translation                             */

static bool clear_link_state(struct graph *g, const char *ifname)
{
    bool changed = false;

    changed |= graph_set_signal(g, ifname, "carrier",  false);
    changed |= graph_set_signal(g, ifname, "admin_up", false);
    changed |= graph_set_signal(g, ifname, "running",  false);

    return changed;
}

static bool apply_link_state(struct graph *g,
                             const char *ifname,
                             unsigned int flags,
                             bool is_bridge)
{
    bool changed = false;

    changed |= graph_set_signal(g, ifname, "carrier",
                                !!(flags & IFF_LOWER_UP));
    changed |= graph_set_signal(g, ifname, "admin_up",
                                !!(flags & IFF_UP));
    changed |= graph_set_signal(g, ifname, "running",
                                !!(flags & IFF_RUNNING));

    changed |= graph_set_signal(g, ifname, "bridge", is_bridge);

    return changed;
}

/* ------------------------------------------------------------ */

int signal_netlink_fd(void)
{
    if (nl_fd >= 0)
        return nl_fd;

    nl_fd = open_rtnetlink();
    return nl_fd;
}

/* initial RTM_GETLINK dump */
int signal_netlink_sync(struct graph *g)
{
    if (request_getlink(nl_fd) < 0)
        return -1;

    for (;;) {
        char buf[4096];

        struct iovec iov = {
            .iov_base = buf,
            .iov_len  = sizeof(buf),
        };

        struct sockaddr_nl sa;
        struct msghdr msg = {
            .msg_name    = &sa,
            .msg_namelen = sizeof(sa),
            .msg_iov     = &iov,
            .msg_iovlen  = 1,
        };

        ssize_t len = recvmsg(nl_fd, &msg, 0);
        if (len < 0)
            return -1;

        for (struct nlmsghdr *nh = (struct nlmsghdr *)buf;
             NLMSG_OK(nh, len);
             nh = NLMSG_NEXT(nh, len)) {

            if (nh->nlmsg_type == NLMSG_DONE)
                return 0;

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

            bool is_bridge = link_is_bridge(ifi, attrlen);

            /* kernel fact → signal */
            graph_set_signal(g, ifname, "bridge", is_bridge);

            apply_link_state(g, ifname, ifi->ifi_flags, is_bridge);
        }
    }
}

/* ------------------------------------------------------------ */

bool signal_netlink_handle(struct graph *g)
{
    char buf[4096];
    bool changed = false;

    struct iovec iov = {
        .iov_base = buf,
        .iov_len  = sizeof(buf),
    };

    struct sockaddr_nl sa;
    struct msghdr msg = {
        .msg_name    = &sa,
        .msg_namelen = sizeof(sa),
        .msg_iov     = &iov,
        .msg_iovlen  = 1,
    };

    ssize_t len = recvmsg(nl_fd, &msg, 0);
    if (len < 0) {
        if (errno == EINTR)
            return false;
        return false;
    }

    for (struct nlmsghdr *nh = (struct nlmsghdr *)buf;
         NLMSG_OK(nh, len);
         nh = NLMSG_NEXT(nh, len)) {

        if (nh->nlmsg_type != RTM_NEWLINK &&
            nh->nlmsg_type != RTM_DELLINK)
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

        if (nh->nlmsg_type == RTM_DELLINK) {
            changed |= clear_link_state(g, ifname);
            continue;
        }

        /* RTM_NEWLINK */
        bool is_bridge = link_is_bridge(ifi, attrlen);

        changed |= apply_link_state(g, ifname,
                            ifi->ifi_flags,
                            is_bridge);
    }

    return changed;
}

/* ------------------------------------------------------------ */

void signal_netlink_close(void)
{
    if (nl_fd >= 0) {
        close(nl_fd);
        nl_fd = -1;
    }
}