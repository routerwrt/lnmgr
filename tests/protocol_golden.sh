#!/bin/sh
set -eu

SOCK=/run/lnmgr.sock

fail() { echo "FAIL: $*" >&2; exit 1; }

# Require daemon socket
[ -S "$SOCK" ] || fail "socket not found: $SOCK"

req() {
  # send one command and print response
  printf "%s\n" "$1" | socat - UNIX-CONNECT:"$SOCK"
}

contains() {
  echo "$1" | grep -q "$2" || fail "missing: $2"
}

# --- SAVE ---
SAVE="$(req SAVE)"
contains "$SAVE" '"version":[[:space:]]*1'
contains "$SAVE" '"nodes"[[:space:]]*:'
contains "$SAVE" '"id"[[:space:]]*:'
contains "$SAVE" '"type"[[:space:]]*:'
contains "$SAVE" '"enabled"[[:space:]]*:'
contains "$SAVE" '"auto"[[:space:]]*:'
contains "$SAVE" '"signals"[[:space:]]*:'
contains "$SAVE" '"requires"[[:space:]]*:'

# --- DUMP ---
DUMP="$(req DUMP)"
contains "$DUMP" '"type"[[:space:]]*:[[:space:]]*"dump"'
contains "$DUMP" '"nodes"[[:space:]]*:'
contains "$DUMP" '"id"[[:space:]]*:'
contains "$DUMP" '"type"[[:space:]]*:'
contains "$DUMP" '"enabled"[[:space:]]*:'
contains "$DUMP" '"auto"[[:space:]]*:'

# --- STATUS ---
STATUS="$(req STATUS)"
contains "$STATUS" '"type"[[:space:]]*:[[:space:]]*"status"'
contains "$STATUS" '"nodes"[[:space:]]*:'
contains "$STATUS" '"state"[[:space:]]*:[[:space:]]*"'

echo "OK: protocol golden tests passed"
