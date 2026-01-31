#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>

#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if_bridge.h>   /* struct bridge_vlan_info + flags */
#include <linux/if_link.h>     /* IFLA_* */
#include <unistd.h>

#include "kernel_bridge.h"
#include "kernel_link.h"

static int rtnl_simple_link_op(struct nlmsghdr *nh)
{
    struct sockaddr_nl sa = {
        .nl_family = AF_NETLINK,
    };

    int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (fd < 0)
        return -1;

    if (sendto(fd, nh, nh->nlmsg_len, 0,
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

/* ------------------------------------------------------------ */
/* bridge lifecycle */

int kernel_bridge_create(const char *br)
{
    if (kernel_link_exists(br))
        return 0;

    struct {
        struct nlmsghdr nh;
        struct ifinfomsg ifm;
        char buf[256];
    } req = {0};

    req.nh.nlmsg_len   = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    req.nh.nlmsg_type  = RTM_NEWLINK;
    req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL | NLM_F_ACK;

    req.ifm.ifi_family = AF_UNSPEC;

    struct rtattr *rta;

    rta = (struct rtattr *)req.buf;
    rta->rta_type = IFLA_IFNAME;
    rta->rta_len  = RTA_LENGTH(strlen(br) + 1);
    strcpy(RTA_DATA(rta), br);

    req.nh.nlmsg_len += rta->rta_len;

    rta = (struct rtattr *)((char *)rta + rta->rta_len);
    rta->rta_type = IFLA_LINKINFO;
    rta->rta_len  = RTA_LENGTH(0);

    struct rtattr *kind =
        (struct rtattr *)((char *)rta + RTA_LENGTH(0));
    kind->rta_type = IFLA_INFO_KIND;
    kind->rta_len  = RTA_LENGTH(strlen("bridge") + 1);
    strcpy(RTA_DATA(kind), "bridge");

    rta->rta_len += kind->rta_len;
    req.nh.nlmsg_len += rta->rta_len;

    return rtnl_simple_link_op(&req.nh);
}

int kernel_bridge_delete(const char *br)
{
    if (!kernel_link_exists(br))
        return 0;

    struct {
        struct nlmsghdr nh;
        struct ifinfomsg ifm;
        char buf[128];
    } req = {0};

    req.nh.nlmsg_len   = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    req.nh.nlmsg_type  = RTM_DELLINK;
    req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;

    struct rtattr *rta = (struct rtattr *)req.buf;
    rta->rta_type = IFLA_IFNAME;
    rta->rta_len  = RTA_LENGTH(strlen(br) + 1);
    strcpy(RTA_DATA(rta), br);

    req.nh.nlmsg_len += rta->rta_len;

    return rtnl_simple_link_op(&req.nh);
}

/* ------------------------------------------------------------ */
/* admin */

int kernel_bridge_set_up(const char *br)
{
    return kernel_link_set_updown(br, true);
}

/* ------------------------------------------------------------ */
/* vlan filtering */


int kernel_bridge_get_vlan_filtering(const char *br, bool *enabled)
{
    int br_ifindex = kernel_link_get_ifindex(br);
    if (br_ifindex < 0)
        return -ENOENT;

    struct {
        struct nlmsghdr nh;
        struct ifinfomsg ifm;
    } req = {0};

    req.nh.nlmsg_len   = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    req.nh.nlmsg_type  = RTM_GETLINK;
    req.nh.nlmsg_flags = NLM_F_REQUEST;
    req.ifm.ifi_family = AF_UNSPEC;
    req.ifm.ifi_index  = br_ifindex;

    struct sockaddr_nl sa = { .nl_family = AF_NETLINK };
    int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (fd < 0)
        return -1;

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
    struct ifinfomsg *ifi = NLMSG_DATA(nh);

    int attrlen = nh->nlmsg_len - NLMSG_LENGTH(sizeof(*ifi));
    for (struct rtattr *rta = IFLA_RTA(ifi);
         RTA_OK(rta, attrlen);
         rta = RTA_NEXT(rta, attrlen)) {

        if (rta->rta_type == IFLA_AF_SPEC) {
            int alen = RTA_PAYLOAD(rta);
            for (struct rtattr *a = RTA_DATA(rta);
                 RTA_OK(a, alen);
                 a = RTA_NEXT(a, alen)) {

                if (a->rta_type == IFLA_BR_VLAN_FILTERING) {
                    *enabled = !!*(uint8_t *)RTA_DATA(a);
                    return 0;
                }
            }
        }
    }

    return -ENOENT;
}

int kernel_bridge_set_vlan_filtering(const char *br, bool enable)
{
    int br_ifindex = kernel_link_get_ifindex(br);
    if (br_ifindex < 0)
        return -ENOENT;

    struct {
        struct nlmsghdr nh;
        struct ifinfomsg ifm;
        char buf[256];
    } req = {0};

    req.nh.nlmsg_len   = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    req.nh.nlmsg_type  = RTM_SETLINK;
    req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
    req.ifm.ifi_family = AF_UNSPEC;
    req.ifm.ifi_index  = br_ifindex;

    struct rtattr *af =
        (struct rtattr *)req.buf;
    af->rta_type = IFLA_AF_SPEC;
    af->rta_len  = RTA_LENGTH(0);

    struct rtattr *vf =
        (struct rtattr *)((char *)af + RTA_LENGTH(0));
    vf->rta_type = IFLA_BR_VLAN_FILTERING;
    vf->rta_len  = RTA_LENGTH(sizeof(uint8_t));
    *(uint8_t *)RTA_DATA(vf) = enable ? 1 : 0;

    af->rta_len += vf->rta_len;
    req.nh.nlmsg_len += af->rta_len;

    return rtnl_simple_link_op(&req.nh);
}

/* ------------------------------------------------------------ */
/* ports */

static int rtnl_set_master(int ifindex, int master)
{
    struct {
        struct nlmsghdr nh;
        struct ifinfomsg ifm;
        char buf[64];
    } req = {0};

    req.nh.nlmsg_len   = NLMSG_LENGTH(sizeof(req.ifm));
    req.nh.nlmsg_type  = RTM_SETLINK;
    req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;

    req.ifm.ifi_index = ifindex;

    struct rtattr *rta = (struct rtattr *)req.buf;
    rta->rta_type = IFLA_MASTER;
    rta->rta_len  = RTA_LENGTH(sizeof(uint32_t));
    *(uint32_t *)RTA_DATA(rta) = master;

    req.nh.nlmsg_len += rta->rta_len;

    return rtnl_simple_link_op(&req.nh);
}

int kernel_bridge_add_port(const char *bridge, const char *port)
{
    int br_ifindex   = kernel_link_get_ifindex(bridge);
    int port_ifindex = kernel_link_get_ifindex(port);

    if (br_ifindex <= 0 || port_ifindex <= 0)
        return -ENOENT;

    return rtnl_set_master(port_ifindex, br_ifindex);
}

int kernel_bridge_del_port(const char *bridge, const char *port)
{
    (void)bridge; /* not needed */

    int port_ifindex = kernel_link_get_ifindex(port);
    if (port_ifindex <= 0)
        return -ENOENT;

    return rtnl_set_master(port_ifindex, 0);
}

 /* For bridge VLAN ops:
 *  - add uses RTM_SETLINK
 *  - del uses RTM_DELLINK
 * Payload: IFLA_AF_SPEC { IFLA_BRIDGE_VLAN_INFO = struct bridge_vlan_info }
 */
static int rtnl_bridge_vlan_modify(int ifindex,
                                   uint16_t vid,
                                   bool tagged,
                                   bool pvid,
                                   bool master_too,
                                   bool add)
{
    struct {
        struct nlmsghdr nh;
        struct ifinfomsg ifm;
        char buf[256];
    } req;

    memset(&req, 0, sizeof(req));

    req.nh.nlmsg_len   = NLMSG_LENGTH(sizeof(req.ifm));
    req.nh.nlmsg_type  = add ? RTM_SETLINK : RTM_DELLINK;
    req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;

    req.ifm.ifi_family = AF_UNSPEC;
    req.ifm.ifi_index  = ifindex;

    /* Outer: IFLA_AF_SPEC (nested) */
    struct rtattr *af = (struct rtattr *)req.buf;
    af->rta_type = IFLA_AF_SPEC;
    af->rta_len  = RTA_LENGTH(0);

    /* Inner: IFLA_BRIDGE_VLAN_INFO (binary) */
    struct bridge_vlan_info vinfo;
    memset(&vinfo, 0, sizeof(vinfo));
    vinfo.vid = vid;

    /*
     * Flags meaning:
     *  - MASTER: also operate on the bridge device (when dev is a port)
     *  - UNTAGGED: egress untagged
     *  - PVID: ingress default VLAN (must not be set with tagged)
     *
     * For DEL, the kernel only needs vid (+ MASTER if you want br_vlan_delete too).
     * Keeping UNTAGGED/PVID off for DEL is safest.
     */
    if (add) {
        if (master_too)
            vinfo.flags |= BRIDGE_VLAN_INFO_MASTER;
        if (!tagged)
            vinfo.flags |= BRIDGE_VLAN_INFO_UNTAGGED;
        if (pvid)
            vinfo.flags |= BRIDGE_VLAN_INFO_PVID;
    } else {
        if (master_too)
            vinfo.flags |= BRIDGE_VLAN_INFO_MASTER;
    }

    struct rtattr *v = (struct rtattr *)(req.buf + RTA_ALIGN(af->rta_len));
    v->rta_type = IFLA_BRIDGE_VLAN_INFO;
    v->rta_len  = RTA_LENGTH(sizeof(vinfo));
    memcpy(RTA_DATA(v), &vinfo, sizeof(vinfo));

    af->rta_len += RTA_ALIGN(v->rta_len);
    req.nh.nlmsg_len += RTA_ALIGN(af->rta_len);

    return rtnl_simple_link_op(&req.nh);
}

int kernel_bridge_vlan_add(const char *bridge,
                           const char *port,
                           uint16_t vid,
                           bool tagged,
                           bool pvid)
{
    int port_ifindex = kernel_link_get_ifindex(port);
    if (port_ifindex <= 0)
        return -ENOENT;

    /*
     * If you want “port add also ensures bridge has VLAN” behavior,
     * set master_too=true. If you manage bridge VLANs separately, set false.
     */
    bool master_too = true;
    (void)bridge;

    return rtnl_bridge_vlan_modify(port_ifindex, vid, tagged, pvid, master_too, true);
}

int kernel_bridge_vlan_del(const char *bridge,
                           const char *port,
                           uint16_t vid)
{
    int port_ifindex = kernel_link_get_ifindex(port);
    if (port_ifindex <= 0)
        return -ENOENT;

    bool master_too = true;
    (void)bridge;

    /* tagged/pvid are irrelevant for DEL */
    return rtnl_bridge_vlan_modify(port_ifindex, vid, false, false, master_too, false);
}