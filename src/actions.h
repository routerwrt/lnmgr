#pragma once

#include "node.h"

struct action_ops {
    action_result_t (*activate)(struct node *n);
    void (*deactivate)(struct node *n);
};

/* Action dispatch */
const struct action_ops *action_ops_for_kind(node_kind_t kind);