#ifndef LNMGR_CONFIG_H
#define LNMGR_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#include "graph.h"

struct l2_vlan_tmp {
    uint16_t vid;
    bool tagged;
    bool pvid;
    struct l2_vlan_tmp *next;
};

struct node_tmp {
    char *id;
    node_kind_t     kind;
    node_type_t     type;   /* derived from kind */
    int             have_kind;
    int             enabled;
    int             auto_up;

    char            **signals;
    int             signals_n;

    char            **requires;
    int             requires_n;

    /* NEW: topology */
    char            *master_id; /* "bridge": "br-lan" */

    /* NEW: VLAN intent */
    struct l2_vlan_tmp *vlans;

    struct node     *gn;
};

int config_load_file(struct graph *g, const char *path);

#endif