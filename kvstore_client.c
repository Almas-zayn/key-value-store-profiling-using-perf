#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#define SOCKET_PATH "/tmp/kvstore_ht.sock"
#define BUF_SIZE 1024

static void die(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

static ssize_t read_line(int fd, char *buf, size_t maxlen)
{
    size_t n = 0;
    while (n + 1 < maxlen)
    {
        char c;
        ssize_t r = read(fd, &c, 1);
        if (r == 0)
            break; // EOF
        if (r < 0)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        buf[n++] = c;
        if (c == '\n')
            break; // stop at newline
    }
    buf[n] = '\0';
    return (ssize_t)n;
}

int main(void)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1)
        die("socket");

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
        die("connect");

    char buf[BUF_SIZE];
    while (1)
    {
        printf("> ");
        if (!fgets(buf, sizeof(buf), stdin))
            break;

        // Remove trailing newline
        size_t len = strlen(buf);
        if (len && buf[len - 1] == '\n')
            buf[len - 1] = '\0';

        // Handle QUIT command
        if (strcasecmp(buf, "QUIT") == 0)
            break;

        // Validate command format
        if (strncasecmp(buf, "SET ", 4) == 0 || strncasecmp(buf, "GET ", 4) == 0)
        {
            // Send to server

            strcat(buf, "\n"); // add newline
            if (write(fd, buf, strlen(buf)) == -1)
                die("write");

            ssize_t n = read_line(fd, buf, sizeof(buf) - 1);
            if (n <= 0)
            {
                printf("Server closed connection\n");
                break;
            }
            buf[n] = '\0';
            printf("%s", buf);
        }
        else
        {
            printf("Invalid command. Use:\n  SET <key> <value>\n  GET <key>\n  QUIT\n");
        }
    }

    close(fd);
    return 0;
}
