#include "actors.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define READBUFSIZE 1024

typedef struct {
    char* buf;
    size_t len;
    size_t max;
} ContextBuffer;

static void read_client(int client, ContextBuffer* rbuf, ContextBuffer* wbuf) {
    rbuf->len = 0;
    wbuf->len = 0;
    ssize_t n = 0;
    fd_set readset;
    FD_ZERO(&readset);
    FD_SET(client, &readset);
    fd_set writeset;
    FD_ZERO(&writeset);
    FD_SET(client, &readset);
    for (;;) {
        select(client + 1, &readset, &writeset, NULL, NULL);
        if (FD_ISSET(client, &readset)) {
            // TODO -- read from descriptor
        }
        if (FD_ISSET(client, &writeset)) {
            // TODO -- write to descriptor.  Where do we get our write
            // data from?
        }
    }
    if (n < 0) {
        perror("Read failed: ");
    }
}

/*

Ahhh single listener

Main thread does nonblocking accept()
If a client connects, spawn a new thread to handle it
Client sends configuration packet
Main thread also handles thread cleanup


Create thread listening on port $p
Wait for connection
When we get a connection, begin thread main loop

Thread main loop:
-Write buffer
-Read buffer
Timeout select (or fcntl + nonblocking, or epoll, or poll - have to benchmark)
When we get a read event, parse the message
If the message is a request for more data, use writev to write up to $n buffers
(measured in either bytes or entries) from the ring buffer to the descriptor
-Might not be able to use writev because we will need to length prefix & tag each
buffer. Unless we make it more complicated and have a single indexing message

If the message is a give-data request, check that we can accept data. If so,
write the data to the entry output slot.

Messages:

(uint64) Length (uint8) ID (byte[N]) data
Length = N + 1

get-data
    (byte) Input or Output
    (byte) Max bytes or Max elements
    (uint64) Maximum

give-data
    (byte) Input or Output
    (uint64) Length
    (byte[N]) data

*/

// Thread main function.  Returns NULL on failure, else Context*
void* context_run(void* context) {
    Context* c = (Context*)context;

    return c;
}

// select()s data from the context's client
void context_select(Context* c) {

}

// Spawns a new thread with the context handler
int context_spawn(Context* c) {
    if (pthread_attr_setdetachstate(&c->thread_attr,
                                    PTHREAD_CREATE_JOINABLE) != 0) {
        perror("Failed to make thread joinable: ");
        return -1;
    }
    if (pthread_create(&c->thread, &c->thread_attr, &context_run, c) != 0) {
        perror("Failed to create thread: ");
        return -1;
    }
    return 0;
}

int context_destroy(Context* c) {
    if (close(c->client) < 0) {
        perror("Failed to close client: ");
        return -1;
    }
    void* ret = NULL;
    if (pthread_join(c->thread, &ret) != 0) {
        perror("pthread join failed: ");
    } else {
        if (ret == NULL) {
            printf("Thread ended in failed state\n");
        }
    }
    return 0;
}

Context* context_alloc(int client) {
    Context* c = (Context*)calloc(1, sizeof(*c));
    c->client = client;
    return c;
}

static void consumer_destroy(Consumer* c) {
    context_destroy(c->c);
    c->c = NULL;
}

void consumers_destroy(Consumers* c) {
    for (size_t i = 0; i < c->len; i++) {
        consumer_destroy(c->i[i]);
    }
    free(c->i);
    c->i = NULL;
    c->len = 0;
    c->max = 0;
}

void producer_destroy(Producer* p) {
    context_destroy(p->c);
    p->c = NULL;
}

static void consumer_group_destroy(ConsumerGroup* g) {
    free(g->i);
    g->i = NULL;
}

void consumer_groups_destroy(ConsumerGroups* g) {
    for (size_t i = 0; i < g->len; i++) {
        consumer_group_destroy(g->i[i]);
    }
    free(g->i);
    g->i = NULL;
    g->len = 0;
    g->max = 0;
}
