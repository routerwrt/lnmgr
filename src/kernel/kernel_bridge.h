// src/kernel/kernel_bridge.h
#pragma once

#include <stdbool.h>

/* lifecycle */
int kernel_bridge_create(const char *br);
int kernel_bridge_delete(const char *br);

/* admin state */
int kernel_bridge_set_up(const char *br);

/* vlan filtering */
int kernel_bridge_get_vlan_filtering(const char *br, bool *enabled);
int kernel_bridge_set_vlan_filtering(const char *br, bool enable);

/* ports */
int kernel_bridge_add_port(const char *br, const char *port);
int kernel_bridge_del_port(const char *br, const char *port);

int kernel_bridge_vlan_add(const char *bridge,
                           const char *port,
                           uint16_t vid,
                           bool tagged,
                           bool pvid);

int kernel_bridge_vlan_del(const char *bridge,
                           const char *port,
                           uint16_t vid);