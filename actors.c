#include "actors.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define READBUFSIZE 1024

// Creates a socket, binds & listens
int context_listen(Context* c) {
    // Open a new socket
    c->sock = socket(AF_INET, SOCK_STREAM, 0);
    if (c->sock < 0) {
        perror(NULL);
        return -1;
    }

    // Bind to the socket
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(c->addr.port);
    sin.sin_addr = c->addr.host;
    if (bind(c->sock, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        perror("Socket bind failed: ");
        return -1;
    }

    // Listen on the socket
    if (listen(c->sock, 1) < 0) {
        perror("Listen failed: ");
        return -1;
    }
    return 0;
}

void context_read_network(Context* c, int sock) {
    char buf[READBUFSIZE];
    struct sockaddr_in cin;
    for (;;) {
        memset(&cin, 0, sizeof(cin));
        int d = accept(c->sock, (struct sockaddr*)&cin, NULL);
        if (d < 0) {
            continue;
        }
        ssize_t n = 0;
        // TODO -- select() the descriptor for r/w.
        // Make sure this method can detect a dropped connection.
        while ((n = read(d, buf, READBUFSIZE)) > 0) {

        }
        if (n < 0) {
            perror("Read failed: ");
        }
    }
}

// Thread main function.  Returns NULL on failure, else Context*
void* context_run(void* context) {
    Context* c = (Context*)context;
    if (context_listen(c) < 0) {
        // TODO -- do we shutdown the thread from within, or from
        // the main crater process?
        return NULL;
    }
    context_read_network(c)
    return c;
}

// Spawns a new thread with the context handler
void context_spawn(Context* c) {
    pthread_create(&c->thread, &c->thread_attr, &context_run, c);
}

void context_destroy(Context* c) {
}

// Sets Context.addr given a hostname of the form "xxx.xx.xx.xxx:yyyy".
// Returns 0 on success, -1 on error.
int context_set_addr(Context* c, const char* hostname) {
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
    int ret = inet_aton(ip, &c->addr.host);
    free(ip);
    return ret;
}
