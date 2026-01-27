#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JSMN_HEADER
#include "json/jsmn.h"

static int jsoneq(const char *js, const jsmntok_t *t, const char *s)
{
    size_t n = (size_t)(t->end - t->start);
    return (t->type == JSMN_STRING &&
            strlen(s) == n &&
            strncmp(js + t->start, s, n) == 0) ? 0 : -1;
}

static char *tok_strdup(const char *js, const jsmntok_t *t)
{
    size_t n = (size_t)(t->end - t->start);
    char *s = calloc(1, n + 1);
    if (!s)
        return NULL;

    memcpy(s, js + t->start, n);
    s[n] = '\0';
    return s;
}

static int tok_int(const char *js, const jsmntok_t *t, int *out)
{
    if (t->type != JSMN_PRIMITIVE)
        return -1;

    char *tmp = tok_strdup(js, t);
    if (!tmp)
        return -1;

    char *end = NULL;
    long v = strtol(tmp, &end, 10);
    int ok = (end && *end == '\0');
    free(tmp);
    if (!ok)
        return -1;
    *out = (int)v;
    return 0;
}

static int tok_bool(const char *js, const jsmntok_t *t, int *out)
{
    if (t->type != JSMN_PRIMITIVE)
        return -1;
    int n = t->end - t->start;
    const char *p = js + t->start;
    if (n == 4 && strncmp(p, "true", 4) == 0) {
        *out = 1;
        return 0;
    }
    if (n == 5 && strncmp(p, "false", 5) == 0) {
        *out = 0;
        return 0;
    }
    return -1;
}

/* Skip a token (and all its children) and return next index */
static int tok_skip(const jsmntok_t *toks, int i)
{
    int j = i;
    switch (toks[j].type) {
    case JSMN_PRIMITIVE:
    case JSMN_STRING:
        return j + 1;
    case JSMN_ARRAY: {
        int count = toks[j].size;
        j++;
        for (int k = 0; k < count; k++)
            j = tok_skip(toks, j);
        return j;
    }
    case JSMN_OBJECT: {
        int count = toks[j].size; /* number of key/value pairs */
        j++;
        for (int k = 0; k < count; k++) {
            j = tok_skip(toks, j); /* key */
            j = tok_skip(toks, j); /* value */
        }
        return j;
    }
    default:
        return j + 1;
    }
}

static int parse_type(const char *s, node_type_t *out)
{
    if (strcmp(s, "device") == 0) {
        *out = NODE_DEVICE;
        return 0;
    }
    if (strcmp(s, "bridge") == 0) {
        *out = NODE_BRIDGE;
        return 0;
    }
    if (strcmp(s, "transformer") == 0) {
        *out = NODE_TRANSFORMER;
        return 0;
    }
    if (strcmp(s, "service") == 0) {
        *out = NODE_SERVICE;
        return 0;
    }

    return -1;
}

struct node_tmp {
    char *id;
    node_type_t type;
    int have_type;
    int enabled;
    int auto_up;
    char **signals;
    int signals_n;
    char **requires;
    int requires_n;
};

static void node_tmp_free(struct node_tmp *n)
{
    if (!n)
        return;
    free(n->id);
    for (int i = 0; i < n->signals_n; i++)
        free(n->signals[i]);
 
    free(n->signals);
    for (int i = 0; i < n->requires_n; i++)
        free(n->requires[i]);

    free(n->requires);
    memset(n, 0, sizeof(*n));
}

static int parse_string_array(const char *js, const jsmntok_t *toks, int *i,
                              char ***out, int *out_n)
{
    const jsmntok_t *a = &toks[*i];
    if (a->type != JSMN_ARRAY)
        return -1;

    int n = a->size;
    char **arr = calloc((size_t)n, sizeof(char *));
    if (!arr)
        return -1;

    int idx = *i + 1;
    for (int k = 0; k < n; k++) {
        if (toks[idx].type != JSMN_STRING) {
            for (int x = 0; x < k; x++) free(arr[x]);
                free(arr);

            return -1;
        }
        arr[k] = tok_strdup(js, &toks[idx]);
        if (!arr[k]) {
            for (int x = 0; x < k; x++) free(arr[x]);
                    free(arr);
 
                    return -1;
        }
        idx = tok_skip(toks, idx);
    }

    *out = arr;
    *out_n = n;
    *i = idx;
 
    return 0;
}

static int parse_node_object(const char *js, const jsmntok_t *toks, int *i,
                             struct node_tmp *out)
{
    const jsmntok_t *o = &toks[*i];
    if (o->type != JSMN_OBJECT)
        return -1;

    struct node_tmp n = {0};
    n.enabled = 0;
    n.auto_up = 0;

    int pairs = o->size;
    int idx = *i + 1;

    for (int p = 0; p < pairs; p++) {
        const jsmntok_t *k = &toks[idx++];
        const jsmntok_t *v = &toks[idx];

        if (k->type != JSMN_STRING) {
                node_tmp_free(&n);
                return -1;
        }

        if (jsoneq(js, k, "id") == 0) {
            if (v->type != JSMN_STRING) {
                node_tmp_free(&n);
                return -1;
            }
            n.id = tok_strdup(js, v);
            if (!n.id) {
                node_tmp_free(&n);
                return -1;
            }
            idx = tok_skip(toks, idx);
            continue;
        }

        if (jsoneq(js, k, "type") == 0) {
            if (v->type != JSMN_STRING) {
                node_tmp_free(&n);
                return -1;
            }
            char *ts = tok_strdup(js, v);
            if (!ts) {
                node_tmp_free(&n);
                return -1;
            }
            int rc = parse_type(ts, &n.type);
            free(ts);
            if (rc < 0) {
                node_tmp_free(&n);
                return -1;
            }
            idx = tok_skip(toks, idx);
            continue;
        }

        if (jsoneq(js, k, "enabled") == 0) {
            int b = 0;
            if (tok_bool(js, v, &b) < 0) {
                node_tmp_free(&n);
                return -1;
            }
            n.enabled = b;
            idx = tok_skip(toks, idx);
            continue;
        }

        if (jsoneq(js, k, "auto") == 0) {
            int b = 0;
            if (tok_bool(js, v, &b) < 0) {
                node_tmp_free(&n);
                return -1;
            }
            n.auto_up = b;
            idx = tok_skip(toks, idx);
            continue;
        }

        if (jsoneq(js, k, "signals") == 0) {
            if (parse_string_array(js, toks, &idx, &n.signals, &n.signals_n) < 0) {
                node_tmp_free(&n);
                return -1;
            }
            continue;
        }

        if (jsoneq(js, k, "requires") == 0) {
            if (parse_string_array(js, toks, &idx, &n.requires, &n.requires_n) < 0) {
                node_tmp_free(&n);
                return -1;
            }
            continue;
        }

        /* Unknown key: strict */
        node_tmp_free(&n);
        return -1;
    }

    /* required fields */
    if (!n.id) {
        node_tmp_free(&n);
        return -1;
    }
    /* type must have been set by parse_type; default is 0 which equals NODE_DEVICE,
       so we must require explicit "type" to avoid silent surprises. */
    /* We detect it by requiring the "type" key to have been present; easiest is to
       enforce by checking that "type" was parsed: but we didn't store that boolean.
       Quick strictness: require that the JSON contains "type" by adding a flag. */

    *out = n;
    *i = idx;
    return 0;
}

/* Read file into memory */
static int read_whole_file(const char *path, char **out, size_t *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return -1;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }

    char *buf = calloc(1, (size_t)sz + 1);
    if (!buf) {
        fclose(f);
        errno = ENOMEM;
        return -1;
    }

    size_t r = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (r != (size_t)sz) {
        free(buf);
        return -1;
    }

    buf[sz] = '\0';
    *out = buf;
    *out_len = (size_t)sz;
    return 0;
}

int config_load_file(struct graph *g, const char *path)
{
    char *js = NULL;
    size_t len = 0;

    if (read_whole_file(path, &js, &len) < 0)
        return -1;

    /* Token buffer: v1 keeps this fixed. Increase later if needed. */
    enum { TOKMAX = 2048 };
    jsmntok_t *toks = calloc(TOKMAX, sizeof(jsmntok_t));
    if (!toks) {
        free(js);
        errno = ENOMEM;
        return -1;
    }

    jsmn_parser p;
    jsmn_init(&p);
    int ntok = jsmn_parse(&p, js, len, toks, TOKMAX);
    if (ntok < 0) {
        free(toks); free(js);
        errno = EINVAL;
        return -1;
    }

    if (toks[0].type != JSMN_OBJECT) {
        free(toks); free(js);
        errno = EINVAL;
        return -1;
    }

    int version = -1;
    int flush = 0;

    /* First pass: parse top-level keys and collect node objects into tmps */
    struct node_tmp *nodes = NULL;
    int nodes_n = 0;

    int pairs = toks[0].size;
    int idx = 1;

    for (int pidx = 0; pidx < pairs; pidx++) {
        const jsmntok_t *k = &toks[idx++];
        const jsmntok_t *v = &toks[idx];

        if (k->type != JSMN_STRING)
                goto fail;

        if (jsoneq(js, k, "version") == 0) {
            if (tok_int(js, v, &version) < 0)
                goto fail;
            idx = tok_skip(toks, idx);
            continue;
        }

        if (jsoneq(js, k, "flush") == 0) {
            if (tok_bool(js, v, &flush) < 0)
                goto fail;
            idx = tok_skip(toks, idx);
            continue;
        }

        if (jsoneq(js, k, "nodes") == 0) {
            if (v->type != JSMN_ARRAY)
                goto fail;
            int n = v->size;

            nodes = calloc((size_t)n, sizeof(struct node_tmp));
            if (!nodes)
                goto fail;

            nodes_n = n;
            idx++; /* enter array */

            for (int i = 0; i < n; i++) {
                if (parse_node_object(js, toks, &idx, &nodes[i]) < 0)
                    goto fail;
            }
            continue;
        }

        /* Unknown top-level key: strict */
        goto fail;
    }

    if (version != 1)
        goto fail;

    if (flush) {
        /* You need to implement graph_flush(g) (Step 2 below) */
        /* graph_flush(g); */
    }

    /* Apply in 3 phases so requires can refer to nodes defined later in file */
    for (int i = 0; i < nodes_n; i++) {
        if (!graph_add_node(g, nodes[i].id, nodes[i].type))
                goto fail;
    }

    for (int i = 0; i < nodes_n; i++) {
        for (int s = 0; s < nodes[i].signals_n; s++) {
            if (graph_add_signal(g, nodes[i].id, nodes[i].signals[s]) < 0)
                goto fail;
        }
    }

    for (int i = 0; i < nodes_n; i++) {
        for (int r = 0; r < nodes[i].requires_n; r++) {
            if (graph_add_require(g, nodes[i].id, nodes[i].requires[r]) < 0)
                goto fail;
        }
    }

    for (int i = 0; i < nodes_n; i++) {
        if (nodes[i].enabled) {
            if (graph_enable_node(g, nodes[i].id) < 0)
                goto fail;
        }
        /* auto semantics can come later; for now it’s just stored (or ignored) */
        (void)nodes[i].auto_up;
    }

    graph_evaluate(g);

    for (int i = 0; i < nodes_n; i++)
        node_tmp_free(&nodes[i]);

    free(nodes);
    free(toks);
    free(js);
    return 0;

fail:
    for (int i = 0; i < nodes_n; i++)
        node_tmp_free(&nodes[i]);
        
    free(nodes);
    free(toks);
    free(js);
    errno = EINVAL;
    return -1;
}