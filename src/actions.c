#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>

#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if.h>

#include "graph.h"
#include "actions.h"

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

    /* wait for ACK */
    char buf[4096];
    if (recv(fd, buf, sizeof(buf), 0) < 0) {
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

/* ---- DEVICE ---- */

static action_result_t device_activate(struct node *n)
{
    if (rtnl_set_link_updown(n->id, true) < 0)
        return ACTION_FAIL;

    return ACTION_OK;
}

static void device_deactivate(struct node *n)
{
    rtnl_set_link_updown(n->id, false);
}

static const struct action_ops device_ops = {
    .activate = device_activate,
    .deactivate = device_deactivate,
};

/* ---- BRIDGE ---- */

static action_result_t bridge_activate(struct node *n)
{
    (void)n;
    return ACTION_OK;
}

static void bridge_deactivate(struct node *n)
{
    (void)n;
}

static const struct action_ops bridge_ops = {
    .activate = bridge_activate,
    .deactivate = bridge_deactivate,
};

/* ---- BOND ---- */

static action_result_t bond_activate(struct node *n)
{
    (void)n;
    return ACTION_OK;
}

static void bond_deactivate(struct node *n)
{
    (void)n;
}

static const struct action_ops bond_ops = {
    .activate = bond_activate,
    .deactivate = bond_deactivate,
};

const struct action_ops *action_ops_for_kind(node_kind_t kind)
{
    switch (kind) {
    case KIND_LINK_ETHERNET:
    case KIND_LINK_WIFI:
    case KIND_LINK_DSA_PORT:
        return &device_ops;

    case KIND_L2_BRIDGE:
        return &bridge_ops;

    case KIND_L2_BOND:
        return &bond_ops;

    default:
        return NULL;
    }
}