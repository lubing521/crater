#ifndef ACTORS_H
#define ACTORS_H

#include <stdbool.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "messages.h"

struct Crater;

typedef struct {
    volatile uint64_t slot;
    uint64_t stride;
    ActorType type;
} Actor;

typedef struct {
    size_t len;
    size_t max;
    Actor* i;
} Actors;

typedef struct {
    ssize_t id;
    Actor* i;
} ActorGroup;

typedef struct {
    size_t len;
    size_t max;
    ActorGroup** i;
} ActorGroups;

typedef struct {
    // State
    pthread_t thread;
    pthread_attr_t thread_attr;
    // Socket descriptor
    int client;
    Actor* actor;
    struct Crater* crater;
} Context;

typedef struct {
    Context** contexts;
    size_t len;
    size_t max;
} Contexts;

void actors_alloc(Actors* a, size_t start);
Actor* actors_fetch(Actors* a);
int actors_unfetch(Actors* a);

void contexts_alloc(Contexts* c, size_t start);
int contexts_add(Contexts* c, Context* ctx);
void contexts_destroy(Contexts* c);
Context* contexts_pop(Contexts* c);

void actor_groups_destroy(ActorGroups* g);
void actors_destroy(Actors* c);

Context* context_alloc(int client);
int context_spawn(Context* c);
int context_destroy(Context* c);

int context_process_get_data_msg(Context* c, GetDataMsg m, Buffer* wbuf);
int context_process_give_data_msg(Context* c, GiveDataMsg m);

#endif /* ACTORS_H */
