#include <assert.h>
#include <stdio.h>

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
    test_disable_node();

    printf("All graph tests passed.\n");
    return 0;
}