#include "graph.h"
#include "actions.h"

#include "kernel/kernel_link.h"
#include "kernel/kernel_bridge.h"

/* ---- DEVICE ---- */

static action_result_t device_activate(struct node *n)
{
    if (kernel_link_set_updown(n->id, true) < 0)
        return ACTION_FAIL;

    return ACTION_OK;
}

static void device_deactivate(struct node *n)
{
    kernel_link_set_updown(n->id, false);
}

/* ---- BRIDGE ---- */

static action_result_t bridge_activate(struct node *n)
{
    struct feat_bridge *fb = (struct feat_bridge *)
                        node_feature_find(n, FEAT_BRIDGE);

    if (!kernel_link_exists(n->id))
        kernel_bridge_create(n->id);

    if (fb->vlan_filtering)
        kernel_bridge_set_vlan_filtering(n->id, true);

    kernel_link_set_up(n->id);
    return ACTION_OK;
}


static void bridge_deactivate(struct node *n)
{
    (void)n;
}

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

/* ---- BRIDGE PORT ---- */

static action_result_t bridge_port_activate(struct node *n)
{
    struct feat_master *fm = (struct feat_master *)
                node_feature_find(n, FEAT_MASTER);

    if (!fm || !fm->master)
        return ACTION_FAIL;

    struct node *br = fm->master;

    /* Bridge must be a bridge */
    if (!br->topo.is_bridge)
        return ACTION_FAIL;

    /* 1. Enslave port to bridge (idempotent) */
    if (kernel_bridge_add_port(br->id, n->id) < 0)
        return ACTION_FAIL;

    /* 2. Ensure port admin UP */
    if (!kernel_link_is_up(n->id)) {
        if (kernel_link_set_up(n->id) < 0)
            return ACTION_FAIL;
    }

    /* 3. Program VLANs (resolved intent) */
    for (struct l2_vlan *v = n->topo.vlans; v; v = v->next) {

        /* Skip inherited-only entries if you ever add that flag */
        /* if (v->inherited)
            continue; */

        if (kernel_bridge_vlan_add(
                br->id,
                n->id,
                v->vid,
                v->tagged,
                v->pvid) < 0)
            return ACTION_FAIL;
    }

    return ACTION_OK;
}

static const struct action_ops device_ops = {
    .activate = device_activate,
    .deactivate = device_deactivate,
};

static const struct action_ops bridge_ops = {
    .activate = bridge_activate,
    .deactivate = bridge_deactivate,
};

static const struct action_ops bond_ops = {
    .activate = bond_activate,
    .deactivate = bond_deactivate,
};

static const struct action_ops bridge_port_ops = {
    .activate   = bridge_port_activate,
    .deactivate = NULL,   /* kernel handles teardown */
};

const struct action_ops *
action_ops_for_kind(node_kind_t kind)
{
    switch (kind) {
    case KIND_LINK_ETHERNET:
    case KIND_LINK_WIFI:
    case KIND_LINK_DSA_PORT:
        return &device_ops;

    case KIND_L2_BRIDGE:
        return &bridge_ops;

    case KIND_L2_BRIDGE_PORT:
        return &bridge_port_ops;

    case KIND_L2_BOND:
        return &bond_ops;

    default:
        return NULL;
    }
}