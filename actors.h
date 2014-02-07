#ifndef ACTORS_H
#define ACTORS_H

#include <stdbool.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/types.h>
#include <arpa/inet.h>

typedef struct {
    char* buf;
    size_t len;
    size_t max;
} Buffer;

typedef struct {
    // State
    pthread_t thread;
    pthread_attr_t thread_attr;
    // Socket descriptor
    int client;
} Context;

typedef struct {
    uint64_t slot;
    Context* c;
} Producer;

typedef struct {
    uint64_t slot;
    uint64_t stride;
    Context* c;
    bool can_write;
} Consumer;

typedef struct {
    size_t len;
    size_t max;
    Consumer** i;
} Consumers;

typedef struct {
    ssize_t id;
    Consumer* i;
} ConsumerGroup;

typedef struct {
    size_t len;
    size_t max;
    ConsumerGroup** i;
} ConsumerGroups;

void buffer_alloc(Buffer* b, size_t n);
void buffer_free(Buffer* b);

void consumer_groups_destroy(ConsumerGroups* g);
void consumers_destroy(Consumers* c);
void producer_destroy(Producer* p);

Context* context_spawn(int client);
int context_destroy(Context* c);

#endif /* ACTORS_H */
