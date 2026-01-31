/*
 * lnmgrd — Link Manager Daemon
 *
 * lnmgrd is a minimal, event-driven network link manager for Linux.
 *
 * Responsibilities:
 *  - Owns the in-memory dependency graph of network objects (devices, links,
 *    bridges, tunnels, services).
 *  - Reacts to kernel events (netlink) and external signals.
 *  - Evaluates link readiness based on explicit dependencies and signals.
 *  - Executes activation/deactivation actions when graph state changes.
 *  - Exposes read-only introspection via a local UNIX control socket.
 *
 * Design principles:
 *  - Single-threaded, deterministic event loop.
 *  - Kernel-facing logic (netlink) separated from policy and presentation.
 *  - No implicit policy: only explicit configuration and signals.
 *  - No background retries, timers, or heuristics.
 *  - No dependency on systemd, dbus, or external frameworks.
 *
 * Non-goals:
 *  - No dynamic policy engine.
 *  - No automatic network configuration or probing.
 *  - No UI logic or user interaction.
 *  - No long-lived client connections.
 *
 * The daemon is intentionally small and conservative. Higher-level behavior
 * (CLI, policy, orchestration, UI) is implemented outside of lnmgrd via the
 * control socket.
 */

/* libc */
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <poll.h>
#include <fcntl.h>


/* sockets */
#include <sys/socket.h>

/* linux netlink */
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

/* project */
#include "graph.h"
#include "config.h"
#include "socket.h"
#include "signal/signal_netlink.h"
#include "signal/signal_nl80211.h"

#define LNMGR_SOCKET_PATH "/run/lnmgr.sock"

static int sigpipe[2] = { -1, -1 };
/*
 * Minimal lnmgr v0
 *
 * - static intent
 * - no config
 * - no hotplug
 * - no signals
 * - no sockets
 */

int signals_handle_netlink(struct graph *g, int nl_fd);

static volatile sig_atomic_t running = 1;

static void on_sigint(int signo)
{
    (void)signo;
    running = false;

    if (sigpipe[1] >= 0) {
        /* wake poll */
        ssize_t r = write(sigpipe[1], "x", 1);
        (void)r;
    }
}

static void setup_signals(void)
{
    if (pipe(sigpipe) < 0) {
        perror("pipe");
        exit(1);
    }

    if (fcntl(sigpipe[0], F_SETFL, O_NONBLOCK) < 0)
        perror("fcntl(sigpipe[0])");

    if (fcntl(sigpipe[1], F_SETFL, O_NONBLOCK) < 0)
        perror("fcntl(sigpipe[1])");

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;   /* NO SA_RESTART */

    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    signal(SIGPIPE, SIG_IGN);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <config.json>\n", argv[0]);
        return 1;
    }

    setup_signals();

    struct graph *g = graph_create();
    if (!g) {
        perror("graph_create");
        return 1;
    }

    if (config_load_file(g, argv[1]) < 0) {
        perror("config_load_file");
        graph_destroy(g);
        return 1;
    }

    if (graph_prepare(g) < 0) {
        fprintf(stderr, "invalid configuration\n");

        /* mark all enabled nodes as FAILED(TOPOLOGY) */
        for (struct node *n = g->nodes; n; n = n->next) {
            if (n->enabled) {
                n->state = NODE_FAILED;
                n->fail_reason = FAIL_TOPOLOGY;
            }
        }
    }

    /* ---------- control + signal init ---------- */

    int ctl_fd  = socket_listen(LNMGR_SOCKET_PATH);
    int nl_fd   = signal_netlink_fd();
    int wifi_fd = signal_nl80211_fd();

    if (ctl_fd < 0 || nl_fd < 0) {
        perror("initialization failed");
        graph_destroy(g);
        return 1;
    }

    /* establish initial facts */
    signal_netlink_sync(g);
    // signal_nl80211_sync(g); /* optional later */

    /* ---------- initial evaluation (AUTO + config) ---------- */

    graph_evaluate(g);
    socket_notify_subscribers(g, /* admin_up = */ true);

    printf("lnmgrd: configuration loaded, running (Ctrl+C to exit)\n");

    /* ---------- main event loop ---------- */
    while (running) {
        struct pollfd pfds[4];
        nfds_t nfds = 0;

        pfds[nfds++] = (struct pollfd){
            .fd     = sigpipe[0],
            .events = POLLIN | POLLERR | POLLHUP,
        };

        pfds[nfds++] = (struct pollfd){
            .fd     = nl_fd,
            .events = POLLIN | POLLERR | POLLHUP,
        };

        if (wifi_fd >= 0) {
            pfds[nfds++] = (struct pollfd){
                .fd     = wifi_fd,
                .events = POLLIN | POLLERR | POLLHUP,
            };
        }

        pfds[nfds++] = (struct pollfd){
            .fd     = ctl_fd,
            .events = POLLIN | POLLERR | POLLHUP,
        };

        int rc = poll(pfds, nfds, -1);
        if (rc < 0) {
            if (errno == EINTR)
                continue;
            perror("poll");
            break;
        }

        bool changed = false;
        bool nl_activity = false;
        nfds_t i = 0;

        /* ---------- signal pipe ---------- */
        if (pfds[i].revents & (POLLIN | POLLERR | POLLHUP)) {
            char buf[32];
            while (read(sigpipe[0], buf, sizeof(buf)) > 0) {
                /* drain */
            }
            break;
        }
        i++;

        /* ---------- rtnetlink ---------- */
        if (pfds[i].revents & POLLIN) {
            DPRINTF("poll nl_fd=%d\n", nl_fd);
            if (pfds[i].revents & (POLLIN | POLLERR | POLLHUP)) {
                nl_activity = signal_netlink_handle(g);
                changed |= nl_activity;
            }
        }

        if (pfds[i].revents & (POLLERR | POLLHUP)) {
            DPRINTF("netlink error → resync\n");
            signal_netlink_sync(g);
            changed = true;
        }
        i++;

        /* ---------- nl80211 ---------- */
        if (wifi_fd >= 0) {
            if (pfds[i].revents & POLLIN)
                changed |= signal_nl80211_handle(g);

            if (pfds[i].revents & (POLLERR | POLLHUP)) {
                signal_nl80211_sync(g);
                changed = true;
            }
            i++;
        }

        /* ---------- control socket ---------- */
        if (pfds[i].revents & POLLIN) {
            int cfd = accept(ctl_fd, NULL, NULL);

            if (cfd < 0) {
                if (errno == EINTR)
                    continue;   /* retry poll loop */

                perror("accept");
                continue;       /* do NOT exit daemon */
            }

            DPRINTF("cfd accept: %d\n", cfd);

            int r = socket_handle_client(cfd, g);

            if (r == 2)          /* graph mutated */
                changed = true;

            if (r <= 0)          /* close client */
                close(cfd);

            DPRINTF("CDF Socket done\n");
        }
 
        if (pfds[i].revents & (POLLERR | POLLHUP)) {
            perror("control socket error");
            break;
        }

        /* ---------- evaluate + notify ONCE ---------- */
        if (changed || nl_activity) {
            graph_evaluate(g);
            socket_notify_subscribers(g, true);
        }
    }    
    printf("lnmgrd: shutting down\n");

    socket_close(ctl_fd, LNMGR_SOCKET_PATH);
    signal_netlink_close();
    signal_nl80211_close();
    graph_destroy(g);

    return 0;
}