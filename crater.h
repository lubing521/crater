#ifndef CRATER_H
#define CRATER_H

#include <stddef.h>
#include <stdint.h>
#include "actors.h"

// Ring buffer element
typedef struct {
    char* input;
    char* output;
} Entry;

// Core ring buffer
typedef struct {
    Entry* buffer;
    uint64_t len;
    Producer* producer;
    Consumer** consumers;
    ConsumerGroup** groups;
} Crater;

Crater* crater_alloc(uint64_t len);
void crater_destroy(Crater* c);
const char* crater_get_input(Crater* c, uint64_t pos);
const char* crater_get_output(Crater* c, uint64_t pos);
void crater_set_input(Crater* c, uint64_t pos, char* data);
void crater_set_output(Crater* c, uint64_t pos, char* data);
void crater_set_copy_input(Crater* c, uint64_t pos, const char* data, size_t len);
void crater_set_copy_output(Crater* c, uint64_t pos, const char* data, size_t len);

#endif /* CRATER_H */
