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
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <poll.h>


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

static void on_sigint(int sig)
{
    (void)sig;
    running = 0;
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


int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <config.json>\n", argv[0]);
        return 1;
    }

    setup_signals();

    signal(SIGINT, on_sigint);
    signal(SIGTERM, on_sigint);

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

    printf("lnmgrd: configuration loaded, running (Ctrl+C to exit)\n");

    int ctl_fd  = socket_listen(LNMGR_SOCKET_PATH);
    int nl_fd   = signal_netlink_fd();
    int wifi_fd = signal_nl80211_fd();

    if (ctl_fd < 0 || nl_fd < 0) {
        perror("initialization failed");
        graph_destroy(g);
        return 1;
    }

    printf("lnmgrd: initial link state synchronized\n");

    while (running) {
        struct pollfd pfds[3];
        nfds_t nfds = 0;

        pfds[nfds++] = (struct pollfd){ .fd = nl_fd, .events = POLLIN };

        if (wifi_fd >= 0)
            pfds[nfds++] = (struct pollfd){ .fd = wifi_fd, .events = POLLIN };

        pfds[nfds++] = (struct pollfd){ .fd = ctl_fd, .events = POLLIN };

        int rc = poll(pfds, nfds, -1);
        if (rc < 0) {
            if (errno == EINTR)
                continue;
            perror("poll");
            break;
        }

        bool changed = false;
        nfds_t i = 0;

        /* --- netlink carrier --- */
        if (pfds[i++].revents & POLLIN) {
            signal_netlink_handle(g);
            changed = true;
        }

        /* --- nl80211 (wifi) --- */
        if (wifi_fd >= 0) {
            if (pfds[i++].revents & POLLIN) {
                signal_nl80211_handle(g);
                changed = true;
            }
        }

        /* --- notify subscribers ONCE --- */
        if (changed) {
            socket_notify_subscribers(g, /* admin_up = */ true);
            /* ^ if we later track admin intent per node,
                 compute it here, not in signal code */
        }

        /* --- control socket --- */
        if (pfds[i].revents & POLLIN) {
            int cfd;
            while ((cfd = accept(ctl_fd, NULL, NULL)) >= 0) {
                int r = socket_handle_client(cfd, g);
                if (r == 0)
                    close(cfd);
            }
            if (errno != EAGAIN && errno != EINTR)
                perror("accept");
        }
    }

    printf("lnmgrd: shutting down\n");

    socket_close(ctl_fd, LNMGR_SOCKET_PATH);
    signal_netlink_close();
    signal_nl80211_close();
    graph_destroy(g);

    return 0;
}