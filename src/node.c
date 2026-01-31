#include <string.h>
#include "node.h"

/* authoritative table */
static const struct node_kind_desc kind_table[] = {
    /* LINK */
    { KIND_LINK_GENERIC,   NODE_LINK,        "link",       0 },
    { KIND_LINK_LOOPBACK,  NODE_LINK,        "loopback",   0 },
    { KIND_LINK_ETHERNET,  NODE_LINK,        "ethernet",   0 },
    { KIND_LINK_WIFI,      NODE_LINK,        "wifi",       0 },
    { KIND_LINK_DSA_PORT,  NODE_LINK,        "dsa-port",   NKF_PRODUCES_L2 },
    { KIND_LINK_LOOPBACK,  NODE_LINK,        "loopback",   0 },
    { KIND_LINK_TUN,       NODE_LINK,        "tun",        NKF_PRODUCES_L3 },
    { KIND_LINK_TAP,       NODE_LINK,        "tap",        NKF_PRODUCES_L2 },
    { KIND_LINK_GRE,       NODE_LINK,        "gre",        NKF_PRODUCES_L3 },
    { KIND_LINK_VTI,       NODE_LINK,        "vti",        NKF_PRODUCES_L3 },
    { KIND_LINK_XFRM,      NODE_LINK,        "xfrm",       NKF_PRODUCES_L3 },

    /* L2 */
    { KIND_L2_BRIDGE,      NODE_L2_AGGREGATE, "bridge",    NKF_HAS_PORTS | NKF_HAS_VLANS },
    { KIND_L2_BOND,        NODE_L2_AGGREGATE, "bond",      NKF_HAS_PORTS },
    { KIND_L2_TEAM,        NODE_L2_AGGREGATE, "team",      NKF_HAS_PORTS },
    { KIND_L2_LAG,         NODE_L2_AGGREGATE, "lag",       NKF_HAS_PORTS },
    { KIND_L2_VLAN_DOMAIN, NODE_L2_AGGREGATE, "vlan",      NKF_HAS_PORTS | NKF_HAS_VLANS },

    /* L3 */
    { KIND_L3_IPV4,        NODE_L3_NETWORK,  "ipv4",      NKF_HAS_IP },
    { KIND_L3_IPV6,        NODE_L3_NETWORK,  "ipv6",      NKF_HAS_IP },
    { KIND_L3_DUALSTACK,   NODE_L3_NETWORK,  "dualstack", NKF_HAS_IP },
    { KIND_L3_VRF,         NODE_L3_NETWORK,  "vrf",       NKF_HAS_IP },

    /* SERVICES */
    { KIND_SVC_DHCP_CLIENT, NODE_SERVICE, "dhcp-client", 0 },
    { KIND_SVC_DHCP_SERVER, NODE_SERVICE, "dhcp-server", 0 },
    { KIND_SVC_ROUTER,      NODE_SERVICE, "router",      0 },
    { KIND_SVC_FIREWALL,    NODE_SERVICE, "firewall",    0 },
    { KIND_SVC_VPN,         NODE_SERVICE, "vpn",         0 },
    { KIND_SVC_MONITOR,     NODE_SERVICE, "monitor",     0 },
};

#define KIND_TABLE_LEN (sizeof(kind_table)/sizeof(kind_table[0]))

static const struct node_feature_ops feature_ops[] = {
    /* filled incrementally */
};

const struct node_feature_ops *
node_feature_ops_lookup(node_feature_type_t type)
{
    size_t n = ARRAY_SIZE(feature_ops);

    for (size_t i = 0; i < n; i++) {
        if (feature_ops[i].type == type)
            return &feature_ops[i];
    }

    return NULL;
}

const struct node_kind_desc *node_kind_lookup(node_kind_t kind)
{
    for (size_t i = 0; i < KIND_TABLE_LEN; i++) {
        if (kind_table[i].kind == kind)
            return &kind_table[i];
    }
    return NULL;
}

const struct node_kind_desc *node_kind_lookup_name(const char *name)
{
    if (!name)
        return NULL;

    for (size_t i = 0; i < KIND_TABLE_LEN; i++) {
        if (strcmp(kind_table[i].name, name) == 0)
            return &kind_table[i];
    }
    return NULL;
}