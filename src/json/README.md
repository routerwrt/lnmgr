# JSON parsing in lnmgr

lnmgr uses **jsmn**, a minimalistic, header-only JSON parser,
vendored directly into the source tree.

- Project: https://github.com/zserge/jsmn
- License: MIT
- Usage model: strict, schema-driven parsing
- No dynamic memory allocation
- No generic JSON object model

## Integration details

- `jsmn.h` is vendored unchanged.
- `jsmn_impl.c` provides the single implementation unit.
- All other users include `jsmn.h` with `JSMN_HEADER` defined.

## Design intent

JSON is used only for **configuration loading**.

- Unknown keys are rejected.
- The schema is closed and versioned.
- No comments, includes, or extensions.
- Parsing is explicit and deterministic.

This keeps lnmgr small, predictable, and suitable for
system and embedded use.
