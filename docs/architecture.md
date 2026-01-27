# lnmgr Architecture

This document describes the core invariants and responsibilities of lnmgr.
It is intentionally short. If this grows, something went wrong.

## Core model

lnmgr separates **intent**, **activation**, and **readiness**.

- **Intent**: whether a node is enabled (admin decision)
- **Activation**: one-time administrative action (e.g. `ip link set up`)
- **Readiness**: operational usability, determined by signals

These concepts are strictly independent.

## Node lifecycle

A node progresses through the following logical states:

- `INACTIVE`  – disabled by intent
- `WAITING`   – enabled, activated, but not ready
- `ACTIVE`    – ready for use
- `FAILED`    – activation failed (explicit error)

### State transitions

- `INACTIVE → WAITING`
  - caused by enable intent
  - triggers activation exactly once

- `WAITING → ACTIVE`
  - all required signals are satisfied

- `ACTIVE → WAITING`
  - any required signal becomes unsatisfied

Signals **never** cause deactivation.

## Activation rules

- Activation is **idempotent**
- Activation happens **once per enable**
- Activation does **not** depend on readiness signals
- Loss of signals does **not** trigger deactivation

Deactivation only occurs on explicit intent change
(e.g. disable, config removal, daemon shutdown).

## Signals

Signals represent **operational conditions**, not commands.

Examples:
- carrier
- association
- tunnel up
- SLA reachability

Signals may change frequently.
They must never cause administrative state changes.

## Netlink handling

- Netlink dumps are treated as streams, not snapshots
- After an initial dump, the graph is re-evaluated once to converge
- Runtime events trigger re-evaluation
- Status is emitted only on effective state changes

## Layering

- `graph/` implements the pure state machine
- `actions/` perform side effects (activation / deactivation)
- `lnmgrd` observes kernel state and reports status
- Policy is explicitly outside the graph

If these boundaries blur, bugs follow.
