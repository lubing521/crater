#include "crater.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

Crater* crater_alloc(uint64_t len) {
    Crater* c = calloc(1, sizeof(*c));
    c->buffer = calloc(len, sizeof(*c->buffer));
    c->len = len;
    return c;
}

void crater_destroy(Crater* c) {
    producer_destroy(&c->producer);
    consumer_groups_destroy(&c->groups);
    consumers_destroy(&c->consumers);
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

bool crater_ready(Crater* c) {
    // TODO -- check that all expected producers & consumers are loaded
    // We need a config loader for this

    // Once they're ready, we switch to the next mode
    return false;
}

void crater_start(Crater* c) {
    // Starts the vacuum thread
}

bool crater_add_context(Crater* c, Context* ctx) {
    return false;
}
