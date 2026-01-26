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

static struct node *node_create(const char *id, node_type_t type)
{
    struct node *n = calloc(1, sizeof(*n));
    if (!n)
        return NULL;

    n->id = strdup(id);
    n->type = type;
    n->enabled = false;
    n->state = NODE_INACTIVE;
    n->requires = NULL;
    n->visited = false;
    n->next = NULL;

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

    n->enabled = false;
    n->state = NODE_INACTIVE;
    return 0;
}

/*
 * Evaluation logic
 *
 * NOTE:
 * - No signals yet
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
                    if (m->enabled)
                        m->state = NODE_FAILED;
                }
                return;
            }
        }
    }

    /* existing evaluation logic */
    bool progress;
    do {
        progress = false;
        for (struct node *n = g->nodes; n; n = n->next) {
            if (!n->enabled)
                continue;

            if (n->state == NODE_WAITING &&
                requirements_met(n)) {

                n->state = NODE_ACTIVE;
                progress = true;
            }
        }
    } while (progress);
}