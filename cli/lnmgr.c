#include <stdbool.h>
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

static int write_all(int fd, const char *buf, size_t len)
{
    while (len > 0) {
        ssize_t n = write(fd, buf, len);
        if (n <= 0)
            return -1;
        buf += n;
        len -= n;
    }
    return 0;
}

static int send_command(int fd, const char *cmd)
{
    size_t len = strlen(cmd);
    if (write_all(fd, cmd, len) < 0)
        return -1;
    if (write_all(fd, "\n", 1) < 0)
        return -1;
    return 0;
}

static void read_and_print_stream(int fd)
{
    char buf[4096];
    ssize_t n;

    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        fwrite(buf, 1, n, stdout);

        /* optional: flush for interactive use */
        fflush(stdout);
    }
}

/* Read exactly one newline-terminated protocol message */
static void read_one_message(int fd, bool print)
{
    char buf[4096];
    ssize_t n;

    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        if (print)
            fwrite(buf, 1, n, stdout);

        if (buf[n - 1] == '\n')
            break;
    }
}

static void usage(const char *argv0)
{
    fprintf(stderr,
        "usage:\n"
        "  %s status [node]\n"
        "  %s dump\n"
        "  %s save\n"
        "  %s watch\n",
        argv0, argv0, argv0, argv0);
}

int main(int argc, char **argv)
{
    bool want_watch = false;
    bool show_hello = false;
    char cmd[256];

    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    /* -------- command parsing -------- */

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
        show_hello = true;
        snprintf(cmd, sizeof(cmd), "DUMP");

    } else if (strcmp(argv[1], "save") == 0) {
        if (argc != 2) {
            usage(argv[0]);
            return 1;
        }
        snprintf(cmd, sizeof(cmd), "SAVE");

    } else if (strcmp(argv[1], "watch") == 0) {
        if (argc != 2) {
            usage(argv[0]);
            return 1;
        }
        snprintf(cmd, sizeof(cmd), "SUBSCRIBE");
        want_watch = true;

    } else {
        usage(argv[0]);
        return 1;
    }

    /* -------- connect -------- */

    int fd = connect_socket();
    if (fd < 0) {
        perror("lnmgr: connect");
        return 1;
    }

    /* -------- protocol handshake -------- */

    if (write(fd, "HELLO\n", 6) != 6) {
        perror("lnmgr: write HELLO");
        close(fd);
        return 1;
    }

    /* read HELLO reply */
    read_one_message(fd, show_hello);

    /* -------- send command -------- */

    fprintf(stderr, "sending command: %s\n", cmd);
    
    if (send_command(fd, cmd) < 0) {
        perror("lnmgr: send command");
        close(fd);
        return 1;
    }

    /* -------- response handling -------- */

    if (want_watch) {
        /*
         * SUBSCRIBE semantics:
         *  - initial snapshot
         *  - then infinite event stream
         *  - only exits on error / EOF
         */
        read_and_print_stream(fd);
        close(fd);
        return 0;
    }

    /* one-shot commands */
    read_one_message(fd, true);
    close(fd);
    return 0;
}