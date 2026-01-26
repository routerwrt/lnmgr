/* libc */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>

/* sockets */
#include <sys/socket.h>

/* linux netlink */
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

/* project */
#include "graph.h"


int signals_handle_netlink(struct graph *g, int nl_fd);

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

static int open_rtnetlink(void)
{
    int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (fd < 0)
        return -1;

    struct sockaddr_nl sa = {
        .nl_family = AF_NETLINK,
        .nl_groups = RTMGRP_LINK,
    };

    if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        close(fd);
        return -1;
    }

    return fd;
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
    graph_add_signal(g, ifname, "carrier");
    graph_enable_node(g, ifname);

    /* evaluate */
    graph_evaluate(g);

    printf("lnmgr: graph evaluated, running (Ctrl+C to exit)\n");

    int nl_fd = open_rtnetlink();
    if (nl_fd < 0) {
        perror("netlink");
        return 1;
    }

    while (running) {
        int rc = signals_handle_netlink(g, nl_fd);
        if (rc < 0) {
                perror("signals_handle_netlink");
                break;
        }
    }

    printf("lnmgr: shutting down\n");
    graph_destroy(g);
    return 0;
}