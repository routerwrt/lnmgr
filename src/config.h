#ifndef LNMGR_CONFIG_H
#define LNMGR_CONFIG_H

#include <stddef.h>
#include "graph.h"

int config_load_file(struct graph *g, const char *path);

#endif