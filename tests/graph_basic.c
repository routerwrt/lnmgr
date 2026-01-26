#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/graph.h"

/*
 * Test 1: single node enable
 */
static void test_single_node_enable(void)
{
    struct graph *g = graph_create();
    assert(g);

    struct node *n = graph_add_node(g, "eth0", NODE_DEVICE);
    assert(n);

    /* initially inactive */
    assert(n->state == NODE_INACTIVE);

    graph_enable_node(g, "eth0");
    graph_evaluate(g);

    assert(n->state == NODE_ACTIVE);

    graph_destroy(g);
    printf("test_single_node_enable: OK\n");
}

/*
 * Test 2: simple dependency chain A -> B
 */
static void test_simple_dependency(void)
{
    struct graph *g = graph_create();

    graph_add_node(g, "A", NODE_DEVICE);
    graph_add_node(g, "B", NODE_DEVICE);

    graph_add_require(g, "B", "A");

    graph_enable_node(g, "A");
    graph_enable_node(g, "B");

    graph_evaluate(g);

    struct node *A = graph_find_node(g, "A");
    struct node *B = graph_find_node(g, "B");

    assert(A->state == NODE_ACTIVE);
    assert(B->state == NODE_ACTIVE);

    graph_destroy(g);
    printf("test_simple_dependency: OK\n");
}

/*
 * Test 3: dependency blocks activation
 */
static void test_blocked_dependency(void)
{
    struct graph *g = graph_create();

    graph_add_node(g, "A", NODE_DEVICE);
    graph_add_node(g, "B", NODE_DEVICE);

    graph_add_require(g, "B", "A");

    graph_enable_node(g, "B");
    graph_evaluate(g);

    struct node *B = graph_find_node(g, "B");
    assert(B->state == NODE_WAITING);

    graph_destroy(g);
    printf("test_blocked_dependency: OK\n");
}

/*
 * Test 4: diamond dependency
 *
 *      A
 *     / \
 *    B   C
 *     \ /
 *      D
 */
static void test_diamond_dependency(void)
{
    struct graph *g = graph_create();

    graph_add_node(g, "A", NODE_DEVICE);
    graph_add_node(g, "B", NODE_DEVICE);
    graph_add_node(g, "C", NODE_DEVICE);
    graph_add_node(g, "D", NODE_DEVICE);

    graph_add_require(g, "B", "A");
    graph_add_require(g, "C", "A");
    graph_add_require(g, "D", "B");
    graph_add_require(g, "D", "C");

    graph_enable_node(g, "A");
    graph_enable_node(g, "B");
    graph_enable_node(g, "C");
    graph_enable_node(g, "D");

    graph_evaluate(g);

    assert(graph_find_node(g, "A")->state == NODE_ACTIVE);
    assert(graph_find_node(g, "B")->state == NODE_ACTIVE);
    assert(graph_find_node(g, "C")->state == NODE_ACTIVE);
    assert(graph_find_node(g, "D")->state == NODE_ACTIVE);

    graph_destroy(g);
    printf("test_diamond_dependency: OK\n");
}

/*
 * Test 5: disable forces inactive
 */
static void test_disable_node(void)
{
    struct graph *g = graph_create();

    graph_add_node(g, "A", NODE_DEVICE);
    graph_enable_node(g, "A");
    graph_evaluate(g);

    struct node *A = graph_find_node(g, "A");
    assert(A->state == NODE_ACTIVE);

    graph_disable_node(g, "A");
    graph_evaluate(g);

    assert(A->state == NODE_INACTIVE);

    graph_destroy(g);
    printf("test_disable_node: OK\n");
}

/*
 * Test 6: simple dependency cycle A <-> B
 */
static void test_simple_cycle(void)
{
    struct graph *g = graph_create();

    graph_add_node(g, "A", NODE_DEVICE);
    graph_add_node(g, "B", NODE_DEVICE);

    graph_add_require(g, "A", "B");
    graph_add_require(g, "B", "A");

    graph_enable_node(g, "A");
    graph_enable_node(g, "B");

    graph_evaluate(g);

    struct node *A = graph_find_node(g, "A");
    struct node *B = graph_find_node(g, "B");

    assert(A->state == NODE_FAILED);
    assert(B->state == NODE_FAILED);

    graph_destroy(g);
    printf("test_simple_cycle: OK\n");
}

static void test_explain_disabled(void)
{
    struct graph *g = graph_create();
    graph_add_node(g, "A", NODE_DEVICE);

    struct explain e = graph_explain_node(g, "A");
    assert(e.type == EXPLAIN_DISABLED);

    graph_destroy(g);
    printf("test_explain_disabled: OK\n");
}

static void test_explain_blocked(void)
{
    struct graph *g = graph_create();

    graph_add_node(g, "A", NODE_DEVICE);
    graph_add_node(g, "B", NODE_DEVICE);
    graph_add_require(g, "B", "A");

    graph_enable_node(g, "B");
    graph_evaluate(g);

    struct explain e = graph_explain_node(g, "B");
    assert(e.type == EXPLAIN_BLOCKED);
    assert(e.detail && strcmp(e.detail, "A") == 0);

    graph_destroy(g);
    printf("test_explain_blocked: OK\n");
}

static void test_explain_failed(void)
{
    struct graph *g = graph_create();

    graph_add_node(g, "A", NODE_DEVICE);
    graph_add_node(g, "B", NODE_DEVICE);
    graph_add_require(g, "A", "B");
    graph_add_require(g, "B", "A");

    graph_enable_node(g, "A");
    graph_enable_node(g, "B");
    graph_evaluate(g);

    struct explain e = graph_explain_node(g, "A");
    assert(e.type == EXPLAIN_FAILED);

    graph_destroy(g);
    printf("test_explain_failed: OK\n");
}

static void test_signal_blocks(void)
{
    struct graph *g = graph_create();

    graph_add_node(g, "eth0", NODE_DEVICE);
    graph_add_signal(g, "eth0", "carrier");

    graph_enable_node(g, "eth0");
    graph_evaluate(g);

    struct node *n = graph_find_node(g, "eth0");
    assert(n->state == NODE_WAITING);

    struct explain e = graph_explain_node(g, "eth0");
    assert(e.type == EXPLAIN_SIGNAL);
    assert(strcmp(e.detail, "carrier") == 0);

    graph_destroy(g);
    printf("test_signal_blocks: OK\n");
}

static void test_signal_allows(void)
{
    struct graph *g = graph_create();

    graph_add_node(g, "eth0", NODE_DEVICE);
    graph_add_signal(g, "eth0", "carrier");

    graph_enable_node(g, "eth0");
    graph_set_signal(g, "eth0", "carrier", true);
    graph_evaluate(g);

    struct node *n = graph_find_node(g, "eth0");
    assert(n->state == NODE_ACTIVE);

    graph_destroy(g);
    printf("test_signal_allows: OK\n");
}

static void test_dependency_before_signal(void)
{
    struct graph *g = graph_create();

    graph_add_node(g, "A", NODE_DEVICE);
    graph_add_node(g, "B", NODE_DEVICE);

    graph_add_require(g, "B", "A");
    graph_add_signal(g, "B", "ready");

    graph_enable_node(g, "B");
    graph_evaluate(g);

    struct explain e = graph_explain_node(g, "B");
    assert(e.type == EXPLAIN_BLOCKED);
    assert(strcmp(e.detail, "A") == 0);

    graph_destroy(g);
    printf("test_dependency_before_signal: OK\n");
}

/*
 * Main test runner
 */
int main(void)
{
    test_single_node_enable();
    test_simple_dependency();
    test_blocked_dependency();
    test_diamond_dependency();
    test_simple_cycle();
    test_explain_disabled();
    test_explain_blocked();
    test_explain_failed();
    test_signal_allows();
    test_signal_blocks();
    test_dependency_before_signal();
    test_disable_node();

    printf("All graph tests passed.\n");
    return 0;
}