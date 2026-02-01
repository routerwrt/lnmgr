# Auto-Up Semantics

This document defines the exact semantics of the `auto` flag in lnmgr.

The goal is to provide predictable, non-intrusive behavior that works
correctly for routers, hot-plug devices, and long-running systems.

---

## Terminology

- **present**  
  The kernel reports that the device exists (RTM_NEWLINK received).

- **absent**  
  The kernel reports that the device no longer exists (RTM_DELLINK received).

- **auto**  
  Configuration intent: lnmgr may attempt to bring the node up automatically.

- **auto_latched**  
  Internal flag preventing repeated auto-activation attempts for the same
  device lifecycle.

---

## Design Principles

1. lnmgr must **never fight the user**
2. Kernel state is authoritative
3. Auto-up is **edge-triggered**, not level-triggered
4. Device lifecycles are explicit and observable
5. Behavior must be deterministic and restart-safe

---

## Auto-Up Rules

### Rule 1: Presence-gated

Auto-up only applies to nodes that are currently **present**.

If a device does not exist in the kernel, lnmgr will not attempt activation.

---

### Rule 2: One-shot per lifecycle

Auto-up is attempted **once per device appearance**.

- First appearance → auto-up may run
- Subsequent evaluations → auto-up will not repeat
- Auto-up is **latched** after the first attempt

---

### Rule 3: Reset on disappearance

When a device disappears (RTM_DELLINK):

- `auto_latched` is cleared
- `activated` is cleared
- The node returns to `NODE_INACTIVE`

If the device later reappears, auto-up may run again.

---

### Rule 4: No reaction to manual admin changes

Manual actions such as:

- `ip link set down`
- `ip link set up`

do **not** reset auto-up state.

lnmgr observes the new reality but does not re-assert intent.

---

### Rule 5: Auto does not mean "ensure up"

Auto-up means:

> *“Try to bring this node up once when it appears.”*

It does **not** mean:

- Keep it up forever
- Re-enable it after manual shutdown
- Fight user or kernel decisions

---

## State Transitions

### On device appearance
absent → present
auto_latched = false
activated    = false
state        = NODE_INACTIVE

Auto-up may move the node to `NODE_WAITING`.

---

### On auto-up trigger

Conditions:
- enabled == true
- auto == true
- present == true
- auto_latched == false
- state == NODE_INACTIVE

Action:
state        = NODE_WAITING
auto_latched = true
---

### On device disappearance
present → absent
auto_latched = false
activated    = false
state        = NODE_INACTIVE
---

## Non-Goals

Auto-up explicitly does **not**:

- Track history
- Retry failed activations indefinitely
- Enforce policy against the kernel
- Provide "auto-down" semantics

---

## Rationale

This model matches real-world router behavior:

- Fixed PHYs come up once at boot
- USB / hot-plug devices auto-configure on insertion
- Manual admin actions are respected
- The daemon remains passive and robust

This design intentionally favors **stability over cleverness**.