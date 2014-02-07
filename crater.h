#ifndef CRATER_H
#define CRATER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "actors.h"

// Ring buffer element-element
typedef struct {
    size_t len;
    char* data;
} Buffer;

// Ring buffer element
typedef struct {
    Buffer input;
    Buffer output;
} Entry;

// Core ring buffer
typedef struct {
    Entry* buffer;
    uint64_t len;
    Producer producer;
    Consumers consumers;
    ConsumerGroups groups;
} Crater;

Crater* crater_alloc(uint64_t len);
void crater_destroy(Crater* c);
Buffer crater_get_input(Crater* c, uint64_t pos);
Buffer crater_get_output(Crater* c, uint64_t pos);
void crater_set_input(Crater* c, uint64_t pos, char* data, size_t len);
void crater_set_output(Crater* c, uint64_t pos, char* data, size_t len);
void crater_set_copy_input(Crater* c, uint64_t pos, const char* data, size_t len);
void crater_set_copy_output(Crater* c, uint64_t pos, const char* data, size_t len);

int crater_create_context(Crater* crater, int client);
bool crater_ready(Crater* crater);
void crater_start(Crater* crater);

#endif /* CRATER_H */
