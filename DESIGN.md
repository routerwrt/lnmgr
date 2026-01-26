# lnmgr – Design Overview

## Core principles

- Intent is explicit
- Reality is observed
- Policy is external
- Links are managed, not guessed

## Responsibilities

- Kernel: creates interfaces and reports signals
- lnmgr: manages administrative state and virtual link lifecycle
- External tools: routing, firewalling, policy, UX

## Non-negotiables

- No inference
- No policy
- No per-node ACLs
- No implicit graph mutation
