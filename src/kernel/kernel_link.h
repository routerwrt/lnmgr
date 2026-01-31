// src/kernel/kernel_link.h
#pragma once

#include <stdbool.h>

int kernel_link_set_updown(const char *ifname, bool up);

static inline int kernel_link_set_up(const char *ifname)
{
    return kernel_link_set_updown(ifname, true);
}

static inline int kernel_link_set_down(const char *ifname)
{
    return kernel_link_set_updown(ifname, false);
}

bool kernel_link_is_up(const char *ifname);
bool kernel_link_exists(const char *ifname);

int kernel_link_get_ifindex(const char *ifname);