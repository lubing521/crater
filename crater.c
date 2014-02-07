#include "crater.h"
#include <stdlib.h>
#include <string.h>

Crater* crater_alloc(uint64_t len) {
    Crater* c = malloc(sizeof(*c));
    c->buffer = calloc(len, sizeof(*c->buffer));
    c->len = len;
    return c;
}

void crater_destroy(Crater* c) {
    for (size_t i = 0; i < c->len; i++) {
        free(c->buffer[i].input);
        free(c->buffer[i].output);
    }
    free(c->buffer);
    free(c);
}

const char* crater_get_input(Crater* c, uint64_t pos) {
    return c->buffer[pos % c->len].input;
}

const char* crater_get_output(Crater* c, uint64_t pos) {
    return c->buffer[pos % c->len].output;
}

void crater_set_input(Crater* c, uint64_t pos, char* data) {
    c->buffer[pos % c->len].input = data;
}

void crater_set_output(Crater* c, uint64_t pos, char* data) {
    c->buffer[pos % c->len].output = data;
}

void crater_set_copy_input(Crater* c, uint64_t pos, const char* data, size_t len) {
    char* input = malloc(len);
    memcpy(input, data, len);
    crater_set_input(c, pos, input);
}

void crater_set_copy_output(Crater* c, uint64_t pos, const char* data, size_t len) {
    char* output = malloc(len);
    memcpy(output, data, len);
    crater_set_output(c, pos, output);
}
