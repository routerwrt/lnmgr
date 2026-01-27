#ifndef LNMGR_GRAPH_H
#define LNMGR_GRAPH_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Node lifecycle states
 */
typedef enum {
    NODE_INACTIVE = 0,  /* disabled by policy / manager */
    NODE_WAITING,       /* enabled, waiting for requirements/signals */
    NODE_ACTIVE,        /* operational */
    NODE_FAILED         /* attempted activation failed */
} node_state_t;

/*
 * Node types (semantic, not kernel types)
 */
typedef enum {
    NODE_DEVICE = 0,
    NODE_BRIDGE,
    NODE_TRANSFORMER,
    NODE_SERVICE
} node_type_t;

typedef enum {
    DFS_WHITE = 0,
    DFS_GRAY,
    DFS_BLACK
} dfs_mark_t;

/* Internal failure classification (not user-visible) */
typedef enum {
    FAIL_NONE = 0,
    FAIL_CYCLE,
    FAIL_ACTION
} fail_reason_t;

/*
 * graph explain_type_t:
 * Structural reasons only.
 * No protocol, policy, or user semantics.
 * Interpretation is the responsibility of the manager layer.
 */
typedef enum {
    EXPLAIN_NONE = 0,
    EXPLAIN_DISABLED,
    EXPLAIN_BLOCKED,
    EXPLAIN_SIGNAL,
    EXPLAIN_FAILED
} explain_type_t;

typedef enum {
    ACTION_OK = 0,
    ACTION_FAIL
} action_result_t;

struct explain {
    explain_type_t type;
    const char *detail; /* blocking node id OR signal name */
};

struct signal {
    const char *name;
    bool value;
    struct signal *next;
};

struct node;

struct action_ops {
    action_result_t (*activate)(struct node *n);
    void (*deactivate)(struct node *n);
};

const struct action_ops *action_ops_for_type(node_type_t type);

/*
 * Dependency edge
 */
struct require {
    struct node *node;
    struct require *next;
};

/*
 * Graph node
 */
struct node {
    char                *id;
    node_type_t         type;
    bool                enabled;
    node_state_t        state;
    struct signal       *signals;
    struct action_ops   *actions;
    bool activated;   /* admin-up has been performed */
    fail_reason_t       fail_reason;

    struct require      *requires;

    /* Traversal bookkeeping:
     * - dfs: used only for cycle detection
     */
    dfs_mark_t          dfs;            /* cycle detection */
    struct node         *next;
};

/*
 * Graph container
 */
struct graph {
    struct node *nodes;
};

/* graph lifecycle */
struct graph *graph_create(void);
void graph_destroy(struct graph *g);

/* node management */
struct node *graph_add_node(struct graph *g,
                            const char *id,
                            node_type_t type);

int graph_del_node(struct graph *g, const char *id);

struct node *graph_find_node(struct graph *g, const char *id);

/* dependencies */
int graph_add_require(struct graph *g,
                      const char *node_id,
                      const char *require_id);

int graph_del_require(struct graph *g,
                      const char *node_id,
                      const char *require_id);

/* state control */
int graph_enable_node(struct graph *g, const char *id);
int graph_disable_node(struct graph *g, const char *id);

/* evaluation */
void graph_evaluate(struct graph *g);

struct explain graph_explain_node(struct graph *g, const char *id);

int graph_add_signal(struct graph *g,
                     const char *node_id,
                     const char *signal);

int graph_set_signal(struct graph *g,
                     const char *node_id,
                     const char *signal,
                     bool value);

int graph_flush(struct graph *g);

#ifdef LNMGR_DEBUG
void graph_debug_dump(struct graph *g);
#endif

#endif /* LNMGR_GRAPH_H */