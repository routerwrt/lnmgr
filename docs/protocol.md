# lnmgr protocol

Version: **1**

This document defines the **external control and observation protocol** for `lnmgrd`.

The protocol is intentionally minimal and stable.
It exposes **state**, **intent**, and **structure** — not policy.

---

## 1. Transport

- UNIX domain socket
- Type: `SOCK_STREAM`
- Default path: `/run/lnmgr.sock`
- Server: `lnmgrd`
- Clients: CLI, UI, automation tools

Access control is provided **only** by filesystem permissions on the socket.

---

## 2. Session model

- One request → one response
- Requests are **line-based ASCII**
- Responses are **single JSON objects**
- No multiplexing
- No implicit state between requests

The daemon does **not** retain client state.

---

## 3. Version negotiation

Client → daemon:
HELLO 1

Daemon → client:
{ "type": "hello", "version": 1 }

If the version is unsupported:
{ "error": "unsupported-version", "supported": [1] }

The client MUST send HELLO before any other command.

---

## 4. Command format

COMMAND [ARG...]

- Commands are uppercase
- Arguments are space-separated
- One command per line
- Trailing newline required

Unknown commands return an error.

---

## 5. Core commands (v1)

STATUS
DUMP
SAVE
LOAD
FLUSH

---

## 6. Node state model

States: inactive, waiting, active, failed
Explain: none, disabled, blocked, signal, failed

---

## 7. Errors

Errors are always returned as JSON.

---

## 8. Compatibility rules

- Protocol version is mandatory
- Fields may be added, never removed
- Enums may be extended, never redefined
