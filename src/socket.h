#ifndef LNMGR_SOCKET_H
#define LNMGR_SOCKET_H

#include "lnmgr_status.h"

struct graph;

struct signal_state {
    char *name;
    bool value;

    struct signal_state *next;
};

struct node_state {
    char *id;                      /* node id */
    struct lnmgr_explain last;     /* last state/code seen */

    struct signal_state *signals;  /* NEW: per-subscriber signal snapshot */

    struct node_state *next;
};

struct subscriber {
    int fd;

    struct node_state *states;

    struct subscriber *next;
};

/* create, bind, listen */
int socket_listen(const char *path);

/*
 * socket_handle_client()
 *
 * Returns:
 *   0  → request handled, caller must close fd
 *   1  → fd is a subscriber, caller must keep it open
 *  -1  → protocol error, caller should close fd
 */
int socket_handle_client(int fd, struct graph *g);

/* cleanup */
void socket_close(int fd, const char *path);

void socket_add_subscriber(struct graph *g, int fd);

void socket_notify_subscribers(struct graph *g, bool admin_up);

#endif