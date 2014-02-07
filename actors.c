#include "actors.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define READBUFSIZE 1024
#define WRITBUFSIZE 1024

// Initializes a Buffer
void buffer_alloc(Buffer* b, size_t n) {
    b->len = 0;
    b->max = n;
    b->buf = malloc(n * sizeof(*(b->buf)));
}

// Releases the Buffer's resources
void buffer_free(Buffer* b) {
    b->len = 0;
    b->max = 0;
    free(b->buf);
}

// Reset the buffer index
void buffer_reset(Buffer* b) {
    b->len = 0;
}

// Resizes the Buffer to max
int buffer_resize(Buffer* b, size_t max) {
    if (max == b->max) {
        return 0;
    }
    if (max < b->len) {
        b->len = max;
    }
    char* buf = realloc(b->buf, max);
    if (buf == NULL) {
        return -1;
    } else {
        b->buf = buf;
    }
    return 0;
}

// Doubles the buffer length
int buffer_grow(Buffer* b) {
    return buffer_resize(b, b->max * 2);
}

// Reads from and writes to a client socket
static int client_rw(int client, Buffer* rbuf, Buffer* wbuf) {
    buffer_reset(rbuf);
    buffer_reset(wbuf);
    fd_set readset;
    FD_ZERO(&readset);
    FD_SET(client, &readset);
    fd_set writeset;
    FD_ZERO(&writeset);
    FD_SET(client, &readset);
    select(client + 1, &readset, &writeset, NULL, NULL);
    if (FD_ISSET(client, &readset)) {
        ssize_t n = read(client, rbuf->buf, rbuf->max);
        if (n < 0) {
            perror("Failed to read from client: ");
        } else if (n == 0) {
            printf("Client %d closed their connection\n", client);
            return -1;
        } else {
            rbuf->len = (size_t)n;
        }
    }
    if (FD_ISSET(client, &writeset)) {
        ssize_t n = write(client, wbuf->buf, wbuf->len);
        if (n < 0) {
            perror("Failed to write to client: ");
            return -1;
        } else {
            wbuf->len = n;
        }
    }
    return 0;
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
    [(uint64) Length
    (byte[N]) data]

*/

// Thread main function.  Returns NULL on failure, else Context*
void* context_run(void* context) {
    Context* c = (Context*)context;
    Buffer rbuf;
    buffer_alloc(&rbuf, READBUFSIZE);
    Buffer wbuf;
    buffer_alloc(&wbuf, WRITBUFSIZE);
    while (client_rw(c->client, &rbuf, &wbuf) == 0) {
        // TODO -- NEXT IMMEDIATELY -- MESSAGE PARSING
        //         GROWING BUFFER IMPLEMENTATION
        // Check the rbuf length
        // Parse it into a message
        //   If the message is incomplete, keep growing the buffer until
        //   we can parse it completely.  Reject anything with too long length
        // If its requesting data,
        //   Set a read marker
        //   Fill the wbuf with as much data as possible. But don't need to
        //   grow too large.
        //   Keep sending until marker terminates
        // If its sending data,
        //   Copy the data out of the buffer and set it onto the ring buffer
    }
    return c;
}

int context_do_thread(Context* c) {
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

Context* context_alloc(int client) {
    Context* c = (Context*)calloc(1, sizeof(*c));
    c->client = client;
    return c;
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

// Spawns a new thread with the context handler
int context_spawn(Context* ctx) {
    if (context_do_thread(ctx) != 0) {
        if (context_destroy(ctx) != 0) {
            printf("Failed to destroy context for client %d\n", ctx->client);
        }
        return -1;
    }
    return 0;
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
