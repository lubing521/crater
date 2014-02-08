#include "actors.h"

#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>

#include "messages.h"
#include "crater.h"

#define READBUFSIZE 1024
#define WRITBUFSIZE 1024

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
        size_t size = rbuf->max - rbuf->len;
        if (size == 0) {
            if (buffer_grow(rbuf) < 0) {
                return -1;
            }
        }
        ssize_t n = read(client, rbuf->buf, size);
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

// Reads an incoming message and handles it if recognized.
// Returns -1 on error.
static int context_handle_incoming(Context* ctx, Buffer* rbuf, Buffer* wbuf) {
    uint64_t rmlen = 0;
    MessageType rmtype = MSG_UNKNOWN;
    size_t n = parse_message_header(rbuf->buf, rbuf->len, &rmlen, &rmtype);
    if (rmlen > MSGMAXLEN) {
        printf("Received message is too long\n");
        return -1;
    }
    if (n == 0) {
        printf("Not enough in the header\n");
        return 0;
    }
    switch (rmtype) {
    case MSG_GET_DATA: {
        GetDataMsg m;
        size_t r = parse_message_get_data(&rbuf->buf[n], rbuf->len - n, &m);
        if (r == 0) {
            return buffer_grow(rbuf);
        } else {
            // Move remaining data to the beginning of the buffer
            // TODO -- use circular buffer
            buffer_strip(rbuf, r + n);
            int ret = context_process_get_data_msg(ctx, m, wbuf);
            get_data_msg_destroy(&m);
            if (ret < 0) {
                printf("Failed to process get data\n");
            }
            return ret;
        }
    }; break;
    case MSG_GIVE_DATA: {
        GiveDataMsg m;
        size_t r = parse_message_give_data(&rbuf->buf[n], rbuf->len - n, &m);
        if (r == 0) {
            return buffer_grow(rbuf);
        } else {
            int ret = context_process_give_data_msg(ctx, m);
            if (ret < 0) {
                printf("Failed to process give-data\n");
            }
            give_data_msg_destroy(&m);
            return ret;
        }
    }; break;
    case MSG_UNKNOWN:
    default:
        printf("Unknown message received\n");
        break;
    }
    buffer_reset(rbuf);
    return 0;
}

static int handle_outgoing(int client, Buffer* buf) {
    if (buf->len == 0) {
        return 0;
    }
    ssize_t n = write(client, buf->buf, buf->len);
    if (n < 0) {
        perror("Failed to write to client: ");
    }
    return n;
}

// Thread main function.  Returns NULL on failure, else Context*
void* context_run(void* context) {
    Context* c = (Context*)context;
    // TODO -- switch to circular buffer for reading
    Buffer rbuf;
    buffer_alloc(&rbuf, READBUFSIZE);
    Buffer wbuf;
    buffer_alloc(&wbuf, WRITBUFSIZE);
    while (client_rw(c->client, &rbuf, &wbuf) == 0) {
        int ret = context_handle_incoming(c, &rbuf, &wbuf);
        if (ret < 0) {
            printf("Failed to handle incoming message\n");
            break;
        }
        if (wbuf.len > 0) {
            size_t wrote = 0;
            while (wrote < wbuf.len) {
                ssize_t n = handle_outgoing(c->client, &wbuf);
                if (n < 0) {
                    printf("Failed to handle outgoing message\n");
                    break;
                }
                wrote += n;
            }
        }
    }

    buffer_free(&rbuf);
    buffer_free(&wbuf);

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
    c->actor = NULL;
    return c;
}

int context_destroy(Context* c) {
    // Assume that the client is closed regardless of failure, and that the
    // context thread has stopped.
    if (close(c->client) != 0) {
        perror("Failed to close client: ");
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

static int actors_resize(Actors* a, size_t max) {
    if (max < a->len) {
        return -1;
    }
    Actor* i = realloc(a->i, max * sizeof(*i));
    if (i == NULL) {
        return -1;
    }
    a->max = max;
    a->i = i;
    return 0;
}

static int actors_grow(Actors* a) {
    return actors_resize(a, a->max * 2);
}

void actors_alloc(Actors* a, size_t start) {
    a->i = calloc(start, sizeof(*a->i));
    a->len = 0;
    a->max = start;
}

void actors_destroy(Actors* c) {
    free(c->i);
    c->i = NULL;
    c->len = 0;
    c->max = 0;
}

// Returns a new Actor*
Actor* actors_fetch(Actors* a) {
    if (a->len >= a->max) {
        if (actors_grow(a) < 0) {
            return NULL;
        }
    }
    Actor* actor = &a->i[a->len++];
    actor->slot = 0;
    actor->stride = 1;
    actor->type = ACTOR_UNKNOWN;
    return actor;
}

// Can only be called immediately after an unfetch.
int actors_unfetch(Actors* a) {
    if (a->len == 0) {
        return -1;
    }
    a->len--;
    return 1;
}

static void actor_group_destroy(ActorGroup* g) {
    free(g->i);
    g->i = NULL;
}

void actor_groups_destroy(ActorGroups* g) {
    for (size_t i = 0; i < g->len; i++) {
        actor_group_destroy(g->i[i]);
    }
    free(g->i);
    g->i = NULL;
    g->len = 0;
    g->max = 0;
}

int context_process_get_data_msg(Context* ctx, GetDataMsg m, Buffer* wbuf) {
    buffer_reset(wbuf);
    if (m.max_type == GDMAX_UNKNOWN || m.io == SLOT_UNKNOWN) {
        return -1;
    }
    uint64_t max = m.max;
    // TODO -- this is cross-thread read, make sure its ok (memory barrier?)
    uint64_t max_slot = 0;
    switch (m.io) {
    case SLOT_INPUT:
        max_slot = ctx->crater->producer.slot;
        break;
    case SLOT_OUTPUT:
        max_slot = ctx->crater->transformer.slot;
        break;
    default:
        assert(false);
        return -1;
    }
    if (m.max_type == GDMAX_ELEMS) {
        size_t avail = max_slot - ctx->actor->slot;
        if (avail < max) {
            max = avail;
        }
    }

    uint64_t slot = ctx->actor->slot;
    while (slot < max_slot) {
        Buffer buf;
        switch (m.io) {
        case SLOT_INPUT:
            buf = crater_get_input(ctx->crater, slot);
            break;
        case SLOT_OUTPUT:
            buf = crater_get_output(ctx->crater, slot);
            break;
        default:
            assert(false);
            return -1;
        }
        if (m.max_type == GDMAX_ELEMS && wbuf->len + buf.len > m.max) {
            break;
        }
        if (buffer_write(wbuf, buf.buf, buf.len) < 0) {
            return -1;
        }
        slot += ctx->actor->stride;
    }
    ctx->actor->slot = slot;

    return 0;
}

int context_process_give_data_msg(Context* ctx, GiveDataMsg m) {
    switch (ctx->actor->type) {
    case ACTOR_TRANSFORMER:
    case ACTOR_PRODUCER:
        break;
    default:
        printf("Actor can't write\n");
        return -1;
    }
    if (m.io == SLOT_UNKNOWN) {
        return -1;
    }
    // TODO -- this is cross-thread read. Review.  Do we need a memory barrier,
    // or is volatile sufficient?
    uint64_t max_slot = 0;
    switch (m.io) {
    case SLOT_INPUT:
        max_slot = ctx->crater->vacuum.slot;
        break;
    case SLOT_OUTPUT:
        max_slot = ctx->crater->producer.slot;
        break;
    default:
        assert(false);
        return -1;
    }
    uint64_t slot = ctx->actor->slot;
    for (uint64_t i = 0; i < m.n && slot < max_slot; i++, slot++) {
        SlotData data = m.data[i];
        switch (m.io) {
        case SLOT_INPUT:
            crater_set_copy_input(ctx->crater, slot, data.buf, data.len);
        case SLOT_OUTPUT:
            crater_set_copy_output(ctx->crater, slot, data.buf, data.len);
        default:
            assert(false);
            return -1;
        }
    }
    return 0;
}

void contexts_alloc(Contexts* c, size_t start) {
    c->len = 0;
    c->max = start;
    c->contexts = calloc(start, sizeof(*c->contexts));
}

void contexts_destroy(Contexts* c) {
    for (size_t i = 0; i < c->len; i++) {
        context_destroy(c->contexts[i]);
    }
    free(c->contexts);
    c->contexts = NULL;
}

static int contexts_resize(Contexts* c, size_t max) {
    if (max < c->len) {
        return -1;
    }
    Context** ctxs = realloc(c->contexts, max * sizeof(*ctxs));
    if (ctxs == NULL) {
        return -1;
    }
    c->max = max;
    c->contexts = ctxs;
    return 0;
}

static int contexts_grow(Contexts* c) {
    return contexts_resize(c, c->max * 2);
}

int contexts_add(Contexts* c, Context* ctx) {
    if (c->len >= c->max) {
        if (contexts_grow(c) < 0) {
            return -1;
        }
    }
    int p = (int)c->len;
    c->contexts[p] = ctx;
    c->len++;
    return p;
}

Context* contexts_pop(Contexts* c) {
    if (c->len == 0) {
        return NULL;
    }
    c->len--;
    return c->contexts[c->len];
}
