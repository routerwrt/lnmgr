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

int signal_netlink_fd(void)
{
    if (nl_fd >= 0)
        return nl_fd;

    nl_fd = open_rtnetlink();
    if (nl_fd < 0)
        return -1;

    if (request_getlink(nl_fd) < 0) {
        close(nl_fd);
        nl_fd = -1;
        return -1;
    }

    /* Initial dump sync */
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
            break;

        for (struct nlmsghdr *nh = (struct nlmsghdr *)buf;
             NLMSG_OK(nh, len);
             nh = NLMSG_NEXT(nh, len)) {

            if (nh->nlmsg_type == NLMSG_DONE)
                return nl_fd;

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

            bool carrier = !!(ifi->ifi_flags & IFF_LOWER_UP);
            graph_set_signal(NULL, ifname, "carrier", carrier);
        }
    }

    return nl_fd;
}

/* ------------------------------------------------------------ */

void signal_netlink_handle(struct graph *g)
{
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
        if (errno == EINTR)
            return;
        return;
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

        bool carrier = !!(ifi->ifi_flags & IFF_LOWER_UP);
        graph_set_signal(g, ifname, "carrier", carrier);
    }
}

/* ------------------------------------------------------------ */

void signal_netlink_close(void)
{
    if (nl_fd >= 0) {
        close(nl_fd);
        nl_fd = -1;
    }
}