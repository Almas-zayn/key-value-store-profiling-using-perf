#define _GNU_SOURCE

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SOCKET_PATH "/tmp/kvstore_ht.sock"
#define BACKLOG 10
#define BUF_SIZE 1024
#define HASH_SIZE 1024

typedef struct kv_entry
{
    char key[256];
    char value[768];
    struct kv_entry *next;
} kv_entry_t;

static kv_entry_t *hash_table[HASH_SIZE];
static int listen_fd = -1;

/*------------------------------------------------
  Error handler
-------------------------------------------------*/
static void die(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

/*------------------------------------------------
  Cleanup function
-------------------------------------------------*/
static void cleanup(void)
{
    if (listen_fd != -1)
        close(listen_fd);
    unlink(SOCKET_PATH);
    // Free hash table
    for (size_t i = 0; i < HASH_SIZE; ++i)
    {
        kv_entry_t *curr = hash_table[i];
        while (curr)
        {
            kv_entry_t *tmp = curr;
            curr = curr->next;
            free(tmp);
        }
    }
}

/*------------------------------------------------
  Signal handler
-------------------------------------------------*/
static void on_signal(int sig)
{
    (void)sig;
    exit(0);
}

/*------------------------------------------------
  Read a line from socket
-------------------------------------------------*/
static ssize_t read_line(int fd, char *buf, size_t maxlen)
{
    size_t n = 0;
    while (n + 1 < maxlen)
    {
        char c;
        ssize_t r = read(fd, &c, 1);
        if (r == 0)
            break;
        if (r < 0)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        buf[n++] = c;
        if (c == '\n')
            break;
    }
    buf[n] = '\0';
    return (ssize_t)n;
}

/*------------------------------------------------
  Write all bytes
-------------------------------------------------*/
static int write_all(int fd, const void *buf, size_t len)
{
    const char *p = buf;
    while (len > 0)
    {
        ssize_t w = write(fd, p, len);
        if (w < 0)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        p += w;
        len -= (size_t)w;
    }
    return 0;
}

/*------------------------------------------------
  Simple hash function (djb2)
-------------------------------------------------*/
static unsigned long hash(const char *str)
{
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash % HASH_SIZE;
}

/*------------------------------------------------
  Set key-value pair
-------------------------------------------------*/
static void set_kv(const char *key, const char *value)
{
    unsigned long idx = hash(key);
    kv_entry_t *curr = hash_table[idx];
    while (curr)
    {
        if (strcmp(curr->key, key) == 0)
        {
            strncpy(curr->value, value, sizeof(curr->value) - 1);
            curr->value[sizeof(curr->value) - 1] = '\0';
            return;
        }
        curr = curr->next;
    }
    kv_entry_t *new_entry = malloc(sizeof(kv_entry_t));
    if (!new_entry)
        return;
    strncpy(new_entry->key, key, sizeof(new_entry->key) - 1);
    new_entry->key[sizeof(new_entry->key) - 1] = '\0';
    strncpy(new_entry->value, value, sizeof(new_entry->value) - 1);
    new_entry->value[sizeof(new_entry->value) - 1] = '\0';
    new_entry->next = hash_table[idx];
    hash_table[idx] = new_entry;
}

/*------------------------------------------------
  Get value by key
-------------------------------------------------*/
static const char *get_kv(const char *key)
{
    unsigned long idx = hash(key);
    kv_entry_t *curr = hash_table[idx];
    while (curr)
    {
        if (strcmp(curr->key, key) == 0)
            return curr->value;
        curr = curr->next;
    }
    return NULL;
}

/*------------------------------------------------
  Handle client
-------------------------------------------------*/
static int handle_client(int client_fd)
{
    char buf[BUF_SIZE];
    ssize_t n = read_line(client_fd, buf, sizeof(buf));
    if (n <= 0)
    {
        printf("recieved 0 bytes");
        return 0; // client closed
    }

    char command[8], key[256], value[768];
    if (sscanf(buf, "SET %255s %767[^\n]", key, value) == 2)
    {
        set_kv(key, value);
        write_all(client_fd, "OK\n", 3);
    }
    else if (sscanf(buf, "GET %255s", key) == 1)
    {
        const char *val = get_kv(key);
        if (val)
        {
            char out[BUF_SIZE];
            int m = snprintf(out, sizeof(out), "%s\n", val);
            if (m > 0)
                write_all(client_fd, out, (size_t)m);
        }
        else
        {
            write_all(client_fd, "NOT_FOUND\n", 10);
        }
    }
    else
    {
        write_all(client_fd, "ERROR: Use SET <key> <value> or GET <key>\n", 40);
    }

    return 1; // continue reading from client
}

/*------------------------------------------------
  Main server
-------------------------------------------------*/
int main(void)
{
    atexit(cleanup);

    struct sigaction sa = {0};
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    umask(077);

    listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd == -1)
        die("socket");

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    unlink(SOCKET_PATH);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
        die("bind");
    if (chmod(SOCKET_PATH, 0600) == -1)
        die("chmod");
    if (listen(listen_fd, BACKLOG) == -1)
        die("listen");

    fprintf(stderr, "[server] listening on %s\n", SOCKET_PATH);

    for (;;)
    {
        int client_fd = accept(listen_fd, NULL, NULL);
        if (client_fd == -1)
        {
            if (errno == EINTR)
                continue;
            die("accept");
        }

        while (1)
        {
            if (!handle_client(client_fd))
                break; // handle_client returns 0 on EOF
        }
        close(client_fd);
    }
}
