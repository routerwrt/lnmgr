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
        .nl_pid = getpid(),
    };

    if (bind(nl_fd, (struct sockaddr *)&sa, sizeof(sa)) < 0)
        goto fail;

    nl80211_family = genl_resolve_family(nl_fd, "nl80211");
    if (nl80211_family < 0)
        goto fail;

    /* AP multicast group is stable name */
    /* For now we skip resolving group IDs and rely on unicast events */
    /* Multicast join can be added next */

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

        struct genlmsghdr *genl = NLMSG_DATA(nlh);

        if (genl->cmd != NL80211_CMD_START_AP &&
            genl->cmd != NL80211_CMD_STOP_AP)
            continue;

        struct nlattr *na = (void *)genl + GENL_HDRLEN;
        int rem = nlh->nlmsg_len - NLMSG_LENGTH(GENL_HDRLEN);

        int ifindex = -1;

        for (; NLA_OK(na, rem); na = NLA_NEXT(na, rem)) {
            if (na->nla_type == NL80211_ATTR_IFINDEX)
                ifindex = *(int *)NLA_DATA(na);
        }

        if (ifindex < 0)
            continue;

        char ifname[IFNAMSIZ];
        if (!if_indextoname(ifindex, ifname))
            continue;

        bool up = (genl->cmd == NL80211_CMD_START_AP);

        if (graph_set_signal(g, ifname, "beaconing", up) == 0)
            changed = true;
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