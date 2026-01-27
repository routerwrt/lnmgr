#include "graph.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * Internal helpers
 */

static bool dfs_cycle(struct node *n)
{
    if (n->dfs == DFS_GRAY)
        return true; /* back-edge → cycle */

    if (n->dfs == DFS_BLACK)
        return false;

    n->dfs = DFS_GRAY;

    for (struct require *r = n->requires; r; r = r->next) {
        if (dfs_cycle(r->node))
            return true;
    }

    n->dfs = DFS_BLACK;
    return false;
}

static struct signal *find_signal(struct node *n, const char *name)
{
    for (struct signal *s = n->signals; s; s = s->next) {
        if (strcmp(s->name, name) == 0)
            return s;
    }
    return NULL;
}

static struct node *node_create(const char *id, node_type_t type)
{
    struct node *n = calloc(1, sizeof(*n));
    if (!n)
        return NULL;

    n->id = strdup(id);
    n->type = type;
    n->enabled = false;
    n->activated = true;
    n->state = NODE_INACTIVE;
    n->requires = NULL;
    n->next = NULL;
    n->fail_reason = FAIL_NONE;
    n->actions = (struct action_ops *)action_ops_for_type(type);

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
                            node_type_t type)
{
    if (graph_find_node(g, id))
        return NULL;

    struct node *n = node_create(id, type);
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

int graph_set_signal(struct graph *g,
                     const char *node_id,
                     const char *signal,
                     bool value)
{
    struct node *n = graph_find_node(g, node_id);
    if (!n)
        return -1;

    struct signal *s = find_signal(n, signal);
    if (!s)
        return -1;

    s->value = value;
    return 0;
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

/*
 * Evaluation logic
 *
 * NOTE:
 * 
 * - No actions yet
 * - Requirements must be ACTIVE
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

void graph_evaluate(struct graph *g)
{
    /* reset DFS marks */
    for (struct node *n = g->nodes; n; n = n->next)
        n->dfs = DFS_WHITE;

    /* detect cycles */
    for (struct node *n = g->nodes; n; n = n->next) {
        if (n->enabled && n->dfs == DFS_WHITE) {
            if (dfs_cycle(n)) {
                /* mark all enabled nodes as FAILED */
                for (struct node *m = g->nodes; m; m = m->next) {
                    if (m->enabled) {
                        m->state = NODE_FAILED;
                        m->fail_reason = FAIL_CYCLE;
                    }
                }
                return;
            }
        }
    }

    /*  evaluation logic */
    bool progress;
    do {
        progress = false;

        for (struct node *n = g->nodes; n; n = n->next) {
            if (!n->enabled)
                continue;

            /* Phase 0: demotion on signal loss (NO deactivation) */
            if (n->state == NODE_ACTIVE && !signals_met(n)) {
                n->state = NODE_WAITING;
                progress = true;
            }

            /* Phase 1: activation (do NOT require signals) */
            if (n->state == NODE_WAITING &&
                            requirements_met(n) &&
                            !n->activated) {
                if (n->actions && n->actions->activate) {
                    action_result_t r = n->actions->activate(n);
                    if (r != ACTION_OK) {
                        n->state = NODE_FAILED;
                        n->fail_reason = FAIL_ACTION;
                        continue;
                    }
                }

                n->activated = true;
            }

            /* Phase 2: readiness (signals decide ACTIVE state) */
            if (n->state == NODE_WAITING &&
                requirements_met(n) &&
                signals_met(n)) {

                n->state = NODE_ACTIVE;
                progress = true;
            }
        }

    } while (progress);
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