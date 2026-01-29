#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/genetlink.h>
#include <linux/nl80211.h>
#include <net/if.h>

#include "signal_nl80211.h"
#include "graph.h"

static int nl_fd = -1;
static int nl80211_family = -1;
static int mcast_ap = -1;

/* ------------------------------------------------------------ */

static int genl_resolve_family(int fd, const char *name)
{
    char buf[512];

    struct {
        struct nlmsghdr nlh;
        struct genlmsghdr genl;
        struct nlattr attr;
        char name[16];
    } req = {
        .nlh = {
            .nlmsg_len = NLMSG_LENGTH(GENL_HDRLEN),
            .nlmsg_type = GENL_ID_CTRL,
            .nlmsg_flags = NLM_F_REQUEST,
        },
        .genl = {
            .cmd = CTRL_CMD_GETFAMILY,
            .version = 1,
        },
        .attr = {
            .nla_type = CTRL_ATTR_FAMILY_NAME,
            .nla_len  = NLA_HDRLEN + strlen(name) + 1,
        },
    };

    strcpy(req.name, name);
    req.nlh.nlmsg_len += NLA_ALIGN(req.attr.nla_len);

    if (send(fd, &req, req.nlh.nlmsg_len, 0) < 0)
        return -1;

    ssize_t len = recv(fd, buf, sizeof(buf), 0);
    if (len < 0)
        return -1;

    for (struct nlmsghdr *nlh = (void *)buf;
         NLMSG_OK(nlh, len);
         nlh = NLMSG_NEXT(nlh, len)) {

        struct genlmsghdr *genl = NLMSG_DATA(nlh);
        struct nlattr *na = (void *)genl + GENL_HDRLEN;
        int rem = nlh->nlmsg_len - NLMSG_LENGTH(GENL_HDRLEN);

        for (; NLA_OK(na, rem); na = NLA_NEXT(na, rem)) {
            if (na->nla_type == CTRL_ATTR_FAMILY_ID)
                return *(int *)NLA_DATA(na);
        }
    }

    return -1;
}

static int genl_join_group(int fd, int family, const char *group)
{
    char buf[4096];

    struct {
        struct nlmsghdr nlh;
        struct genlmsghdr genl;
        struct nlattr attr;
        char name[32];
    } req = {
        .nlh = {
            .nlmsg_len   = NLMSG_LENGTH(GENL_HDRLEN),
            .nlmsg_type  = GENL_ID_CTRL,
            .nlmsg_flags = NLM_F_REQUEST,
        },
        .genl = {
            .cmd = CTRL_CMD_GETFAMILY,
            .version = 1,
        },
        .attr = {
            .nla_type = CTRL_ATTR_FAMILY_NAME,
            .nla_len  = NLA_HDRLEN + strlen("nl80211") + 1,
        },
    };

    strcpy(req.name, "nl80211");
    req.nlh.nlmsg_len += NLA_ALIGN(req.attr.nla_len);

    if (send(fd, &req, req.nlh.nlmsg_len, 0) < 0)
        return -1;

    ssize_t len = recv(fd, buf, sizeof(buf), 0);
    if (len < 0)
        return -1;

    for (struct nlmsghdr *nlh = (void *)buf;
         NLMSG_OK(nlh, len);
         nlh = NLMSG_NEXT(nlh, len)) {

        struct genlmsghdr *genl = NLMSG_DATA(nlh);
        struct nlattr *na = (void *)genl + GENL_HDRLEN;
        int rem = nlh->nlmsg_len - NLMSG_LENGTH(GENL_HDRLEN);

        for (; NLA_OK(na, rem); na = NLA_NEXT(na, rem)) {
            if (na->nla_type != CTRL_ATTR_MCAST_GROUPS)
                continue;

            struct nlattr *grp;
            int rem2 = NLA_PAYLOAD(na->nla_len);

            for (grp = NLA_DATA(na);
                 NLA_OK(grp, rem2);
                 grp = NLA_NEXT(grp, rem2)) {

                int id = -1;
                const char *name = NULL;

                struct nlattr *a;
                int rem3 = NLA_PAYLOAD(grp->nla_len);

                for (a = NLA_DATA(grp);
                     NLA_OK(a, rem3);
                     a = NLA_NEXT(a, rem3)) {

                    if (a->nla_type == CTRL_ATTR_MCAST_GRP_NAME)
                        name = NLA_DATA(a);
                    else if (a->nla_type == CTRL_ATTR_MCAST_GRP_ID)
                        id = *(int *)NLA_DATA(a);
                }

                if (name && id >= 0 && strcmp(name, group) == 0)
                    return setsockopt(fd, SOL_NETLINK,
                                      NETLINK_ADD_MEMBERSHIP,
                                      &id, sizeof(id));
            }
        }
    }

    return -1;
}

static int nl80211_get_mcast_id(int fd,
                               int nl80211_id,
                               const char *group)
{
    char buf[8192];
    struct nlmsghdr *nlh;
    struct genlmsghdr *genl;
    struct nlattr *tb[CTRL_ATTR_MAX + 1];

    struct {
        struct nlmsghdr nh;
        struct genlmsghdr genl;
        char attrs[256];
    } req = {
        .nh = {
            .nlmsg_len = NLMSG_LENGTH(GENL_HDRLEN),
            .nlmsg_type = GENL_ID_CTRL,
            .nlmsg_flags = NLM_F_REQUEST,
        },
        .genl = {
            .cmd = CTRL_CMD_GETFAMILY,
        },
    };

    struct nlattr *na = (struct nlattr *)req.attrs;
    na->nla_type = CTRL_ATTR_FAMILY_ID;
    na->nla_len  = NLA_HDRLEN + sizeof(uint16_t);
    *(uint16_t *)NLA_DATA(na) = nl80211_id;

    req.nh.nlmsg_len += NLA_ALIGN(na->nla_len);

    if (send(fd, &req, req.nh.nlmsg_len, 0) < 0)
        return -1;

    ssize_t len = recv(fd, buf, sizeof(buf), 0);
    if (len < 0)
        return -1;

    nlh = (struct nlmsghdr *)buf;
    genl = NLMSG_DATA(nlh);

    nla_parse(tb, CTRL_ATTR_MAX,
              genlmsg_attrdata(genl, 0),
              genlmsg_attrlen(genl, 0),
              NULL);

    struct nlattr *mcgrp;
    int rem;

    nla_for_each_nested(mcgrp, tb[CTRL_ATTR_MCAST_GROUPS], rem) {
        struct nlattr *tb2[CTRL_ATTR_MCAST_GRP_MAX + 1];

        nla_parse(tb2, CTRL_ATTR_MCAST_GRP_MAX,
                  nla_data(mcgrp),
                  nla_len(mcgrp),
                  NULL);

        if (!tb2[CTRL_ATTR_MCAST_GRP_NAME] ||
            !tb2[CTRL_ATTR_MCAST_GRP_ID])
            continue;

        const char *name =
            nla_data(tb2[CTRL_ATTR_MCAST_GRP_NAME]);

        if (!strcmp(name, group))
            return nla_get_u32(tb2[CTRL_ATTR_MCAST_GRP_ID]);
    }

    return -1;
}

static int join_mcast_group(int fd, int group_id)
{
    return setsockopt(fd, SOL_NETLINK,
                      NETLINK_ADD_MEMBERSHIP,
                      &group_id,
                      sizeof(group_id));
}

static bool nl80211_get_ifname(struct nlattr **tb, char *ifname)
{
    if (!tb[NL80211_ATTR_IFINDEX])
        return false;

    int ifindex = nla_get_u32(tb[NL80211_ATTR_IFINDEX]);
    return if_indextoname(ifindex, ifname) != NULL;
}

static bool handle_ap_event(struct graph *g,
                            struct genlmsghdr *genl,
                            struct nlattr **tb)
{
    char ifname[IFNAMSIZ];

    if (!nl80211_get_ifname(tb, ifname))
        return false;

    bool up = (genl->cmd == NL80211_CMD_START_AP);

    if (graph_get_signal(g, ifname, "beaconing") != up) {
        graph_set_signal(g, ifname, "beaconing", up);
        return true;
    }

    return false;
}

static bool handle_sta_event(struct graph *g,
                             struct genlmsghdr *genl,
                             struct nlattr **tb)
{
    char ifname[IFNAMSIZ];

    if (!nl80211_get_ifname(tb, ifname))
        return false;

    bool assoc = false;
    bool conn  = false;

    switch (genl->cmd) {
    case NL80211_CMD_CONNECT:
        assoc = true;
        conn  = true;
        break;

    case NL80211_CMD_DISCONNECT:
        assoc = false;
        conn  = false;
        break;

    default:
        return false;
    }

    bool changed = false;

    if (graph_get_signal(g, ifname, "associated") != assoc) {
        graph_set_signal(g, ifname, "associated", assoc);
        changed = true;
    }

    if (graph_get_signal(g, ifname, "connected") != conn) {
        graph_set_signal(g, ifname, "connected", conn);
        changed = true;
    }

    return changed;
}

int signal_nl80211_init(void)
{
    nl_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_GENERIC);
    if (nl_fd < 0)
        return -1;

    struct sockaddr_nl sa = {
        .nl_family = AF_NETLINK,
    };

    if (bind(nl_fd, (struct sockaddr *)&sa, sizeof(sa)) < 0)
        return -1;

    nl80211_id = genl_resolve_family(nl_fd, "nl80211");
    if (nl80211_id < 0)
        return -1;

    int mlme = nl80211_get_mcast_id(nl_fd, nl80211_id, "mlme");
    int ap   = nl80211_get_mcast_id(nl_fd, nl80211_id, "ap");

    if (mlme >= 0)
        join_mcast_group(nl_fd, mlme);

    if (ap >= 0)
        join_mcast_group(nl_fd, ap);

    return nl_fd;
}

/* ------------------------------------------------------------ */

int signal_nl80211_fd(void)
{
    if (nl_fd >= 0)
        return nl_fd;

    nl_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_GENERIC);
    if (nl_fd < 0)
        return -1;

    struct sockaddr_nl sa = {
        .nl_family = AF_NETLINK,
        .nl_pid    = getpid(),
    };

    if (bind(nl_fd, (struct sockaddr *)&sa, sizeof(sa)) < 0)
        goto fail;

    nl80211_family = genl_resolve_family(nl_fd, "nl80211");
    if (nl80211_family < 0)
        goto fail;

    /* ---- multicast wiring (REQUIRED) ---- */

    int mlme = nl80211_get_mcast_id(nl_fd, nl80211_family, "mlme");
    int ap   = nl80211_get_mcast_id(nl_fd, nl80211_family, "ap");

    if (mlme >= 0) {
        if (setsockopt(nl_fd, SOL_NETLINK,
                       NETLINK_ADD_MEMBERSHIP,
                       &mlme, sizeof(mlme)) < 0)
            goto fail;
    }

    if (ap >= 0) {
        if (setsockopt(nl_fd, SOL_NETLINK,
                       NETLINK_ADD_MEMBERSHIP,
                       &ap, sizeof(ap)) < 0)
            goto fail;
    }

    return nl_fd;

fail:
    close(nl_fd);
    nl_fd = -1;
    return -1;
}

/* ------------------------------------------------------------ */

bool signal_nl80211_handle(struct graph *g)
{
    char buf[8192];
    bool changed = false;

    ssize_t len = recv(nl_fd, buf, sizeof(buf), 0);
    if (len < 0) {
        if (errno == EINTR)
            return false;
        return false;
    }

    for (struct nlmsghdr *nlh = (void *)buf;
         NLMSG_OK(nlh, len);
         nlh = NLMSG_NEXT(nlh, len)) {

        if (nlh->nlmsg_type != nl80211_id)
            continue;

        struct genlmsghdr *genl = NLMSG_DATA(nlh);

        struct nlattr *tb[NL80211_ATTR_MAX + 1];
        memset(tb, 0, sizeof(tb));

        nla_parse(tb, NL80211_ATTR_MAX,
                  genlmsg_attrdata(genl, 0),
                  genlmsg_attrlen(genl, 0),
                  NULL);

        switch (genl->cmd) {
        case NL80211_CMD_START_AP:
        case NL80211_CMD_STOP_AP:
            changed |= handle_ap_event(g, genl, tb);
            break;

        case NL80211_CMD_CONNECT:
        case NL80211_CMD_DISCONNECT:
            changed |= handle_sta_event(g, genl, tb);
            break;

        default:
            break;
        }
    }

    return changed;
}

/* ------------------------------------------------------------ */

void signal_nl80211_close(void)
{
    if (nl_fd >= 0) {
        close(nl_fd);
        nl_fd = -1;
    }
}