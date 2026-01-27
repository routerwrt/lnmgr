CC ?= cc
CFLAGS ?= -Wall -Wextra -O2

DAEMON_CFLAGS = $(CFLAGS) -DLNMGR_DEBUG
CLI_CFLAGS    = $(CFLAGS)

all: lnmgrd lnmgr

SRC = \
    src/lnmgrd.c \
    src/graph.c \
    src/actions.c \
    src/signals.c \
    src/lnmgr_status.c \
    src/config.c \
    src/socket.c \
    src/json/jsmn_impl.c \
    src/graph_strings.c \
    src/protocol.c

OBJ = $(SRC:.c=.o)

lnmgrd: $(OBJ)
	$(CC) $(DAEMON_CFLAGS) -o $@ $(OBJ)

CLI_SRC = cli/lnmgr.c
CLI_OBJ = $(CLI_SRC:.c=.o)

lnmgr: $(CLI_OBJ)
	$(CC) $(CLI_CFLAGS) -o $@ $(CLI_OBJ)

clean:
	rm -f $(OBJ) $(CLI_OBJ) lnmgr lnmgrd

.PHONY: clean
