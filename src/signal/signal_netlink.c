#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <stdio.h>
#include <fcntl.h>     /* fcntl, F_GETFL, F_SETFL, O_NONBLOCK */
#include <unistd.h>   /* close */
#include <poll.h>

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

    /* ---- CRITICAL: increase receive buffer ---- */
    int rcvbuf = 4 * 1024 * 1024;   /* 4 MB */
    if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF,
                   &rcvbuf, sizeof(rcvbuf)) < 0) {
        perror("setsockopt(SO_RCVBUF)");
        close(fd);
        return -1;
    }

    /* make non-blocking */
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(fd);
        return -1;
    }

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
                             unsigned int flags)
{
    bool changed = false;

    changed |= graph_set_signal(g, ifname, "carrier",
                                !!(flags & IFF_LOWER_UP));
    changed |= graph_set_signal(g, ifname, "admin_up",
                                !!(flags & IFF_UP));
    changed |= graph_set_signal(g, ifname, "running",
                                !!(flags & IFF_RUNNING));

    DPRINTF("link %s: carrier=%d admin=%d running=%d\n",
            ifname,
            !!(flags & IFF_LOWER_UP),
            !!(flags & IFF_UP),
            !!(flags & IFF_RUNNING));

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

static void drain_netlink_socket(int fd)
{
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

        ssize_t len = recvmsg(fd, &msg, 0);
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return;
            return;
        }
    }
}

/* initial RTM_GETLINK dump */
int signal_netlink_sync(struct graph *g)
{
    drain_netlink_socket(nl_fd);

    if (request_getlink(nl_fd) < 0)
        return -1;

    bool done = false;

    while (!done) {
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
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* wait for more dump data */
                struct pollfd pfd = {
                    .fd = nl_fd,
                    .events = POLLIN,
                };
                poll(&pfd, 1, -1);
                continue;
            }
            return -1;
        }

        for (struct nlmsghdr *nh = (struct nlmsghdr *)buf;
             NLMSG_OK(nh, len);
             nh = NLMSG_NEXT(nh, len)) {

            if (nh->nlmsg_type == NLMSG_DONE) {
                done = true;
                break;
            }

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

            if (ifname)
                apply_link_state(g, ifname, ifi->ifi_flags);
        }
    }

    return 0;
}

/* ------------------------------------------------------------ */

bool signal_netlink_handle(struct graph *g)
{
    bool changed = false;

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
        if (len < 0) {
            if (errno == ENOBUFS) {
                /* kernel dropped messages */
                signal_netlink_sync(g);
                return true;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;

            return changed;
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

            changed |= apply_link_state(g, ifname, ifi->ifi_flags);
        }
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