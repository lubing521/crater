#include "server.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

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
    return accept(server, (struct sockaddr*)&cin, NULL);
}

// Listens on Addr.  Return -1 on error.
int server_run(Addr addr, Crater* crater) {
    int server = server_listen(addr);
    if (server < 0) {
        return -1;
    }
    bool ready = false;
    for (;;) {
        int client = server_accept(server);
        if (client < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("Server accept failed: ");
                break;
            }
        } else {
            // Start a new context for this client
            Context* ctx = context_alloc(client);
            ready = crater_add_context(crater, ctx);
            if (context_spawn(ctx) != 0) {
                crater_remove_context(crater, ctx);
                ready = false;
                if (close(client) < 0) {
                    perror("Failed to close client: ");
                }
            }
            if (ready) {
                crater_start(crater);
            }
        }
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
