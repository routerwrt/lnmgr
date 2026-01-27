CC ?= cc
CFLAGS ?= -Wall -Wextra -O2

SRC = src/lnmgrd.c src/graph.c src/netlink.c src/actions.c src/socket.c
OBJ = $(SRC:.c=.o)

lnmgrd: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

clean:
	rm -f $(OBJ) lnmgrd
