#ifndef LNMGR_SOCKET_H
#define LNMGR_SOCKET_H

struct graph;

/* create, bind, listen */
int socket_listen(const char *path);

/* accept + handle exactly one request */
int socket_handle_client(int fd, struct graph *g);

/* cleanup */
void socket_close(int fd, const char *path);

#endif