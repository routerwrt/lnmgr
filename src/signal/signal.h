#ifndef LNMGR_SIGNAL_H
#define LNMGR_SIGNAL_H

#include "graph.h"

struct signal_producer {
    int  (*fd)(void);
    int  (*handle)(struct graph *g);
    void (*close)(void);
};

#endif