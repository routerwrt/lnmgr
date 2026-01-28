#ifndef LNMGR_SOCKET_H
#define LNMGR_SOCKET_H

#include "lnmgr_status.h"

struct graph;

struct subscriber {
    int fd;

    struct node_state {
        char *id;
        struct lnmgr_explain last;
        struct node_state *next;
    } *states;

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