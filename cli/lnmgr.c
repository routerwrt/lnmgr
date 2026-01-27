#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#define LNMGR_SOCKET_PATH "/run/lnmgr.sock"

static int connect_socket(void)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, LNMGR_SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static int send_command(int fd, const char *cmd)
{
    size_t len = strlen(cmd);
    if (write(fd, cmd, len) != (ssize_t)len)
        return -1;
    if (write(fd, "\n", 1) != 1)
        return -1;
    return 0;
}

static void read_and_print_reply(int fd)
{
    char buf[4096];
    ssize_t r;

    while ((r = read(fd, buf, sizeof(buf))) > 0) {
        fwrite(buf, 1, r, stdout);
    }
}

static void usage(const char *argv0)
{
    fprintf(stderr,
        "usage:\n"
        "  %s status [node]\n"
        "  %s dump\n"
        "  %s save\n",
        argv0, argv0, argv0);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    char cmd[256];

    if (strcmp(argv[1], "status") == 0) {
        if (argc == 2) {
            snprintf(cmd, sizeof(cmd), "STATUS");
        } else if (argc == 3) {
            snprintf(cmd, sizeof(cmd), "STATUS %s", argv[2]);
        } else {
            usage(argv[0]);
            return 1;
        }
    } else if (strcmp(argv[1], "dump") == 0) {
        if (argc != 2) {
            usage(argv[0]);
            return 1;
        }
        snprintf(cmd, sizeof(cmd), "DUMP");
    } else if (strcmp(argv[1], "save") == 0) {
        if (argc != 2) {
            usage(argv[0]);
            return 1;
        }
        snprintf(cmd, sizeof(cmd), "SAVE");
    } else {
        usage(argv[0]);
        return 1;
    }

    int fd = connect_socket();
    if (fd < 0) {
        perror("lnmgr: connect");
        return 1;
    }

    write(fd, "HELLO\n", 6);
    read_reply(fd);   /* ignore for now, but read it */

    if (send_command(fd, cmd) < 0) {
        perror("lnmgr: write");
        close(fd);
        return 1;
    }

    read_and_print_reply(fd);
    close(fd);
    return 0;
}