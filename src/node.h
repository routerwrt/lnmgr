#ifndef LNMGR_NODE_H
#define LNMGR_NODE_H

#include <stdbool.h>

/* ------------------------------
 * Node semantic layer
 * ------------------------------ */

typedef enum {
    NODE_LINK = 0,          /* physical / virtual link endpoint */
    NODE_L2_AGGREGATE,      /* bridge, bond, lag, vlan domain */
    NODE_L3_NETWORK,        /* IP network / routing domain */
    NODE_SERVICE            /* consumers / producers of connectivity */
} node_type_t;

/* ------------------------------
 * Concrete implementation kind
 * ------------------------------ */

typedef enum {
    /* LINK */
    KIND_LINK_GENERIC = 0,
    KIND_LINK_ETHERNET,
    KIND_LINK_WIFI,
    KIND_LINK_DSA_PORT,
    KIND_LINK_LOOPBACK,
    KIND_LINK_TUN,
    KIND_LINK_TAP,
    KIND_LINK_GRE,
    KIND_LINK_VTI,
    KIND_LINK_XFRM,

    /* L2 */
    KIND_L2_BRIDGE,
    KIND_L2_BOND,
    KIND_L2_TEAM,
    KIND_L2_LAG,
    KIND_L2_VLAN_DOMAIN,

    /* L3 */
    KIND_L3_IPV4,
    KIND_L3_IPV6,
    KIND_L3_DUALSTACK,
    KIND_L3_VRF,

    /* SERVICES */
    KIND_SVC_DHCP_CLIENT,
    KIND_SVC_DHCP_SERVER,
    KIND_SVC_ROUTER,
    KIND_SVC_FIREWALL,
    KIND_SVC_VPN,
    KIND_SVC_MONITOR,

    KIND_MAX
} node_kind_t;

/* ------------------------------
 * Capability flags
 * ------------------------------ */

#define NKF_HAS_PORTS     (1U << 0)
#define NKF_HAS_VLANS     (1U << 1)
#define NKF_HAS_IP        (1U << 2)
#define NKF_PRODUCES_L2   (1U << 3)
#define NKF_PRODUCES_L3   (1U << 4)

/* ------------------------------
 * Kind descriptor
 * ------------------------------ */

struct node_kind_desc {
    node_kind_t   kind;
    node_type_t   type;
    const char   *name;     /* config / JSON name */
    unsigned int  flags;
};

/*
 * Node lifecycle states
 */
typedef enum {
    NODE_INACTIVE = 0,  /* disabled by policy / manager */
    NODE_WAITING,       /* enabled, waiting for requirements/signals */
    NODE_ACTIVE,        /* operational */
    NODE_FAILED         /* attempted activation failed */
} node_state_t;

/* Lookup helpers (implemented in enum_str.c or node.c) */
const struct node_kind_desc *node_kind_lookup(node_kind_t kind);
const struct node_kind_desc *node_kind_lookup_name(const char *name);

#endif /* LNMGR_NODE_H */