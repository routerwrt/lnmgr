// SPDX-License-Identifier: MIT
#include <linux/netlink.h>
#include <linux/genetlink.h>
#include <linux/nl80211.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <net/if.h>

#include "signal_nl80211.h"
#include "graph.h"

/* ------------------------------------------------------------ */
/* globals */

static int nl_fd = -1;
static int nl80211_family = -1;

/* ------------------------------------------------------------ */
/* minimal NLA helpers (kernel ABI compatible) */

static inline bool nla_ok(const struct nlattr *nla, int rem)
{
    return rem >= (int)sizeof(*nla) &&
           nla->nla_len >= sizeof(*nla) &&
           nla->nla_len <= rem;
}

static inline struct nlattr *nla_next(const struct nlattr *nla, int *rem)
{
    int len = NLA_ALIGN(nla->nla_len);
    *rem -= len;
    return (struct nlattr *)((char *)nla + len);
}

static inline uint32_t nla_get_u32(const struct nlattr *nla)
{
    return *(uint32_t *)((char *)nla + NLA_HDRLEN);
}

/* ------------------------------------------------------------ */
/* resolve generic netlink family ID */

static int genl_resolve_family(int fd, const char *name)
{
    char buf[512];

    struct {
        struct nlmsghdr     nlh;
        struct genlmsghdr   genl;
        struct nlattr       attr;
        char                name[32];
    } req = {
        .nlh = {
            .nlmsg_len   = NLMSG_LENGTH(GENL_HDRLEN),
            .nlmsg_type  = GENL_ID_CTRL,
            .nlmsg_flags = NLM_F_REQUEST,
        },
        .genl = {
            .cmd     = CTRL_CMD_GETFAMILY,
            .version = 1,
        },
        .attr = {
            .nla_type = CTRL_ATTR_FAMILY_NAME,
        },
    };

    size_t nlen = strlen(name) + 1;
    req.attr.nla_len = NLA_HDRLEN + nlen;
    memcpy(req.name, name, nlen);

    req.nlh.nlmsg_len += NLA_ALIGN(req.attr.nla_len);

    if (send(fd, &req, req.nlh.nlmsg_len, 0) < 0)
        return -1;

    ssize_t len = recv(fd, buf, sizeof(buf), 0);
    if (len < 0)
        return -1;

    for (struct nlmsghdr *nlh = (struct nlmsghdr *)buf;
         NLMSG_OK(nlh, len);
         nlh = NLMSG_NEXT(nlh, len)) {

        struct genlmsghdr *genl = NLMSG_DATA(nlh);
        struct nlattr *na = (struct nlattr *)((char *)genl + GENL_HDRLEN);
        int rem = nlh->nlmsg_len - NLMSG_LENGTH(GENL_HDRLEN);

        for (; nla_ok(na, rem); na = nla_next(na, &rem)) {
            if (na->nla_type == CTRL_ATTR_FAMILY_ID)
                return *(int *)((char *)na + NLA_HDRLEN);
        }
    }

    return -1;
}

/* ------------------------------------------------------------ */
/* socket init */

int signal_nl80211_fd(void)
{
    if (nl_fd >= 0)
        return nl_fd;

    nl_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_GENERIC);
    if (nl_fd < 0)
        return -1;

    struct sockaddr_nl sa = {
        .nl_family = AF_NETLINK,
    };

    if (bind(nl_fd, (struct sockaddr *)&sa, sizeof(sa)) < 0)
        goto fail;

    nl80211_family = genl_resolve_family(nl_fd, "nl80211");
    if (nl80211_family < 0)
        goto fail;

    return nl_fd;

fail:
    close(nl_fd);
    nl_fd = -1;
    return -1;
}

int signal_nl80211_sync(struct graph *g)
{
    (void)g;
    return 0;
}

/* ------------------------------------------------------------ */
/* event handler */

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

    for (struct nlmsghdr *nlh = (struct nlmsghdr *)buf;
         NLMSG_OK(nlh, len);
         nlh = NLMSG_NEXT(nlh, len)) {

        if (nlh->nlmsg_type != nl80211_family)
            continue;

        struct genlmsghdr *genl = NLMSG_DATA(nlh);

        /* --- extract ifname --- */
        char ifname[IF_NAMESIZE];
        bool have_ifname = false;

        struct nlattr *na =
            (struct nlattr *)((char *)genl + GENL_HDRLEN);
        int rem = nlh->nlmsg_len - NLMSG_LENGTH(GENL_HDRLEN);

        for (; nla_ok(na, rem); na = nla_next(na, &rem)) {
            if (na->nla_type == NL80211_ATTR_IFINDEX) {
                int ifindex = nla_get_u32(na);
                if (if_indextoname(ifindex, ifname))
                    have_ifname = true;
            }
        }

        if (!have_ifname)
            continue;

        struct node *n = graph_find_node(g, ifname);
        if (!n || !n->present)
            continue;

        switch (genl->cmd) {

        /* --- AP events --- */
        case NL80211_CMD_START_AP:
        case NL80211_CMD_STOP_AP: {
            bool up = (genl->cmd == NL80211_CMD_START_AP);

            changed |= graph_set_signal(g, ifname, "beaconing", up);
            break;
        }

        /* --- STA events --- */
        case NL80211_CMD_CONNECT:
        case NL80211_CMD_DISCONNECT: {
            bool assoc = (genl->cmd == NL80211_CMD_CONNECT);
            bool conn  = assoc;

            changed |= graph_set_signal(g, ifname, "associated", assoc);
            changed |= graph_set_signal(g, ifname, "connected",  conn);
            break;
        }

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