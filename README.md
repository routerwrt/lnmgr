# lnmgr – Link Manager

lnmgr is a deterministic, event-driven link manager for Linux.

It manages the administrative state and lifecycle of network links
(physical and virtual) based on a declarative dependency graph and
observed system signals.

lnmgr does **not** manage routing, firewalling, policy, preference,
or user intent.

## What lnmgr does
- Brings links up and down explicitly
- Reacts to kernel and userspace events
- Manages virtual link constructs (bridges, VLANs, tunnels)
- Emits observable state changes

## What lnmgr does not do
- Choose between links
- Infer dependencies
- Manage routing or firewall rules
- Implement network policy

See DESIGN.md for details.
