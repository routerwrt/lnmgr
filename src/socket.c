#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>

#include "socket.h"
#include "graph.h"

static struct subscriber *subscribers = NULL;

static struct lnmgr_explain *
subscriber_last(struct subscriber *s, struct node *n)
{
    struct node_state *st;

    for (st = s->states; st; st = st->next) {
        if (strcmp(st->id, n->id) == 0)
            return &st->last;
    }

    st = calloc(1, sizeof(*st));
    st->id = strdup(n->id);
    st->last.status = LNMGR_STATUS_UNKNOWN;
    st->next = s->states;
    s->states = st;

    return &st->last;
}

static void socket_send_event(int fd,
                              const char *id,
                              const struct lnmgr_explain *ex)
{
    const char *state = lnmgr_status_to_str(ex->status);
    const char *code  = lnmgr_code_to_str(ex->code);

    dprintf(fd,
        "{ \"type\": \"event\", \"id\": \"%s\", \"state\": \"%s\"%s%s }\n",
        id,
        state,
        code ? ", \"code\": \"" : "",
        code ? code : ""
    );
}

static void notify_subscribers(struct graph *g, bool admin_up)
{
    for (struct subscriber *s = subscribers; s; s = s->next) {

        for (struct node *n = g->nodes; n; n = n->next) {

            struct lnmgr_explain now =
                    lnmgr_status_for_node(g, n, admin_up);

            struct lnmgr_explain *prev =
                subscriber_last(s, n);

            if (lnmgr_explain_equal(prev, &now))
                continue;

            socket_send_event(s->fd, n->id, &now);
            *prev = now;
        }
    }
}

void socket_notify_subscribers(struct graph *g, bool admin_up)
{
    notify_subscribers(g, admin_up);
}

static void send_snapshot(int fd, struct subscriber *s)
{
    dprintf(fd, "{ \"type\": \"snapshot\", \"nodes\": [");

    bool first = true;
    for (struct node_state *ns = s->states; ns; ns = ns->next) {
        if (!first)
            dprintf(fd, ",");
        first = false;

        dprintf(fd,
            "{ \"id\": \"%s\", \"state\": \"%s\"",
            ns->id,
            lnmgr_status_to_str(ns->last.status));

        const char *code = lnmgr_code_to_str(ns->last.code);
        if (code)
            dprintf(fd, ", \"code\": \"%s\"", code);

        dprintf(fd, " }");
    }

    dprintf(fd, "] }\n");
}

static void subscriber_init_states(struct subscriber *s, struct graph *g)
{
    struct node_state *tail = NULL;

    for (struct node *n = g->nodes; n; n = n->next) {
        struct node_state *ns = calloc(1, sizeof(*ns));
        if (!ns)
            continue;

        ns->id = strdup(n->id);

        struct explain gex = graph_explain_node(g, n->id);
        ns->last = lnmgr_status_from_graph(&gex, true /* admin_up placeholder */);

        if (!tail)
            s->states = ns;
        else
            tail->next = ns;

        tail = ns;
    }
}

static void add_subscriber(int fd, struct graph *g)
{
    struct subscriber *s = calloc(1, sizeof(*s));
    if (!s) {
        /* DO NOT close(fd) */
        return;
    }

    s->fd = fd;
    subscriber_init_states(s, g);
    send_snapshot(fd, s);

    s->next = subscribers;
    subscribers = s;
}

static void drop_subscriber(struct subscriber *prev, struct subscriber *s)
{
    if (prev)
        prev->next = s->next;
    else
        subscribers = s->next;

    close(s->fd);
    free(s);
}

void socket_add_subscriber(struct graph *g, int fd)
{
    add_subscriber(fd, g);
}

int socket_listen(const char *path)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    unlink(path);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        goto fail;

    chmod(path, 0666);

    if (listen(fd, 5) < 0)
        goto fail;

    return fd;

fail:
    close(fd);
    return -1;
}

static ssize_t read_line(int fd, char *buf, size_t max)
{
    size_t i = 0;
    while (i + 1 < max) {
        char c;
        ssize_t r = read(fd, &c, 1);
        if (r <= 0)
            break;
        if (c == '\n')
            break;
        buf[i++] = c;
    }
    buf[i] = '\0';
    return i;
}

static void reply_status_one(int fd, struct graph *g, const char *id)
{
    struct explain e = graph_explain_node(g, id);

    dprintf(fd,
        "{ \"type\": \"status\", \"id\": \"%s\", "
        "\"state\": %d, \"explain\": %d }\n",
        id, e.type == EXPLAIN_NONE ? NODE_ACTIVE : NODE_WAITING, e.type);
}

static void reply_status_all(int fd, struct graph *g)
{
    dprintf(fd, "{ \"type\": \"status\", \"nodes\": [");

    struct node *n = g->nodes;
    bool first = true;

    while (n) {
        if (!first)
            dprintf(fd, ",");
        first = false;

        struct explain gex = graph_explain_node(g, n->id);
        struct lnmgr_explain lex = lnmgr_status_from_graph(&gex, true);

        const char *code = lnmgr_code_to_str(lex.code);

        dprintf(fd, "{ \"id\": \"%s\", \"state\": \"%s\"%s%s }",
            n->id,
            lnmgr_status_to_str(lex.status),
            code ? ", \"code\": \"" : "",
            code ? code : "");

        n = n->next;
    }

    dprintf(fd, "] }\n");
}

static void reply_dump(int fd, struct graph *g)
{
    dprintf(fd, "{ \"type\": \"dump\", \"nodes\": [");

    struct node *n = g->nodes;
    bool first = true;

    while (n) {
        if (!first)
            dprintf(fd, ",");
        first = false;

        dprintf(fd,
            "{ \"id\": \"%s\", \"type\": \"%s\", \"enabled\": %s, \"auto\": %s }",
                n->id,
                node_type_to_str(n->type),
                n->enabled ? "true" : "false",
                n->auto_up ? "true" : "false");

        n = n->next;
    }

    dprintf(fd, "] }\n");
}

static void reply_save(int fd, struct graph *g)
{
    graph_save_json(g, fd);
}

static void handle_signal_cmd(int fd, struct graph *g, char *args)
{
    char node[64], sig[64];
    int val;

    if (sscanf(args, "%63s %63s %d", node, sig, &val) != 3) {
        dprintf(fd, "{ \"error\": \"invalid syntax\" }\n");
        return;
    }

    if (val != 0 && val != 1) {
        dprintf(fd, "{ \"error\": \"invalid value\" }\n");
        return;
    }

    if (!graph_find_node(g, node)) {
        dprintf(fd, "{ \"error\": \"unknown node\" }\n");
        return;
    }

    bool changed = graph_set_signal(g, node, sig, val);

    if (changed) {
        graph_evaluate(g);
        socket_notify_subscribers(g, /* admin_up = */ true);
    }

    dprintf(fd,
        "{ \"type\": \"signal\", "
        "\"node\": \"%s\", "
        "\"signal\": \"%s\", "
        "\"value\": %s, "
        "\"changed\": %s }\n",
        node,
        sig,
        val ? "true" : "false",
        changed ? "true" : "false");
}

int socket_handle_client(int fd, struct graph *g)
{
    char line[256];

    for (;;) {
        if (read_line(fd, line, sizeof(line)) <= 0)
            return 0;   /* client closed */

        if (strcmp(line, "HELLO") == 0) {
            dprintf(fd,
                "{ \"type\": \"hello\", \"version\": 1, "
                "\"features\": [\"status\",\"dump\",\"save\",\"subscribe\"] }\n");
            continue;
        }

        if (strcmp(line, "SUBSCRIBE") == 0) {
            add_subscriber(fd, g);
            return 1;   /* daemon must NOT close fd */
        }

        if (strcmp(line, "STATUS") == 0) {
            reply_status_all(fd, g);
            continue;
        }

        if (strncmp(line, "STATUS ", 7) == 0) {
            reply_status_one(fd, g, line + 7);
            continue;
        }

        if (strcmp(line, "DUMP") == 0) {
            reply_dump(fd, g);
            continue;
        }

        if (strcmp(line, "SAVE") == 0) {
            reply_save(fd, g);
            continue;
        }

        if (strncmp(line, "SIGNAL ", 7) == 0) {
            handle_signal_cmd(fd, g, line + 7);
            continue;
        }

        dprintf(fd, "{ \"error\": \"unknown command\" }\n");
    }
}

void socket_close(int fd, const char *path)
{
    if (fd >= 0)
        close(fd);
    if (path)
        unlink(path);
}