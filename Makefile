CC ?= cc

BASE_CFLAGS := \
  -Wall -Wextra \
  -Wformat -Wformat-security \
  -Wshadow \
  -Wpointer-arith \
  -Wstrict-prototypes \
  -Wmissing-prototypes \
  -Werror=implicit-function-declaration

# POSIX.1-2008 for fcntl, sigaction, pipe, poll; portable across glibc/musl
BASE_CPPFLAGS := \
  -Isrc \
  -D_POSIX_C_SOURCE=200809L

CFLAGS   += $(BASE_CFLAGS)
CPPFLAGS += $(BASE_CPPFLAGS)

DAEMON_CFLAGS = $(CFLAGS) -DLNMGR_DEBUG
CLI_CFLAGS    = $(CFLAGS)


all: lnmgrd lnmgr

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

OBJ = $(SRC:.c=.o)

lnmgrd: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)

CLI_SRC = cli/lnmgr.c
CLI_OBJ = $(CLI_SRC:.c=.o)

lnmgr: $(CLI_OBJ)
	$(CC) $(LDFLAGS) -o $@ $(CLI_OBJ)

test-protocol:
	@tests/protocol_golden.sh

clean:
	rm -f $(OBJ) $(CLI_OBJ) lnmgr lnmgrd

.PHONY: all clean test-protocol
