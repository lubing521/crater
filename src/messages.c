#include "messages.h"

#include <string.h>
#include <stdio.h>

// Initializes a Buffer
void buffer_alloc(Buffer* b, size_t n) {
    b->len = 0;
    b->max = n;
    b->buf = malloc(n * sizeof(*(b->buf)));
}

// Releases the Buffer's resources
void buffer_free(Buffer* b) {
    b->len = 0;
    b->max = 0;
    free(b->buf);
}

// Reset the buffer indices
void buffer_reset(Buffer* b) {
    b->len = 0;
}

// Resizes the Buffer to max
int buffer_resize(Buffer* b, size_t max) {
    if (max == b->max) {
        return 0;
    }
    char* buf = realloc(b->buf, max * sizeof(*buf));
    if (buf == NULL) {
        return -1;
    }
    if (max < b->len) {
        b->len = max;
    }
    b->buf = buf;
    b->max = max;
    return 0;
}

// Doubles the buffer length
int buffer_grow(Buffer* b) {
    return buffer_resize(b, b->max * 2);
}

// Writes bytes to the buffer
int buffer_write(Buffer* b, char* data, size_t len) {
    while (b->max - b->len < len) {
        if (buffer_grow(b) < 0) {
            return -1;
        }
    }
    strncpy(&b->buf[len], data, len);
    b->len += len;
    return 0;
}

// Removes the first up_to bytes and moves the remainder to the beginning
// of the buffer
// TODO -- use a circular buffer for reads
void buffer_strip(Buffer* b, size_t up_to) {
    if (up_to >= b->len) {
        b->len = 0;
        return;
    }
    size_t new_len = b->len - up_to;
    memmove(b->buf, &b->buf[up_to], new_len);
    b->len = new_len;
    return;
}


// Parses a uint64_t.  If buf is not long enough, will return 0, otherwise
// number of bytes read.
static size_t parse_uint64(const char* buf, size_t len, uint64_t* i) {
    if (len < sizeof(uint64_t)) {
        return 0;
    }
    *i = *(uint64_t*)buf;
    return sizeof(uint64_t);
}

// Parses a uint8_t.  If buf is not long enough, will return 0, otherwise
// number of bytes read.
static size_t parse_uint8(const char* buf, size_t len, uint8_t* i) {
    if (len < sizeof(uint8_t)) {
        return 0;
    }
    *i = *(uint8_t*)buf;
    return sizeof(uint8_t);
}

static SlotDestination map_slot_dest(uint8_t io) {
    switch (io) {
    case SLOT_INPUT:
        return SLOT_INPUT;
    case SLOT_OUTPUT:
        return SLOT_OUTPUT;
    default:
        return SLOT_UNKNOWN;
    }
}

static GetDataMaxType map_gdmax_type(uint8_t mt) {
    switch (mt) {
    case GDMAX_BYTES:
        return GDMAX_BYTES;
    case GDMAX_ELEMS:
        return GDMAX_ELEMS;
    default:
        return GDMAX_UNKNOWN;
    }
}

// Returns number of bytes read.  If its 0, no message was parsed
size_t parse_message_header(const char* buf, size_t len, uint64_t* mlen,
                            MessageType* mtype) {
    *mtype = MSG_UNKNOWN;
    *mlen = 0;
    size_t r = 0;
    // Read message length prefix
    size_t n = parse_uint64(buf, len, mlen);
    if (n == 0) {
        return 0;
    }
    r += n;

    // Read msg id
    uint8_t msg_id = 0;
    n = parse_uint8(&buf[r], len - r, &msg_id);
    if (n == 0) {
        return 0;
    }
    r += n;
    switch (msg_id) {
    case MSG_GIVE_DATA:
        *mtype = MSG_GIVE_DATA;
        break;
    case MSG_GET_DATA:
        *mtype = MSG_GET_DATA;
        break;
    case MSG_CONFIGURE:
        *mtype = MSG_CONFIGURE;
        break;
    default:
        *mtype = MSG_UNKNOWN;
        break;
    }
    return r;
}

size_t parse_message_get_data(const char* buf, size_t len, GetDataMsg* m) {
    size_t r = 0;
    uint8_t io = 0;
    size_t n = parse_uint8(buf, len, &io);
    if (n == 0) {
        return 0;
    }
    r += n;

    uint8_t max_type = 0;
    n = parse_uint8(&buf[r], len - r, &max_type);
    if (n == 0) {
        return 0;
    }
    r += n;

    uint64_t max = 0;
    n = parse_uint64(&buf[r], len - r, &max);
    if (n == 0) {
        return 0;
    }
    r += n;

    m->io = map_slot_dest(io);
    m->max_type = map_gdmax_type(max_type);
    m->max = max;
    return r;
}

size_t parse_message_give_data(const char* buf, size_t len, GiveDataMsg* m) {
    size_t r = 0;
    uint8_t io = 0;
    size_t n = parse_uint8(buf, len, &io);
    if (n == 0) {
        return 0;
    }
    r += n;

    uint64_t count = 0;
    n = parse_uint64(&buf[r], len - r, &count);
    if (n == 0) {
        return 0;
    }
    r += n;

    SlotData* data = malloc(count * sizeof(*data));
    for (uint64_t i = 0; i < count; i++) {
        uint64_t dlen = 0;
        n = parse_uint64(&buf[r], len - r, &dlen);
        if (n == 0) {
            free(data);
            return 0;
        }
        r += n;
        data[i].len = dlen;

        char* buf = malloc(dlen * sizeof(*buf));
        if (len - r < dlen) {
            free(data);
            return 0;
        }
        strncpy(buf, &buf[r], dlen);
        r += dlen;
        data[i].buf = buf;
    }

    m->io = map_slot_dest(io);
    m->n = count;
    m->data = data;
    return r;
}

size_t parse_message_configure(const char* buf, size_t len, ConfigureMessage* m) {
    printf("Parsing configure message of len %llu\n", (long long unsigned)len);
    uint8_t actor_type = 0;
    size_t n = parse_uint8(buf, len, &actor_type);
    if (n == 0) {
        return 0;
    }
    m->actor_type = actor_type;
    return n;
}

void get_data_msg_destroy(GetDataMsg* m) {
    m->io = SLOT_UNKNOWN;
    m->max_type = GDMAX_UNKNOWN;
    m->max = 0;
}

void give_data_msg_destroy(GiveDataMsg* m) {
    for (uint64_t i = 0; i < m->n; i++) {
        free(m->data[i].buf);
    }
    free(m->data);
    m->data = NULL;
    m->n = 0;
    m->io = SLOT_UNKNOWN;
}
