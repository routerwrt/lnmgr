CC ?= cc
CFLAGS ?= -Wall -Wextra -O2

SRC = \
    src/lnmgrd.c \
    src/graph.c \
    src/actions.c \
    src/signals.c \
    src/lnmgr_status.c

SRC += src/json/jsmn_impl.c

OBJ = $(SRC:.c=.o)

lnmgrd: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

clean:
	rm -f $(OBJ) lnmgrd
