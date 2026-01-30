/* l2.h */
#pragma once

#include <stdint.h>
#include <stdbool.h>

struct l2_vlan {
    uint16_t vid;
    bool     tagged;
    bool     pvid;
    struct l2_vlan *next;
};
