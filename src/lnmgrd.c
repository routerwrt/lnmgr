/* libc */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

/* sockets */
#include <sys/socket.h>

/* linux netlink */
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

/* project */
#include "graph.h"
#include "lnmgr_status.h"

/*
 * Minimal lnmgr v0
 *
 * - static intent
 * - no config
 * - no hotplug
 * - no signals
 * - no sockets
 */

int signals_handle_netlink(struct graph *g, int nl_fd,
                           const char *watch_if,
                           bool *admin_up_out);

static int nl_fd = -1;
static volatile sig_atomic_t running = 1;

static int netlink_request_getlink(int nl_fd)
{
    struct {
        struct nlmsghdr nh;
        struct ifinfomsg ifm;
    } req;

    memset(&req, 0, sizeof(req));

    req.nh.nlmsg_len   = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    req.nh.nlmsg_type  = RTM_GETLINK;
    req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    req.nh.nlmsg_seq   = 1;

    req.ifm.ifi_family = AF_UNSPEC;

    return send(nl_fd, &req, req.nh.nlmsg_len, 0);
}

static void on_sigint(int sig)
{
    (void)sig;
    running = 0;

    if (nl_fd >= 0) {
        close(nl_fd);
        nl_fd = -1;
    }
}

static void setup_signals(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;   /* NO SA_RESTART */

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
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

    setup_signals();

    signal(SIGINT, on_sigint);
    signal(SIGTERM, on_sigint);

    printf("lnmgr: bringing up interface '%s'\n", ifname);

    struct graph *g = graph_create();
    if (!g) {
        fprintf(stderr, "lnmgr: failed to create graph\n");
        return 1;
    }
    /* for testing only - until config parser done*/
    /* testing-only heuristic */
    bool require_carrier = strncmp(ifname, "eth", 3) == 0;

    /* intent */
    graph_add_node(g, ifname, NODE_DEVICE);

    /* policy (temporary, until config exists) */
    if (require_carrier) {
       graph_add_signal(g, ifname, "carrier");
    }

    graph_enable_node(g, ifname);

    /* evaluate */
    graph_evaluate(g);

    printf("lnmgr: graph evaluated, running (Ctrl+C to exit)\n");
    
    nl_fd = open_rtnetlink();
    if (nl_fd < 0) {
        perror("netlink");
        return 1;
    }

    if (netlink_request_getlink(nl_fd) < 0) {
        perror("RTM_GETLINK");
        return 1;
    }
    /* Initial link state sync */
    bool admin_up = false;

    for (;;) {
        int rc = signals_handle_netlink(g, nl_fd, ifname, &admin_up);
        if (rc < 0) {
            perror("signals_handle_netlink");
            return 1;
        }
        if (rc == 1)
            break; /* NLMSG_DONE */
    }

    while (running) {
        int rc = signals_handle_netlink(g, nl_fd, ifname, &admin_up);
        if (rc < 0) {
            perror("signals_handle_netlink");
            break;
        }

        struct explain gex = graph_explain_node(g, ifname);
        struct lnmgr_explain lex = lnmgr_status_from_graph(&gex, admin_up);

        printf("lnmgrd: %s -> %s", ifname, lnmgr_status_str(lex.status));
        if (lex.code)
            printf(" (%s)", lex.code);
        printf("\n");
    }

    printf("lnmgr: shutting down\n");
    graph_destroy(g);
    return 0;
}