#include <string.h>
#include <errno.h>
#include <stdio.h>

#include "node.h"
#include "graph.h"

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

/* ---- feature op prototypes ---- */

static int feat_master_validate(struct graph *g,
                                struct node *n,
                                struct node_feature *f);

static int feat_master_resolve(struct graph *g,
                               struct node *n,
                               struct node_feature *f);

static int feat_bridge_validate(struct graph *g,
                                struct node *n,
                                struct node_feature *f);

static int feat_bridge_resolve(struct graph *g,
                               struct node *n,
                               struct node_feature *f);

static int feat_bridge_port_validate(struct graph *g,
                                     struct node *n,
                                     struct node_feature *f);

static int feat_bridge_port_resolve(struct graph *g,
                                    struct node *n,
                                    struct node_feature *f);

static const struct node_feature_ops feature_ops[] = {
    {
        .type     = FEAT_MASTER,
        .name     = "master",
        .validate = feat_master_validate,
        .resolve  = feat_master_resolve,
        .cap_check = NULL,
    },
    {
        .type     = FEAT_BRIDGE,
        .name     = "bridge",
        .validate = feat_bridge_validate,
        .resolve  = feat_bridge_resolve,
        .cap_check = NULL,
    },
    {
        .type     = FEAT_BRIDGE_PORT,
        .name     = "bridge-port",
        .validate = feat_bridge_port_validate,
        .resolve  = feat_bridge_port_resolve,
        .cap_check = NULL,
    },
};


/* helper */

struct node_feature *
node_feature_find(struct node *n, node_feature_type_t type)
{
    for (struct node_feature *f = n->features; f; f = f->next) {
        if (f->type == type)
            return f;
    }
    return NULL;
}

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

static int feat_master_validate(struct graph *g,
                                struct node *n,
                                struct node_feature *f)
{
    (void)g;
    struct feat_master *fm = (struct feat_master *)f;

    if (!fm->master_id || !fm->master_id[0])
        return FAIL_TOPOLOGY;

    if (strcmp(fm->master_id, n->id) == 0)
        return FAIL_TOPOLOGY;

    /* Only one master feature per node */
    for (struct node_feature *x = n->features; x; x = x->next) {
        if (x != f && x->type == FEAT_MASTER)
            return FAIL_TOPOLOGY;
    }

    return 0;
}

static int feat_master_resolve(struct graph *g,
                               struct node *n,
                               struct node_feature *f)
{
    struct feat_master *fm = (struct feat_master *)f;

    fm->master = graph_find_node(g, fm->master_id);
    if (!fm->master)
        return FAIL_TOPOLOGY;

    /* Wire topology */
    n->topo.master = fm->master;
    n->topo.slave_next = fm->master->topo.slaves;
    fm->master->topo.slaves = n;

    return 0;
}

static int feat_bridge_validate(struct graph *g,
                                struct node *n,
                                struct node_feature *f)
{
    (void)g;
    struct feat_bridge *fb = (struct feat_bridge *)f;

    /* Only one bridge feature per node */
    for (struct node_feature *x = n->features; x; x = x->next) {
        if (x != f && x->type == FEAT_BRIDGE)
            return FAIL_TOPOLOGY;
    }

    bool seen_pvid = false;

    for (struct l2_vlan *v = fb->vlans; v; v = v->next) {
        if (v->vid < 1 || v->vid > 4094)
            return FAIL_TOPOLOGY;

        if (v->pvid) {
            if (seen_pvid)
                return FAIL_TOPOLOGY;
            seen_pvid = true;
        }

        /* Duplicate VID check */
        for (struct l2_vlan *u = fb->vlans; u != v; u = u->next) {
            if (u->vid == v->vid)
                return FAIL_TOPOLOGY;
        }
    }

    return 0;
}

static int feat_bridge_resolve(struct graph *g,
                               struct node *n,
                               struct node_feature *f)
{
    (void)g;
    struct feat_bridge *fb = (struct feat_bridge *)f;

    n->topo.is_bridge = true;
    n->topo.vlans     = fb->vlans;

    return 0;
}

static int feat_bridge_port_validate(struct graph *g,
                                     struct node *n,
                                     struct node_feature *f)
{
    (void)g;
    struct feat_bridge_port *bp = (struct feat_bridge_port *)f;

    /* Must have a master */
    if (!node_feature_find(n, FEAT_MASTER))
        return FAIL_TOPOLOGY;

    bool seen_pvid = false;

    for (struct l2_vlan *v = bp->vlans; v; v = v->next) {
        if (v->vid < 1 || v->vid > 4094)
            return FAIL_TOPOLOGY;

        if (v->pvid) {
            if (seen_pvid)
                return FAIL_TOPOLOGY;
            seen_pvid = true;
        }

        /* Duplicate VID check */
        for (struct l2_vlan *u = bp->vlans; u != v; u = u->next) {
            if (u->vid == v->vid)
                return FAIL_TOPOLOGY;
        }
    }

    return 0;
}

static int feat_bridge_port_resolve(struct graph *g,
                                    struct node *n,
                                    struct node_feature *f)
{
    (void)g;
    (void)f;
    struct feat_master *fm =
        (struct feat_master *)node_feature_find(n, FEAT_MASTER);

    if (!fm || !fm->master)
        return FAIL_TOPOLOGY;

    struct node *br = fm->master;

    /* Master must actually be a bridge */
    if (!br->topo.is_bridge)
        return FAIL_TOPOLOGY;

    n->topo.is_bridge_port = true;
    n->topo.vlans          = ((struct feat_bridge_port *)f)->vlans;

    return 0;
}
