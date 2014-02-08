#include "crater.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

static void crater_config_init(CraterConfig* c, size_t n_consumers) {
    c->have_producer = false;
    c->have_transformer = false;
    c->have_consumers = 0;
    c->expect_consumers = n_consumers;
    c->expect_transformer = true;
    c->expect_producer = true;
}

Crater* crater_alloc(uint64_t len, size_t n_contexts, size_t n_consumers) {
    Crater* c = calloc(1, sizeof(*c));
    c->buffer = calloc(len, sizeof(*c->buffer));
    c->len = len;
    contexts_alloc(&c->contexts, n_contexts);
    actors_alloc(&c->consumers, n_consumers);
    crater_config_init(&c->config, n_consumers);
    return c;
}

void crater_destroy(Crater* c) {
    contexts_destroy(&c->contexts);
    actor_groups_destroy(&c->groups);
    actors_destroy(&c->consumers);
    for (size_t i = 0; i < c->len; i++) {
        free(c->buffer[i].input.buf);
        free(c->buffer[i].output.buf);
    }
    free(c->buffer);
    free(c);
}

Buffer crater_get_input(Crater* c, uint64_t pos) {
    return c->buffer[pos % c->len].input;
}

Buffer crater_get_output(Crater* c, uint64_t pos) {
    return c->buffer[pos % c->len].output;
}

void crater_set_input(Crater* c, uint64_t pos, char* data, size_t len,
                      size_t max) {
    assert(c->buffer[pos % c->len].input.buf == NULL);
    c->buffer[pos % c->len].input = (Buffer) {
        .buf = data, .len = len, .max = max
    };
}

void crater_set_output(Crater* c, uint64_t pos, char* data, size_t len,
                       size_t max) {
    assert(c->buffer[pos % c->len].output.buf == NULL);
    c->buffer[pos % c->len].output = (Buffer) {
        .buf = data, .len = len, .max = max
    };
}

void crater_set_copy_input(Crater* c, uint64_t pos, const char* data,
                           size_t len) {
    char* input = malloc(len);
    memcpy(input, data, len);
    crater_set_input(c, pos, input, len, len);
}

void crater_set_copy_output(Crater* c, uint64_t pos, const char* data,
                            size_t len) {
    char* output = malloc(len);
    memcpy(output, data, len);
    crater_set_output(c, pos, output, len, len);
}

static int crater_config_ready(CraterConfig c) {
    // TODO -- check that all expected producers & actors are loaded
    // We need a config loader for this
    bool have_transformer = (!c.expect_transformer ||
                             (c.expect_transformer && c.have_transformer));
    bool have_producer = (!c.expect_producer ||
                          (c.expect_producer && c.have_producer));
    bool have_consumers = (c.have_consumers >= c.expect_consumers);
    return (have_producer && have_transformer && have_consumers);
}

// Returns 1 if crater is ready, 0 if not ready and -1 on invalid configuration
int crater_add_context(Crater* c, Context* ctx, ConfigureMessage m) {
    switch (m.actor_type) {
    case ACTOR_PRODUCER:
        if (c->config.have_producer) {
            return -1;
        }
        ctx->actor = &c->producer;
        c->config.have_producer = true;
        break;
    case ACTOR_TRANSFORMER:
        if (c->config.have_transformer) {
            return -1;
        }
        ctx->actor = &c->transformer;
        c->config.have_transformer = true;
        break;
    case ACTOR_CONSUMER:
        ctx->actor = actors_fetch(&c->consumers);
        if (ctx->actor == NULL) {
            return -1;
        } else if (c->config.have_consumers >= c->config.expect_consumers) {
            return -1;
        } else {
            c->config.have_consumers++;
        }
        break;
    default:
        assert(false);
        return -1;
    }
    if (contexts_add(&c->contexts, ctx) < 0) {
        // This is fatal -- out of memory
        assert(false);
        return -1;
    }
    ctx->actor->type = m.actor_type;
    ctx->crater = c;
    return crater_config_ready(c->config);
}

// Can only be called immediately after crater_add_context, due to
// contexts_pop and actors_unfetch implementations
int crater_undo_add_context(Crater* c, ConfigureMessage m) {
    if (contexts_pop(&c->contexts) == NULL) {
        return -1;
    }
    switch (m.actor_type) {
    case ACTOR_PRODUCER:
        c->config.have_producer = false;
    case ACTOR_TRANSFORMER:
        c->config.have_transformer = false;
    case ACTOR_CONSUMER:
        return actors_unfetch(&c->consumers);
    default:
        return -1;
    }
    return 0;
}

void crater_start(Crater* c) {
    // Starts the vacuum thread
    for (;;) {
        // TODO -- check for dead threads (client closed)
        sleep(1);
    }
}
