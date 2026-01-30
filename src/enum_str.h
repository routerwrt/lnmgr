#pragma once

#include "graph.h"
#include "node.h"
#include "lnmgr_status.h"

const char *node_kind_to_str(node_kind_t kind);
node_kind_t node_kind_from_str(const char *name);

const char *node_state_to_str(node_state_t s);
const char *explain_type_to_str(explain_type_t e);

const char *lnmgr_status_to_str(lnmgr_status_t st);
const char *lnmgr_code_to_str(lnmgr_code_t code);