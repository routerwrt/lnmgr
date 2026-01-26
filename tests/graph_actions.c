#include <assert.h>
#include <stdio.h>

#include "../src/graph.h"

static action_result_t activate_ok(struct node *n)
{
    (void)n;
    return ACTION_OK;
}

static action_result_t activate_fail(struct node *n)
{
    (void)n;
    return ACTION_FAIL;
}

static struct action_ops ok_ops = {
    .activate = activate_ok,
    .deactivate = NULL,
};

static struct action_ops fail_ops = {
    .activate = activate_fail,
    .deactivate = NULL,
};

void test_action_success(void)
{
    struct graph *g = graph_create();

    struct node *n = graph_add_node(g, "A", NODE_DEVICE);
    n->actions = &ok_ops;

    graph_enable_node(g, "A");
    graph_evaluate(g);

    assert(n->state == NODE_ACTIVE);

    graph_destroy(g);
    printf("test_action_success: OK\n");
}

void test_action_failure(void)
{
    struct graph *g = graph_create();

    struct node *n = graph_add_node(g, "A", NODE_DEVICE);
    n->actions = &fail_ops;

    graph_enable_node(g, "A");
    graph_evaluate(g);

    assert(n->state == NODE_FAILED);
    assert(n->fail_reason == FAIL_ACTION);

    struct explain e = graph_explain_node(g, "A");
    assert(e.type == EXPLAIN_FAILED);

    graph_destroy(g);
    printf("test_action_failure: OK\n");
}
