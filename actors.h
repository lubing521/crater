#ifndef ACTORS_H
#define ACTORS_H

#include <pthread.h>
#include <stdint.h>
#include <sys/types.h>
#include <arpa/inet.h>

typedef struct {
    struct in_addr host;
    uint16_t port;
} Addr;

typedef struct {
    // Configuration
    Addr addr;

    // State
    pthread_t thread;
    pthread_attr_t thread_attr;
    // Socket descriptor
    int sock;
} Context;

typedef struct {
    uint64_t slot;
    Context* c;
} Producer;

typedef struct {
    uint64_t slot;
    uint64_t stride;
    Context* c;
} Consumer;

typedef struct {
    ssize_t id;
    Consumer* members;
} ConsumerGroup;

void context_init(Context* c);
void context_destroy(Context* c);

#endif /* ACTORS_H */
