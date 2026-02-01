#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

#include "graph.h"
#include "actions.h"
#include "enum_str.h"


static struct signal *find_signal(struct node *n, const char *name)
{
    for (struct signal *s = n->signals; s; s = s->next) {
        if (strcmp(s->name, name) == 0)
            return s;
    }
    return NULL;
}

static struct node *node_create(const char *id, node_kind_t kind)
{
    const struct node_kind_desc *kd;
    struct node *n;

    kd = node_kind_lookup(kind);
    if (!kd)
        return NULL;

    n = calloc(1, sizeof(*n));
    if (!n)
        return NULL;

    n->id        = strdup(id);
    n->kind      = kind;
    n->type      = kd->type;

    n->enabled   = false;
    n->state     = NODE_INACTIVE;
    n->activated = false;
    n->fail_reason = FAIL_NONE;

    n->requires  = NULL;
    n->actions = action_ops_for_kind(n->kind);

    return n;
}

static void node_destroy(struct node *n)
{
    struct require *r = n->requires;
    while (r) {
        struct require *tmp = r;
        r = r->next;
        free(tmp);
    }

    struct signal *s = n->signals;
    while (s) {
        struct signal *tmp = s;
        s = s->next;
        free((char *)tmp->name);
        free(tmp);
    }

    free(n->id);
    free(n);
}

/*
 * Graph lifecycle
 */

static void graph_error(struct graph *g,
                        struct node *n,
                        const char *fmt, ...)
{
    (void)g;

    va_list ap;
    va_start(ap, fmt);

    fprintf(stderr, "node '%s': ", n->id);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");

    va_end(ap);
}

struct graph *graph_create(void)
{
    return calloc(1, sizeof(struct graph));
}

void graph_destroy(struct graph *g)
{
    struct node *n = g->nodes;
    while (n) {
        struct node *tmp = n;
        n = n->next;
        node_destroy(tmp);
    }
    free(g);
}

/*
 * Node management
 */
struct node *graph_find_node(struct graph *g, const char *id)
{
    for (struct node *n = g->nodes; n; n = n->next) {
        if (strcmp(n->id, id) == 0)
            return n;
    }
    return NULL;
}

struct node *graph_add_node(struct graph *g,
                            const char *id,
                            node_kind_t kind)
{
    if (graph_find_node(g, id))
        return NULL;

    const struct node_kind_desc *kd = node_kind_lookup(kind);
    if (!kd)
        return NULL;

    struct node *n = node_create(id, kind);
    if (!n)
        return NULL;

    n->next = g->nodes;
    g->nodes = n;
    return n;
}

int graph_del_node(struct graph *g, const char *id)
{
    struct node **pp = &g->nodes;
    while (*pp) {
        if (strcmp((*pp)->id, id) == 0) {
            struct node *victim = *pp;
            *pp = victim->next;
            node_destroy(victim);
            return 0;
        }
        pp = &(*pp)->next;
    }
    return -1;
}

int graph_add_signal(struct graph *g,
                     const char *node_id,
                     const char *signal)
{
    struct node *n = graph_find_node(g, node_id);
    if (!n || !signal)
        return -1;

    if (find_signal(n, signal))
        return -1;  /* duplicate */

    struct signal *s = calloc(1, sizeof(*s));
    if (!s)
        return -1;

    s->name = strdup(signal);
    s->value = false;
    s->next = n->signals;
    n->signals = s;

    return 0;
}

/*
 * Sets a signal value on a node.
 *
 * Returns:
 *   true  - signal value changed (new or updated)
 *   false - no change or error
 */
bool graph_set_signal(struct graph *g,
                      const char *node_id,
                      const char *signal,
                      bool value)
{
    struct node *n = graph_find_node(g, node_id);
    if (!n || !signal)
        return false;

    struct signal *s = find_signal(n, signal);
    if (!s) {
        /* dynamic signal */
        s = calloc(1, sizeof(*s));
        if (!s)
            return false;

        s->name = strdup(signal);
        if (!s->name) {
            free(s);
            return false;
        }

        s->value = value;
        s->next = n->signals;
        n->signals = s;
        return true; /* NEW signal => changed */
    }

    if (s->value == value)
        return false; /* no change */

    s->value = value;
    return true;
}

int graph_flush(struct graph *g)
{
    /* Disable everything first (deactivate where appropriate) */
    for (struct node *n = g->nodes; n; n = n->next) {
        if (n->enabled)
            graph_disable_node(g, n->id);
    }

    /* Free all nodes (and attached requires/signals) */
    while (g->nodes) {
        struct node *n = g->nodes;
        g->nodes = n->next;
        node_destroy(n);
    }
    return 0;
}

/*
 * Dependencies
 */

int graph_add_require(struct graph *g,
                      const char *node_id,
                      const char *require_id)
{
    struct node *n = graph_find_node(g, node_id);
    struct node *r = graph_find_node(g, require_id);

    if (!n || !r)
        return -1;

    struct require *req = calloc(1, sizeof(*req));
    if (!req)
        return -1;

    req->node = r;
    req->next = n->requires;
    n->requires = req;
    return 0;
}

int graph_del_require(struct graph *g,
                      const char *node_id,
                      const char *require_id)
{
    struct node *n = graph_find_node(g, node_id);
    if (!n)
        return -1;

    struct require **pp = &n->requires;
    while (*pp) {
        if (strcmp((*pp)->node->id, require_id) == 0) {
            struct require *victim = *pp;
            *pp = victim->next;
            free(victim);
            return 0;
        }
        pp = &(*pp)->next;
    }
    return -1;
}

/*
 * Enable / disable
 */

int graph_enable_node(struct graph *g, const char *id)
{
    struct node *n = graph_find_node(g, id);
    if (!n)
        return -1;

    n->enabled = true;
    if (n->state == NODE_INACTIVE)
        n->state = NODE_WAITING;

    return 0;
}

int graph_disable_node(struct graph *g, const char *id)
{
    struct node *n = graph_find_node(g, id);
    if (!n)
        return -1;

    if (n->state == NODE_ACTIVE &&
        n->actions &&
        n->actions->deactivate) {
        n->actions->deactivate(n);
    }

    n->enabled = false;
    n->state = NODE_INACTIVE;
    n->activated = false;

    return 0;
}

static void node_topology_reset(struct node *n)
{
    n->topo.master     = NULL;
    n->topo.slaves     = NULL;
    n->topo.slave_next = NULL;
}

static int graph_features_validate(struct graph *g)
{
    for (struct node *n = g->nodes; n; n = n->next) {
        for (struct node_feature *f = n->features; f; f = f->next) {

            const struct node_feature_ops *ops =
                node_feature_ops_lookup(f->type);

            if (!ops) {
                graph_error(g, n,
                    "unknown feature type %d", f->type);
                return -1;
            }

            if (ops->validate) {
                if (ops->validate(g, n, f) < 0)
                    return -1;
            }
        }
    }
    return 0;
}

static int graph_features_resolve(struct graph *g)
{
    for (struct node *n = g->nodes; n; n = n->next) {
        for (struct node_feature *f = n->features; f; f = f->next) {
            const struct node_feature_ops *ops =
                node_feature_ops_lookup(f->type);

            if (!ops || !ops->resolve)
                continue;

            if (ops->resolve(g, n, f) < 0)
                return -1;
        }
    }
    return 0;
}

static int graph_features_cap_check(struct graph *g)
{
    for (struct node *n = g->nodes; n; n = n->next) {
        for (struct node_feature *f = n->features; f; f = f->next) {
            const struct node_feature_ops *ops =
                node_feature_ops_lookup(f->type);

            if (!ops || !ops->cap_check)
                continue;

            if (ops->cap_check(g, n, f) < 0)
                return -1;
        }
    }
    return 0;
}

/*
 * Evaluation logic
 *
 */
static bool requirements_met(struct node *n)
{
    for (struct require *r = n->requires; r; r = r->next) {
        if (r->node->state != NODE_ACTIVE)
            return false;
    }
    return true;
}

static bool signals_met(struct node *n)
{
    for (struct signal *s = n->signals; s; s = s->next) {
        if (!s->value)
            return false;
    }
    return true;
}

static bool graph_activate_node(struct graph *g, struct node *n)
{
    (void)g;

    if (!n->actions || !n->actions->activate)
        return true;

    if (n->actions->activate(n) != ACTION_OK) {
        n->state = NODE_FAILED;
        n->fail_reason = FAIL_ACTION;
        return false;
    }

    return true;
}

bool graph_state_machine(struct graph *g)
{
    bool changed = false;
    bool progress;

    do {
        progress = false;

        for (struct node *n = g->nodes; n; n = n->next) {
            if (!n->enabled)
                continue;

            /* 1. Demotion on signal loss */
            if (n->state == NODE_ACTIVE && !signals_met(n)) {
                n->state = NODE_WAITING;
                changed = progress = true;
                continue;
            }

            /* 2. Activation (side effects, ONCE per enable-cycle) */
            if (n->state == NODE_WAITING &&
                requirements_met(n) &&
                !n->activated) {

                if (!graph_activate_node(g, n)) {
                    n->state = NODE_FAILED;
                    n->fail_reason = FAIL_ACTION;
                    changed = true;
                    continue;
                }

                n->activated = true;
                progress = true;
            }

            /* 3. Readiness */
            if (n->state == NODE_WAITING &&
                requirements_met(n) &&
                signals_met(n)) {

                n->state = NODE_ACTIVE;
                changed = progress = true;
            }
        }

    } while (progress);

    return changed;
}

/*
 * Auto-up semantics:
 * - One-shot per kernel lifecycle
 * - No retries
 * - No admin override
 */
static bool graph_apply_auto_up(struct graph *g)
{
    bool changed = false;

    for (struct node *n = g->nodes; n; n = n->next) {

        if (!n->enabled)
            continue;

        if (!n->auto_up)
            continue;

        if (!n->present)
            continue;

        if (n->auto_latched)
            continue;

        if (n->state != NODE_INACTIVE)
            continue;

        /*
         * One-shot auto activation attempt for this lifecycle
         */
        n->state = NODE_WAITING;
        n->auto_latched = true;
        changed = true;
    }

    return changed;
}

static void graph_runtime_reset(struct graph *g)
{
    for (struct node *n = g->nodes; n; n = n->next) {
        n->activated = false;

        if (!n->enabled)
            n->state = NODE_INACTIVE;
    }
}

bool graph_evaluate(struct graph *g)
{
    bool changed = false;

    /* Phase A: reset transient runtime state */
    graph_runtime_reset(g);

    /* Phase B: intent → desired states */
    changed |= graph_apply_auto_up(g);

    /* Phase C: state machine + actions */
    changed |= graph_state_machine(g);

    return changed;
}

struct explain graph_explain_node(struct graph *g, const char *id)
{
    struct explain e = { EXPLAIN_NONE, NULL };
    struct node *n = graph_find_node(g, id);

    if (!n)
        return e;

    if (!n->enabled) {
        e.type = EXPLAIN_DISABLED;
        return e;
    }

    if (n->state == NODE_FAILED) {
        e.type = EXPLAIN_FAILED;
        return e;
    }

    if (n->state == NODE_WAITING) {

        /* 1. dependencies first */
        for (struct require *r = n->requires; r; r = r->next) {
            if (r->node->state != NODE_ACTIVE) {
                e.type = EXPLAIN_BLOCKED;
                e.detail = r->node->id;
                return e;
            }
        }

        /* 2. then signals */
        for (struct signal *s = n->signals; s; s = s->next) {
            if (!s->value) {
                e.type = EXPLAIN_SIGNAL;
                e.detail = s->name;
                return e;
            }
        }
    }

    return e;
}

static int node_cmp_id(const void *a, const void *b)
{
    const struct node *na = *(const struct node **)a;
    const struct node *nb = *(const struct node **)b;
    return strcmp(na->id, nb->id);
}

int graph_save_json(struct graph *g, int fd)
{
    size_t count = 0;
    for (struct node *n = g->nodes; n; n = n->next)
        count++;

    struct node **arr = calloc(count, sizeof(*arr));
    if (!arr)
        return -1;

    size_t i = 0;
    for (struct node *n = g->nodes; n; n = n->next)
        arr[i++] = n;

    qsort(arr, count, sizeof(*arr), node_cmp_id);

    dprintf(fd, "{ \"version\": 1, \"nodes\": [");

    bool first = true;
    for (i = 0; i < count; i++) {
        struct node *n = arr[i];

        if (!first)
            dprintf(fd, ",");
        first = false;

        dprintf(fd,
            "{ \"id\": \"%s\", \"type\": \"%s\", "
            "\"enabled\": %s, \"auto\": %s",
            n->id,
            node_kind_to_str(n->kind),
            n->enabled ? "true" : "false",
            n->auto_up ? "true" : "false");

        /* signals */
        dprintf(fd, ", \"signals\": [");
        bool sfirst = true;
        for (struct signal *s = n->signals; s; s = s->next) {
            if (!sfirst)
                dprintf(fd, ",");
            sfirst = false;
            dprintf(fd, "\"%s\"", s->name);
        }
        dprintf(fd, "]");

        /* requires */
        dprintf(fd, ", \"requires\": [");
        bool rfirst = true;
        for (struct require *r = n->requires; r; r = r->next) {
            if (!rfirst)
                dprintf(fd, ",");
            rfirst = false;
            dprintf(fd, "\"%s\"", r->node->id);
        }
        dprintf(fd, "]");

        dprintf(fd, " }");
    }

    dprintf(fd, "] }\n");
    free(arr);
    return 0;
}

static int graph_build_topology(struct graph *g)
{
    /* ---------- reset derived topology ---------- */
    for (struct node *n = g->nodes; n; n = n->next)
        node_topology_reset(n);

    /* ---------- build master/slave relationships ---------- */
    for (struct node *n = g->nodes; n; n = n->next) {

        struct feat_master *fm =
            (struct feat_master *)node_feature_find(n, FEAT_MASTER);

        if (!fm)
            continue;   /* standalone node */

        /* resolve phase guarantees this */
        if (!fm->master)
            return -EINVAL;

        struct node *master = fm->master;

        /* enforce single master */
        if (n->topo.master)
            return -EINVAL;

        /* slave → master */
        n->topo.master = master;

        /* master → slave (push front) */
        n->topo.slave_next = master->topo.slaves;
        master->topo.slaves = n;
    }

    return 0;
}

static int graph_validate_topology(struct graph *g)
{
    /* ---------- basic topology sanity ---------- */
    for (struct node *n = g->nodes; n; n = n->next) {

        /* A bridge must not have a master */
        if (n->topo.is_bridge && n->topo.master) {
            n->fail_reason = FAIL_TOPOLOGY;
            return -EINVAL;
        }

        /* A bridge port must have a master */
        if (n->topo.is_bridge_port && !n->topo.master) {
            n->fail_reason = FAIL_TOPOLOGY;
            return -EINVAL;
        }

        /* A node with a master must be a bridge port */
        if (n->topo.master && !n->topo.is_bridge_port) {
            n->fail_reason = FAIL_TOPOLOGY;
            return -EINVAL;
        }

        /* A bridge port’s master must be a bridge */
        if (n->topo.master && !n->topo.master->topo.is_bridge) {
            n->fail_reason = FAIL_TOPOLOGY;
            return -EINVAL;
        }
    }

    /* ---------- detect master/slave cycles ---------- */
    for (struct node *n = g->nodes; n; n = n->next) {
        struct node *slow = n;
        struct node *fast = n;

        while (fast && fast->topo.master) {
            slow = slow->topo.master;
            fast = fast->topo.master;
            if (fast)
                fast = fast->topo.master;

            if (slow == fast) {
                n->fail_reason = FAIL_TOPOLOGY;
                return -EINVAL;
            }
        }
    }

    return 0;
}

static struct l2_vlan *
vlan_find(struct l2_vlan *list, uint16_t vid)
{
    for (; list; list = list->next)
        if (list->vid == vid)
            return list;
    return NULL;
}

static int
vlan_inherit_from_bridge(struct node *port, struct node *bridge)
{
    for (struct l2_vlan *bv = bridge->topo.vlans; bv; bv = bv->next) {

        if (vlan_find(port->topo.vlans, bv->vid))
            continue;

        struct l2_vlan *v = calloc(1, sizeof(*v));
        if (!v)
            return -ENOMEM;

        *v = *bv;
        v->pvid = false;
        v->inherited = true;

        v->next = port->topo.vlans;
        port->topo.vlans = v;
    }

    return 0;
}

static int
vlan_apply_port_overrides(struct node *port,
                          struct feat_bridge_port *bp,
                          struct node *bridge)
{
    (void)bridge;
    
    for (struct l2_vlan *pv = bp->vlans; pv; pv = pv->next) {

        struct l2_vlan *v = vlan_find(port->topo.vlans, pv->vid);
        if (!v)
            return FAIL_TOPOLOGY;   /* port introduces unknown VLAN */

        v->tagged = pv->tagged;
        v->pvid   = pv->pvid;
        v->inherited = false;
    }

    return 0;
}

static int
vlan_resolve_pvid(struct node *port)
{
    struct l2_vlan *pvid = NULL;

    for (struct l2_vlan *v = port->topo.vlans; v; v = v->next) {

        if (v->tagged && v->pvid)
            return FAIL_TOPOLOGY;

        if (v->pvid) {
            if (pvid)
                return FAIL_TOPOLOGY;
            pvid = v;
        }
    }

    if (!pvid) {
        for (struct l2_vlan *v = port->topo.vlans; v; v = v->next) {
            if (!v->tagged) {
                v->pvid = true;
                pvid = v;
                break;
            }
        }
    }

    if (!pvid)
        return FAIL_TOPOLOGY;

    return 0;
}

static int graph_resolve_vlans(struct graph *g)
{
    for (struct node *n = g->nodes; n; n = n->next) {

        if (!n->topo.is_bridge_port)
            continue;

        struct node *br = n->topo.master;
        struct feat_bridge_port *bp =
            (struct feat_bridge_port *)node_feature_find(n, FEAT_BRIDGE_PORT);

        int r;

        r = vlan_inherit_from_bridge(n, br);
        if (r) return r;

        if (bp) {
            r = vlan_apply_port_overrides(n, bp, br);
            if (r) return r;
        }

        r = vlan_resolve_pvid(n);
        if (r) return r;
    }

    return 0;
}

int graph_prepare(struct graph *g)
{
    int r;

    /* --------------------------------------------------
     * Phase 0: reset derived / runtime state
     * -------------------------------------------------- */
    for (struct node *n = g->nodes; n; n = n->next) {
        n->fail_reason = FAIL_NONE;
        node_topology_reset(n);
    }

    /* --------------------------------------------------
     * Phase 1: feature-level validation (pure intent)
     * -------------------------------------------------- */
    r = graph_features_validate(g);
    if (r)
        return r;   /* config error, nothing to mark yet */

    /* --------------------------------------------------
     * Phase 2: feature resolution (IDs → pointers)
     * -------------------------------------------------- */
    r = graph_features_resolve(g);
    if (r)
        return r;   /* missing references, fatal */

    /* --------------------------------------------------
     * Phase 3: capability checks (kernel / platform)
     * -------------------------------------------------- */
    r = graph_features_cap_check(g);
    if (r)
        return r;   /* unsupported feature */

    /* --------------------------------------------------
     * Phase 4: build derived topology
     * -------------------------------------------------- */
    r = graph_build_topology(g);
    if (r)
        return r;   /* internal inconsistency */

    /* --------------------------------------------------
     * Phase 5: topology validation
     * (THIS is where FAIL_* is assigned)
     * -------------------------------------------------- */
    r = graph_validate_topology(g);
    if (r) {
        for (struct node *n = g->nodes; n; n = n->next) {
            if (n->fail_reason != FAIL_NONE)
                n->state = NODE_FAILED;
        }
        return r;
    }

    /* --------------------------------------------------
     * Phase 6: VLAN resolution (derived intent)
     * -------------------------------------------------- */
    r = graph_resolve_vlans(g);
    if (r) {
        for (struct node *n = g->nodes; n; n = n->next) {
            if (n->state != NODE_FAILED) {
                n->state = NODE_FAILED;
                n->fail_reason = FAIL_TOPOLOGY;
            }
        }
        return r;
    }

    return 0;
}

#ifdef LNMGR_DEBUG
void graph_debug_dump(struct graph *g)
{
    for (struct node *n = g->nodes; n; n = n->next) {
        struct explain e = graph_explain_node(g, n->id);

        printf("graph: %s state=%d explain=%d",
               n->id,
               n->state,
               e.type);

        if (e.detail)
            printf(" detail=%s", e.detail);

        printf("\n");
    }
}
#endif