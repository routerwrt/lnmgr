CC ?= cc
CFLAGS ?= -Wall -Wextra -O2

CFLAGS += -DLNMGR_DEBUG

SRC = \
    src/lnmgrd.c \
    src/graph.c \
    src/actions.c \
    src/signals.c \
    src/lnmgr_status.c \
    src/config.c \
    src/socket.c

SRC += src/json/jsmn_impl.c

OBJ = $(SRC:.c=.o)

lnmgrd: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

clean:
	rm -f $(OBJ) lnmgrd
