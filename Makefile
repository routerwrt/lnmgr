CC ?= cc

BASE_CFLAGS := \
  -Wall -Wextra \
  -Wformat -Wformat-security \
  -Wshadow \
  -Wpointer-arith \
  -Wstrict-prototypes \
  -Wmissing-prototypes \
  -Werror=implicit-function-declaration

# POSIX.1-2008 for fcntl, sigaction, pipe, poll
BASE_CPPFLAGS := \
  -Isrc \
  -D_POSIX_C_SOURCE=200809L

CFLAGS   += $(BASE_CFLAGS)
CPPFLAGS += $(BASE_CPPFLAGS)

DAEMON_CFLAGS = $(CFLAGS)

ifeq ($(DEBUG),1)
DAEMON_CFLAGS += -DLNMGR_DEBUG
endif

CLI_CFLAGS    = $(CFLAGS)

all: lnmgrd lnmgr

# -----------------------------
# Daemon
# -----------------------------

SRC = \
    src/lnmgrd.c \
    src/node.c \
    src/graph.c \
    src/actions.c \
    src/lnmgr_status.c \
    src/config.c \
    src/socket.c \
    src/json/jsmn_impl.c \
    src/enum_str.c \
    src/signal/signal_netlink.c \
    src/signal/signal_nl80211.c

DAEMON_OBJ = $(SRC:.c=.daemon.o)

%.daemon.o: %.c
	$(CC) $(CPPFLAGS) $(DAEMON_CFLAGS) -c -o $@ $<

lnmgrd: $(DAEMON_OBJ)
	$(CC) $(LDFLAGS) -o $@ $(DAEMON_OBJ)

# -----------------------------
# CLI
# -----------------------------

CLI_SRC = cli/lnmgr.c
CLI_OBJ = $(CLI_SRC:.c=.cli.o)

%.cli.o: %.c
	$(CC) $(CPPFLAGS) $(CLI_CFLAGS) -c -o $@ $<

lnmgr: $(CLI_OBJ)
	$(CC) $(LDFLAGS) -o $@ $(CLI_OBJ)

# -----------------------------
# Misc
# -----------------------------

test-protocol:
	@tests/protocol_golden.sh

clean:
	rm -f $(DAEMON_OBJ) $(CLI_OBJ) lnmgr lnmgrd

.PHONY: all clean test-protocol
