#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "socket.h"
#include "enum_str.h"
#include "actions.h"
#include "graph.h"

static struct subscriber *subscribers = NULL;

static bool write_all(int fd, const char *buf, size_t len)
{
    while (len > 0) {
        ssize_t n = write(fd, buf, len);

        if (n > 0) {
            buf += n;
            len -= n;
            continue;
        }

        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return false;   /* slow subscriber */

        return false;       /* fatal */
    }

    return true;
}

static bool fd_printf_nb(int fd, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int rc = vdprintf(fd, fmt, ap);
    va_end(ap);

    if (rc < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK ||
            errno == EPIPE  || errno == ECONNRESET)
            return false;
    }

    return true;
}

static bool json_emit_signals_nb(int fd, struct node *n)
{
    if (!n->signals)
        return true;

    if (!write_all(fd, ", \"signals\": {", 14))
        return false;

    bool first = true;

    for (struct signal *s = n->signals; s; s = s->next) {
        if (!first) {
            if (!write_all(fd, ", ", 2))
                return false;
        }
        first = false;

        char buf[256];
        int len = snprintf(buf, sizeof(buf),
            "\"%s\": %s",
            s->name,
            s->value ? "true" : "false");

        if (len < 0 || len >= (int)sizeof(buf))
            return false;

        if (!write_all(fd, buf, len))
            return false;
    }

    if (!write_all(fd, "}", 1))
        return false;

    return true;
}

static struct node_state *
subscriber_get_node(struct subscriber *s, const char *id)
{
    for (struct node_state *ns = s->states; ns; ns = ns->next) {
        if (strcmp(ns->id, id) == 0)
            return ns;
    }

    struct node_state *ns = calloc(1, sizeof(*ns));
    if (!ns)
        return NULL;

    ns->id = strdup(id);
    if (!ns->id) {
        free(ns);
        return NULL;
    }

    ns->last.status = LNMGR_STATUS_UNKNOWN;
    ns->last.code   = LNMGR_CODE_NONE;

    ns->next = s->states;
    s->states = ns;

    return ns;
}

static struct signal_state *
signal_state_get(struct signal_state **list, const char *name)
{
    for (struct signal_state *ss = *list; ss; ss = ss->next) {
        if (strcmp(ss->name, name) == 0)
            return ss;
    }

    struct signal_state *ss = calloc(1, sizeof(*ss));
    if (!ss)
        return NULL;

    ss->name = strdup(name);
    if (!ss->name) {
        free(ss);
        return NULL;
    }

    ss->value = false;

    ss->next = *list;
    *list = ss;

    return ss;
}

static bool
signals_changed(struct node_state *ns, struct node *n)
{
    bool changed = false;

    for (struct signal *s = n->signals; s; s = s->next) {
        struct signal_state *ss =
            signal_state_get(&ns->signals, s->name);
        if (!ss)
            continue;

        if (ss->value != s->value) {
            ss->value = s->value;
            changed = true;
        }
    }

    return changed;
}

static bool socket_send_event(int fd,
                              struct graph *g,
                              const char *id,
                              const struct lnmgr_explain *ex)
{
    const char *state = lnmgr_status_to_str(ex->status);
    const char *code  = lnmgr_code_to_str(ex->code);

     if (!fd_printf_nb(fd,
        "{ \"type\": \"event\", \"id\": \"%s\", \"state\": \"%s\"",
        id, state))
        return false;

    if (code) {
        if (!fd_printf_nb(fd, ", \"code\": \"%s\"", code))
            return false;
    }
    
    struct node *n = graph_find_node(g, id);
    if (n) {
        if (!json_emit_signals_nb(fd, n))
            return false;
    } else {
        if (!fd_printf_nb(fd, ", \"signals\": {}"))
            return false;
    }

    if (!fd_printf_nb(fd, " }\n"))
        return false;

    return true;
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

static void notify_subscribers(struct graph *g, bool admin_up)
{
    struct subscriber *s = subscribers;
    struct subscriber *prev = NULL;

    while (s) {
        bool alive = true;

        for (struct node *n = g->nodes; n; n = n->next) {
            struct lnmgr_explain now =
                lnmgr_status_for_node(g, n, admin_up);

            struct node_state *ns = subscriber_get_node(s, n->id);
            if (!ns)
                continue;

            bool changed = false;

            if (ns->last.status != now.status ||
                ns->last.code   != now.code) {
                ns->last = now;
                changed = true;
            }

            changed |= signals_changed(ns, n);

            if (!changed)
                continue;

            if (!socket_send_event(s->fd, g, n->id, &now)) {
                alive = false;
                break;   /* stop sending to this subscriber */
            }
        }

        if (!alive) {
            struct subscriber *dead = s;
            s = s->next;
            drop_subscriber(prev, dead);
            continue;
        }

        prev = s;
        s = s->next;
    }
}

void socket_notify_subscribers(struct graph *g, bool admin_up)
{
    notify_subscribers(g, admin_up);
}

static bool send_snapshot(int fd, struct subscriber *s, struct graph *g)
{
    char buf[1024];
    int len;

    /* opening */
    len = snprintf(buf, sizeof(buf),
                   "{ \"type\": \"snapshot\", \"nodes\": [");
    if (len < 0 || len >= (int)sizeof(buf))
        return false;
    if (!write_all(fd, buf, len))
        return false;

    bool first = true;

    for (struct node_state *ns = s->states; ns; ns = ns->next) {
        if (!first) {
            if (!write_all(fd, ",", 1))
                return false;
        }
        first = false;

        struct node *n = graph_find_node(g, ns->id);

        len = snprintf(buf, sizeof(buf),
            "{ \"id\": \"%s\", \"state\": \"%s\"",
            ns->id,
            lnmgr_status_to_str(ns->last.status));

        if (len < 0 || len >= (int)sizeof(buf))
            return false;
        if (!write_all(fd, buf, len))
            return false;

        /* node type (human-visible kind) */
        if (n) {
            const struct node_kind_desc *kd = node_kind_lookup(n->kind);
            if (kd) {
                len = snprintf(buf, sizeof(buf),
                               ", \"type\": \"%s\"",
                               kd->name);
                if (len < 0 || len >= (int)sizeof(buf))
                    return false;
                if (!write_all(fd, buf, len))
                    return false;
            }
        }

        /* optional code */
        const char *code = lnmgr_code_to_str(ns->last.code);
        if (code) {
            len = snprintf(buf, sizeof(buf),
                           ", \"code\": \"%s\"",
                           code);
            if (len < 0 || len >= (int)sizeof(buf))
                return false;
            if (!write_all(fd, buf, len))
                return false;
        }

        /* signals */
        if (n) {
            if (!json_emit_signals_nb(fd, n))
                return false;
        }

        if (!write_all(fd, " }", 2))
            return false;
    }

    /* closing */
    if (!write_all(fd, "] }\n", 4))
        return false;

    return true;
}

static void subscriber_init_states(struct subscriber *s, struct graph *g)
{
    struct node_state *tail = NULL;

    for (struct node *n = g->nodes; n; n = n->next) {
        struct node_state *ns = calloc(1, sizeof(*ns));
        if (!ns)
            continue;

        ns->id = strdup(n->id);
        ns->last = lnmgr_status_for_node(g, n, true /* admin_up placeholder */);

        if (!tail)
            s->states = ns;
        else
            tail->next = ns;

        tail = ns;
    }
}

/*
 * Subscribers are best-effort observers.
 * They may be disconnected at any time.
 * Reconnection + snapshot is the only recovery mechanism.
 */
static void add_subscriber(int fd, struct graph *g)
{
    struct subscriber *s = calloc(1, sizeof(*s));
    if (!s)
        return;

    /* make subscriber socket non-blocking */
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0)
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    s->fd = fd;
    subscriber_init_states(s, g);

    /* IMPORTANT: snapshot must also tolerate EAGAIN */
    if (!send_snapshot(fd, s, g)) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            /* snapshot incomplete – subscriber still valid */
            s->next = subscribers;
            subscribers = s;
            return;
        }

        /* real error */
        close(fd);
        free(s);
        return;
    }

    s->next = subscribers;
    subscribers = s;
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

static bool reply_status_one(int fd, struct graph *g, const char *id)
{
    struct explain e = graph_explain_node(g, id);

    if (!fd_printf_nb(fd, "{ \"type\": \"status\", \"id\": \"%s\", "
        "\"state\": %d, \"explain\": %d }\n",
        id,
        e.type == EXPLAIN_NONE ? NODE_ACTIVE : NODE_WAITING,
        e.type))
        return false;

    return true;
}

static bool reply_status_all(int fd, struct graph *g)
{
    if (!fd_printf_nb(fd, "{ \"type\": \"status\", \"nodes\": ["))
        return false;
 
    struct node *n = g->nodes;
    bool first = true;

    while (n) {
        if (!first) {
           if (!fd_printf_nb(fd, ","))
                return false;
        } 
        first = false;

        struct lnmgr_explain lex =
            lnmgr_status_for_node(g, n, true /* admin_up placeholder */);

        const char *code = lnmgr_code_to_str(lex.code);

        if (!fd_printf_nb(fd,
            "{ \"id\": \"%s\", \"state\": \"%s\"%s%s }",
            n->id,
            lnmgr_status_to_str(lex.status),
            code ? ", \"code\": \"" : "",
            code ? code : ""))
            return false;
 
        n = n->next;
    }

    if (!fd_printf_nb(fd, "] }\n"))
        return false;

    return true;
}

static bool reply_dump(int fd, struct graph *g)
{
    if (!fd_printf_nb(fd, "{ \"type\": \"dump\", \"nodes\": ["))
        return false;

    struct node *n = g->nodes;
    bool first = true;

    while (n) {
        if (!first)
            if (!fd_printf_nb(fd, ","))
                return false;

        first = false;

        const struct node_kind_desc *kd = node_kind_lookup(n->kind);

        if (!fd_printf_nb(fd,
            "{ \"id\": \"%s\", "
            "\"type\": \"%s\", "
            "\"enabled\": %s, "
            "\"auto\": %s",
            n->id,
            kd ? kd->name : "unknown",
            n->enabled ? "true" : "false",
            n->auto_up ? "true" : "false"))
            return false;

        /* ---- requires[] ---- */
        if (!fd_printf_nb(fd, ", \"requires\": ["))
            return false;

        bool rfirst = true;
        for (struct require *r = n->requires; r; r = r->next) {
            if (!rfirst)
                if (!fd_printf_nb(fd, ","))
                    return false;
        
            rfirst = false;

            if (!fd_printf_nb(fd, "\"%s\"", r->node->id))
                return false;
        }
        if (!fd_printf_nb(fd, "]"))
            return false;

        /* ---- actions (presence only) ---- */
        if (!fd_printf_nb(fd,
            ", \"actions\": { "
            "\"activate\": %s, "
            "\"deactivate\": %s }",
            (n->actions && n->actions->activate)   ? "true" : "false",
            (n->actions && n->actions->deactivate) ? "true" : "false"))
            return false;

        if (!fd_printf_nb(fd, " }"))
            return false;

        n = n->next;
    }

    if (!fd_printf_nb(fd, "] }\n"))
        return false;
    
    return true;
}

static bool reply_save(int fd, struct graph *g)
{
    graph_save_json(g, fd);

    return true;
}

static bool handle_signal_cmd(int fd, struct graph *g, char *args)
{
    char node[64], sig[64];
    int val;

    if (sscanf(args, "%63s %63s %d", node, sig, &val) != 3) {
        if (!fd_printf_nb(fd, "{ \"error\": \"invalid syntax\" }\n"))
            return false;

        return true;
    }

    if (val != 0 && val != 1) {
        if (!fd_printf_nb(fd, "{ \"error\": \"invalid value\" }\n"))
            return false;

        return true;
    }

    if (!graph_find_node(g, node)) {
        if (!fd_printf_nb(fd, "{ \"error\": \"unknown node\" }\n"))
            return false;

        return true;
    }

    bool changed = graph_set_signal(g, node, sig, val);

    if (changed) {
        graph_evaluate(g);
        socket_notify_subscribers(g, /* admin_up = */ true);
    }

    if (!fd_printf_nb(fd,
        "{ \"type\": \"signal\", "
        "\"node\": \"%s\", "
        "\"signal\": \"%s\", "
        "\"value\": %s, "
        "\"changed\": %s }\n",
        node,
        sig,
        val ? "true" : "false",
        changed ? "true" : "false"))
        return false;
    
    return true;
}

int socket_handle_client(int fd, struct graph *g)
{
    char line[256];

    DPRINTF("socket_handle_client(fd=%d)\n", fd);

    for (;;) {
        ssize_t rc = read_line(fd, line, sizeof(line));
        if (rc <= 0)
            return SOCKET_CLOSE;

        if (strcmp(line, "HELLO") == 0) {
            if (!fd_printf_nb(fd,
                "{ \"type\": \"hello\", \"version\": 1, "
                "\"features\": [\"status\",\"dump\",\"save\",\"subscribe\"] }\n"))
                return SOCKET_ERROR;
            continue;
        }

        if (strcmp(line, "SUBSCRIBE") == 0) {
            DPRINTF("SUBSCRIBE accepted fd=%d\n", fd);
            socket_add_subscriber(g, fd);
            return SOCKET_KEEP;   /* DO NOT CLOSE */
        }

        if (strcmp(line, "STATUS") == 0) {
            if (!reply_status_all(fd, g))
                return SOCKET_ERROR;
            continue;
        }

        if (strncmp(line, "STATUS ", 7) == 0) {
            if (!reply_status_one(fd, g, line + 7))
                return SOCKET_ERROR;
            continue;
        }

        if (strcmp(line, "DUMP") == 0) {
            if (!reply_dump(fd, g))
                return SOCKET_ERROR;
            continue;
        }

        if (strcmp(line, "SAVE") == 0) {
            reply_save(fd, g);
            return SOCKET_MUTATE;
        }

        if (strncmp(line, "SIGNAL ", 7) == 0) {
            if (!handle_signal_cmd(fd, g, line + 7))
                return SOCKET_ERROR;
            return SOCKET_MUTATE;
        }

        if (!fd_printf_nb(fd, "{ \"error\": \"unknown command\" }\n"))
            return SOCKET_ERROR;
    }
}

void socket_close(int fd, const char *path)
{
    if (fd >= 0)
        close(fd);
    if (path)
        unlink(path);
}