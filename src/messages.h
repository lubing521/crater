#ifndef MESSAGES_H
#define MESSAGES_H

#include <stdint.h>
#include <stdlib.h>

#define MSGMAXLEN (0x00FFFFFF - sizeof(uint64_t)) /* 16.7M */

typedef struct {
    char* buf;
    size_t len;
    size_t max;
} Buffer;

void buffer_alloc(Buffer* b, size_t n);
void buffer_free(Buffer* b);
int buffer_resize(Buffer* b, size_t max);
int buffer_grow(Buffer* b);
void buffer_reset(Buffer* b);
int buffer_write(Buffer* b, char* data, size_t len);

typedef enum {
    MSG_GET_DATA,
    MSG_GIVE_DATA,
    MSG_CONFIGURE,
    MSG_UNKNOWN = 0xFF
} MessageType;

typedef enum {
    GDMAX_BYTES,
    GDMAX_ELEMS,
    GDMAX_UNKNOWN = 0xFF
} GetDataMaxType;

typedef enum {
    SLOT_INPUT,
    SLOT_OUTPUT,
    SLOT_UNKNOWN = 0xFF
} SlotDestination;

typedef enum {
    ACTOR_PRODUCER,
    ACTOR_CONSUMER,
    ACTOR_TRANSFORMER,
    ACTOR_UNKNOWN = 0xFF
} ActorType;

typedef struct {
    uint64_t len;
    char* buf;
} SlotData;

typedef struct {
    SlotDestination io;
    GetDataMaxType max_type;
    uint64_t max;
} GetDataMsg;

typedef struct {
    SlotDestination io;
    uint64_t n;
    SlotData* data;
} GiveDataMsg;

typedef struct {
    ActorType actor_type;
    // TODO -- actor group -- use string name (easiest for config)?
} ConfigureMessage;

size_t parse_message_header(const char* buf, size_t len, uint64_t* mlen, MessageType* mtype);
size_t parse_message_get_data(const char* buf, size_t len, GetDataMsg* m);
size_t parse_message_give_data(const char* buf, size_t len, GiveDataMsg* m);
size_t parse_message_configure(const char* buf, size_t len, ConfigureMessage* m);

void get_data_msg_destroy(GetDataMsg* m);
void give_data_msg_destroy(GiveDataMsg* m);

#endif /* MESSAGES_H */
