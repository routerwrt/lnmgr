#ifndef LNMGR_SOCKET_H
#define LNMGR_SOCKET_H

struct graph;

struct subscriber {
    int fd;
    struct lnmgr_explain *last; /* array indexed by node */
    struct subscriber *next;
};

/* create, bind, listen */
int socket_listen(const char *path);

/* accept + handle exactly one request */
int socket_handle_client(int fd, struct graph *g);

/* cleanup */
void socket_close(int fd, const char *path);

void socket_add_subscriber(int fd);

#endif