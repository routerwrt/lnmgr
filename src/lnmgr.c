#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include "graph.h"

/*
 * Minimal lnmgr v0
 *
 * - static intent
 * - no config
 * - no hotplug
 * - no signals
 * - no sockets
 */

static volatile sig_atomic_t running = 1;

static void on_sigint(int sig)
{
    (void)sig;
    running = 0;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <ifname>\n", argv[0]);
        return 1;
    }

    const char *ifname = argv[1];

    signal(SIGINT, on_sigint);
    signal(SIGTERM, on_sigint);

    printf("lnmgr: bringing up interface '%s'\n", ifname);

    struct graph *g = graph_create();
    if (!g) {
        fprintf(stderr, "lnmgr: failed to create graph\n");
        return 1;
    }

    /* intent */
    graph_add_node(g, ifname, NODE_DEVICE);
    graph_enable_node(g, ifname);

    /* evaluate */
    graph_evaluate(g);

    printf("lnmgr: graph evaluated, running (Ctrl+C to exit)\n");

    /* idle loop */
    while (running) {
        pause();
    }

    printf("lnmgr: shutting down\n");
    graph_destroy(g);
    return 0;
}