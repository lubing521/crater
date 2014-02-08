#include "server.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


static int server_listen(Addr addr) {
    // Open a new socket
    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) {
        perror(NULL);
        return -1;
    }

    // Bind to the socket
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(addr.port);
    sin.sin_addr = addr.host;
    if (bind(server, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        perror("Socket bind failed: ");
        return -1;
    }

    // Listen on the socket
    if (listen(server, 1) < 0) {
        perror("Listen failed: ");
        return -1;
    }

    return server;
}

// Accept a client connection
static int server_accept(int server) {
    struct sockaddr_in cin;
    memset(&cin, 0, sizeof(cin));
    socklen_t sin_size = sizeof(cin);
    return accept(server, (struct sockaddr*)&cin, &sin_size);
}

static void terminate_client(int client) {
    if (close(client) < 0) {
        perror("Client close failed: ");
    }
}

// Blocks while waiting for ConfigureMessage from a client socket.
// Returns -1 on error, 0 on success.
static int read_client_config(int client, Buffer* buf, ConfigureMessage* m) {
    for (;;) {
        // The configure message should fit in the buffer we were given
        // If not, fail
        if (buf->len >= buf->max) {
            return -1;
        }
        // Read from the client socket
        ssize_t r = read(client, buf->buf, buf->max);
        if (r < 0) {
            perror("Client read failed: ");
            return -1;
        }
        // Update the buffer
        buf->len += (size_t)r;
        // Parse the message header
        uint64_t rmlen = 0;
        MessageType rmtype = MSG_UNKNOWN;
        size_t n = parse_message_header(buf->buf, buf->len, &rmlen, &rmtype);
        if (rmlen > MSGMAXLEN) {
            return -1;
        }
        // If we were unable to parse the header, go read more from the client
        if (n == 0) {
            continue;
        }
        // Parse the message body
        if (parse_message_configure(buf->buf, buf->len - n, m) == 0) {
            printf("Client configuration failed\n");
            return -1;
        }
        break;
    }
    return 0;
}


// Listens on Addr until all expected clients have connected.  Returns -1
// if it fails to achieve this.
int server_run(Addr addr, Crater* crater) {
    int server = server_listen(addr);
    if (server < 0) {
        return -1;
    }
    Buffer buf;
    buffer_alloc(&buf, 1024);
    for (;;) {
        int client = server_accept(server);
        if (client < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("Server accept failed: ");
            }
            continue;
        }

        // Block on client read until we get a configuration packet from them.
        // This may interfere with other client connection attempts, if this
        // client is misbehaving.  We want the configuration first so we can
        // avoid having the context thread handle it, requiring a lock on
        // the crater's actors.  If this is a problem, we can collect client
        // descriptors into an array and use a non-blocking select, while using
        // a timeout for accept
        ConfigureMessage m;
        int ret = read_client_config(client, &buf, &m);
        buffer_reset(&buf);
        if (ret < 0) {
            terminate_client(client);
            continue;
        }

        // Start a new context for this client
        Context* ctx = context_alloc(client);
        int ready = crater_add_context(crater, ctx, m);
        if (ready < 0) {
            terminate_client(client);
            continue;
        }
        if (context_spawn(ctx) != 0) {
            crater_undo_add_context(crater, m);
            terminate_client(client);
            ready = 0;
        }
        if (ready > 0) {
            break;
        }
    }
    buffer_free(&buf);
    if (close(server) < 0) {
        perror("Failed to close server: ");
    }
    return 0;
}

// Converts hostname of the form "xxx.xx.xx.xxx:yyyy" to an Addr
// Returns 0 on success, -1 on error.
int addr_from_hostname(const char* hostname, Addr* addr) {
    uint16_t port = 0;
    char* colon = strchr(hostname, ':');
    if (colon != NULL) {
        int iport = atoi(colon + 1);
        if (iport < 0 || iport > UINT16_MAX) {
            return -1;
        }
        port = (uint16_t)iport;
    }
    addr->port = port;
    size_t ip_len = 0;
    if (colon == NULL) {
        ip_len = strlen(hostname);
    } else {
        ip_len = colon - hostname;
    }
    char* ip = malloc(ip_len + 1);
    strncpy(ip, hostname, ip_len);
    ip[ip_len] = '\0';
    int ret = inet_aton(ip, &addr->host);
    free(ip);
    return ret;
}
