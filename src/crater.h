#ifndef CRATER_H
#define CRATER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "actors.h"
#include "messages.h"

// Ring buffer element
typedef struct {
    Buffer input;
    Buffer output;
} Entry;

typedef struct {
    bool expect_transformer;
    bool expect_producer;
    size_t expect_consumers;
    bool have_transformer;
    bool have_producer;
    size_t have_consumers;
} CraterConfig;

// Core ring buffer
typedef struct Crater {
    CraterConfig config;
    uint64_t len;
    Entry* buffer;
    Actor vacuum;
    // Producer writes to input and does not read
    Actor producer;
    // Transformer is a consumer that reads input and writes to output
    Actor transformer;
    // Consumers read from either input or output
    Actors consumers;
    ActorGroups groups;
    // Config
    Contexts contexts;
    size_t n_contexts;
} Crater;

Crater* crater_alloc(uint64_t len, size_t n_contexts, size_t n_consumers);
void crater_destroy(Crater* c);
Buffer crater_get_input(Crater* c, uint64_t pos);
Buffer crater_get_output(Crater* c, uint64_t pos);
void crater_set_input(Crater* c, uint64_t pos, char* data, size_t len, size_t max);
void crater_set_output(Crater* c, uint64_t pos, char* data, size_t len, size_t max);
void crater_set_copy_input(Crater* c, uint64_t pos, const char* data, size_t len);
void crater_set_copy_output(Crater* c, uint64_t pos, const char* data, size_t len);

int crater_create_context(Crater* crater, int client);
void crater_start(Crater* crater);
int crater_add_context(Crater* c, Context* ctx, ConfigureMessage m);
int crater_undo_add_context(Crater* c, ConfigureMessage m);

#endif /* CRATER_H */
