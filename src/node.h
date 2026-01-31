#ifndef LNMGR_NODE_H
#define LNMGR_NODE_H

#include <stdbool.h>
#include <stdint.h>

struct graph;

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/* ---------- semantic layer ---------- */

typedef enum {
    NODE_LINK = 0,
    NODE_L2_AGGREGATE,
    NODE_L3_NETWORK,
    NODE_SERVICE
} node_type_t;

/* ---------- concrete kind ---------- */

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

/* ---------- capabilities ---------- */

#define NKF_HAS_PORTS   (1U << 0)
#define NKF_HAS_VLANS   (1U << 1)
#define NKF_HAS_IP      (1U << 2)
#define NKF_PRODUCES_L2 (1U << 3)
#define NKF_PRODUCES_L3 (1U << 4)

/* ------------------------------
 * Feature system
 * ------------------------------ */

typedef enum {
    FEAT_NONE = 0,

    /* Topology intent */
    FEAT_MASTER,         /* “this node is enslaved to <master>” */

    /* L2 */
    FEAT_BRIDGE,         /* bridge instance settings (and bridge-wide VLAN list) */
    FEAT_BRIDGE_PORT,    /* per-port bridge VLAN membership (tagged/untagged/pvid) */

    /* Optional: explicit VLAN subif / VLAN-domain concept */
    FEAT_VLAN_DOMAIN,    /* vlan aware domain (bridge-like or standalone) */

    /* DSA / switch specifics (optional, later) */
    FEAT_DSA_PORT,       /* marks link as DSA port, cpu/user, switch id, etc. */

    FEAT_MAX
} node_feature_type_t;

struct node_feature {
    node_feature_type_t    type;
    void                   *data;
    struct node_feature    *next;
};

struct l2_vlan {
    uint16_t vid;        /* 1..4094 */
    bool     tagged;     /* false => untagged */
    bool     pvid;       /* ingress default VLAN */
    struct l2_vlan *next;
};

struct feat_master {
    struct node_feature base;

    /* config intent: the master node id */
    char *master_id;

    /* resolved pointer after graph build (optional) */
    struct node *master;
};

struct feat_bridge {
    struct node_feature base;

    /* vlan filtering + default behavior (you can add knobs later) */
    bool vlan_filtering;        /* default true for vlan-aware */
    struct l2_vlan *vlans;      /* bridge-wide allowed VLANs / membership */
};

struct feat_bridge_port {
    struct node_feature base;

    /* per-port membership within the master bridge */
    struct l2_vlan *vlans;      /* tagged/untagged/pvid per VID */
};

struct feat_vlan_domain {
    struct node_feature base;

    /* e.g. “lan1.100” semantics if you want a node representing a VLAN device */
    uint16_t vid;
    bool     reorder_hdr; /* placeholder / future */
};

struct feat_dsa_port {
    struct node_feature base;

    /* intent / classification */
    bool is_cpu_port;          /* true for cpu@ethX */
    bool is_user_port;         /* optional clarity */

    /* topology intent */
    char *link;                /* e.g. "eth0", "eth1" */
                                /* NOT resolved yet */

    /* optional future */
    char *switch_id;           /* if multiple switches exist */
};

#define FEAT_CAST(_p, _type) ((struct _type *)(_p))

struct node_feature_ops {
    node_feature_type_t type;
    const char         *name;

    int (*validate)(struct graph *g,
                    struct node *n,
                    struct node_feature *f);

    int (*resolve)(struct graph *g,
                   struct node *n,
                   struct node_feature *f);

    int (*cap_check)(struct graph *g,
                     struct node *n,
                     struct node_feature *f);
};

struct node_topology {
    /* Master/slave (generic) */
    struct node *master;        /* bridge for ports */
    struct node *slaves;        /* linked list of ports */
    struct node *slave_next;

    /* Bridge-specific */
    bool is_bridge;
    bool is_bridge_port;

    /* VLANs (resolved intent) */
    struct l2_vlan *vlans;      /* bridge-wide or per-port */
};

/* ---------- descriptor ---------- */

struct node_kind_desc {
    node_kind_t   kind;
    node_type_t   type;
    const char   *name;   /* config / JSON name */
    unsigned int  flags;
};

/* ---------- lifecycle ---------- */

typedef enum {
    NODE_INACTIVE = 0,
    NODE_WAITING,
    NODE_ACTIVE,
    NODE_FAILED
} node_state_t;

/* ---------- lookup API ---------- */

struct node_feature *
node_feature_find(struct node *n, node_feature_type_t type);

const struct node_feature_ops *
node_feature_ops_lookup(node_feature_type_t type);

const struct node_kind_desc *node_kind_lookup(node_kind_t kind);

const struct node_kind_desc *node_kind_lookup_name(const char *name);

#endif /* LNMGR_NODE_H */