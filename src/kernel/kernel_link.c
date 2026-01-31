#define _GNU_SOURCE

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include "kernel_link.h"

/* Internal helper */
static int rtnl_set_link_updown(const char *ifname, bool up)
{
    struct {
        struct nlmsghdr nh;
        struct ifinfomsg ifm;
        char attrbuf[256];
    } req = {0};

    int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (fd < 0)
        return -1;

    req.nh.nlmsg_len   = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    req.nh.nlmsg_type  = RTM_SETLINK;
    req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;

    req.ifm.ifi_family = AF_UNSPEC;
    req.ifm.ifi_change = IFF_UP;
    req.ifm.ifi_flags  = up ? IFF_UP : 0;

    struct rtattr *rta = (struct rtattr *)req.attrbuf;
    rta->rta_type = IFLA_IFNAME;
    rta->rta_len  = RTA_LENGTH(strlen(ifname) + 1);
    strcpy(RTA_DATA(rta), ifname);

    req.nh.nlmsg_len += RTA_LENGTH(strlen(ifname) + 1);

    struct sockaddr_nl sa = {
        .nl_family = AF_NETLINK
    };

    if (sendto(fd, &req, req.nh.nlmsg_len, 0,
               (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        close(fd);
        return -1;
    }

    char buf[4096];
    if (recv(fd, buf, sizeof(buf), 0) < 0) {
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

/* Public API */

int kernel_link_set_updown(const char *ifname, bool up)
{
    return rtnl_set_link_updown(ifname, up);
}

bool kernel_link_is_up(const char *ifname)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        return false;

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));

    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

    if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
        close(fd);
        return false;
    }

    close(fd);
    return !!(ifr.ifr_flags & IFF_UP);
}

bool kernel_link_exists(const char *ifname)
{
    return if_nametoindex(ifname) != 0;
}

int kernel_link_get_ifindex(const char *ifname)
{
    struct {
        struct nlmsghdr nh;
        struct ifinfomsg ifm;
        char buf[256];
    } req = {0};

    int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (fd < 0)
        return -1;

    req.nh.nlmsg_len   = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    req.nh.nlmsg_type  = RTM_GETLINK;
    req.nh.nlmsg_flags = NLM_F_REQUEST;
    req.ifm.ifi_family = AF_UNSPEC;

    struct rtattr *rta = (struct rtattr *)req.buf;
    rta->rta_type = IFLA_IFNAME;
    rta->rta_len  = RTA_LENGTH(strlen(ifname) + 1);
    strcpy(RTA_DATA(rta), ifname);
    req.nh.nlmsg_len += rta->rta_len;

    struct sockaddr_nl sa = { .nl_family = AF_NETLINK };

    if (sendto(fd, &req, req.nh.nlmsg_len, 0,
               (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        close(fd);
        return -1;
    }

    char buf[4096];
    ssize_t len = recv(fd, buf, sizeof(buf), 0);
    close(fd);

    if (len < 0)
        return -1;

    struct nlmsghdr *nh = (struct nlmsghdr *)buf;
    if (nh->nlmsg_type != RTM_NEWLINK)
        return -1;

    struct ifinfomsg *ifi = NLMSG_DATA(nh);
    return ifi->ifi_index;
}