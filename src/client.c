#include <stdio.h>

#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "server.h"

typedef struct {
    int client;
    ActorType type;
} Client;

int client_connect(Client* c, Addr addr) {

    int client = socket(AF_INET, SOCK_STREAM, 0);
    if (client < 0) {
        perror("Failed to make socket: ");
        return -1;
    }

    struct sockaddr_in cin;
    memset(&cin, 0, sizeof(cin));
    cin.sin_family = AF_INET;
    cin.sin_port = htons(0);
    cin.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(client, (struct sockaddr*)&cin, sizeof(cin)) < 0) {
        perror("Failed to bind to socket: ");
        return -1;
    }

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(addr.port);
    sin.sin_addr = addr.host;
    if (connect(client, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        perror("Failed to connect to server: ");
        return -1;
    }

    c->client = client;
    return 0;
}

static size_t write_uint64(uint64_t val, char* buf, size_t len) {
    if (len < sizeof(uint64_t)) {
        return 0;
    }
    *(uint64_t*)buf = val;
    return sizeof(uint64_t);
}

static size_t write_uint8(uint8_t val, char* buf, size_t len) {
    if (len < sizeof(uint8_t)) {
        return 0;
    }
    *(uint8_t*)buf = val;
    return sizeof(uint8_t);
}

static size_t write_bytes(char* data, size_t ndata, char* buf, size_t len) {
    if (len < ndata) {
        return 0;
    }
    memcpy(buf, data, ndata);
    return ndata;
}

static char* serialize_configure_msg(ConfigureMessage m, size_t* buflen) {
    size_t mlen = sizeof(uint8_t);
    size_t blen = mlen + sizeof(uint64_t) + sizeof(uint8_t);
    char* buf = malloc(blen);
    size_t r = 0;
    r += write_uint64(mlen, buf, blen);
    r += write_uint8(MSG_CONFIGURE, &buf[r], blen - r);
    r += write_uint8(m.actor_type, &buf[r], blen - r);
    *buflen = r;
    return buf;
}

static char* serialize_give_data_msg(GiveDataMsg m, size_t* buflen) {
    size_t mlen = sizeof(uint8_t) + sizeof(m.n);
    for (uint64_t i = 0; i < m.n; i++) {
        mlen += sizeof(m.data[i].len) + m.data[i].len;
    }
    size_t blen = mlen + sizeof(uint64_t) + sizeof(uint8_t);
    char* buf = malloc(blen);
    size_t r = 0;
    r += write_uint64(mlen, buf, blen);
    r += write_uint8(MSG_GIVE_DATA, &buf[r], blen - r);
    r += write_uint8(m.io, &buf[r], blen - r);
    r += write_uint64(m.n, &buf[r], blen - r);
    for (uint64_t i = 0; i < m.n; i++) {
        r += write_uint64(m.data[i].len, &buf[r], blen - r);
        r += write_bytes(m.data[i].buf, m.data[i].len, &buf[r], blen - r);
    }
    *buflen = r;
    return buf;
}

int client_sendall(Client* c, char* buf, size_t len) {
    size_t wrote = 0;
    while (wrote < len) {
        ssize_t n = send(c->client, &buf[wrote], len, 0);
        if (n < 0) {
            perror("send failed: ");
            free(buf);
            return -1;
        }
        wrote += (size_t)n;
    }
    return 0;
}

int client_send_config(Client* c) {
    ConfigureMessage m;
    m.actor_type = c->type;
    size_t len = 0;
    char* buf = serialize_configure_msg(m, &len);
    if (buf == NULL) {
        return -1;
    }
    printf("Sending %lu bytes as configuration\n", len);
    int sent = client_sendall(c, buf, len);
    free(buf);
    return sent;
}

static GiveDataMsg create_give_data_msg(const char text[]) {
    size_t tlen = strlen(text);
    GiveDataMsg m;
    m.io = SLOT_INPUT;
    m.n = 10;
    m.data = malloc(m.n * sizeof(SlotData));
    for (uint64_t i = 0; i < m.n; i++) {
        m.data[i].len = tlen;
        char* c = malloc(tlen);
        strncpy(c, text, tlen);
        m.data[i].buf = c;
    }
    return m;
}

int client_give_data(Client* c) {
    const char text[] = "hello world";
    GiveDataMsg m = create_give_data_msg(text);
    size_t blen = 0;
    char* buf = serialize_give_data_msg(m, &blen);
    give_data_msg_destroy(&m);
    if (buf == NULL) {
        return -1;
    }
    int sent = client_sendall(c, buf, blen);
    free(buf);
    return sent;
}

int client_get_data(Client* c) {
    return -1;
}

int client_produce(Client* c) {
    for (;;) {
        printf("Sending some data to server\n");
        client_give_data(c);
        sleep(3);
    }
    return 0;
}

int client_consume(Client* c) {
    return -1;
}

int client_transform(Client* c) {
    return -1;
}


int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: ./crater-client server_addr:port actor_type\n");
        return 0;
    }
    ActorType actor_type = ACTOR_UNKNOWN;
    switch (argv[2][0]) {
    case 'p':
        actor_type = ACTOR_PRODUCER;
        break;
    case 'c':
        actor_type = ACTOR_CONSUMER;
        break;
    case 't':
        actor_type = ACTOR_TRANSFORMER;
        break;
    default:
        printf("Unknown actor type %c.  Options are p, c, t\n", argv[2][0]);
        return 1;
    }
    printf("Client actor type is %d\n", actor_type);

    const char* server = argv[1];
    Addr addr;
    if (addr_from_hostname(server, &addr) < 0) {
        printf("Invalid server: %s\n", server);
        return 1;
    }

    Client c;
    c.type = actor_type;
    if (client_connect(&c, addr) < 0) {
        printf("Failed to connect to %s\n", server);
        return 1;
    }

    sleep(2);

    if (client_send_config(&c) < 0) {
        printf("Failed to send config message to %s\n", server);
        return 1;
    } else {
        printf("Sent configuration\n");
    }

    sleep(2);

    int ret = 0;
    switch (c.type) {
    case ACTOR_PRODUCER:
        ret = client_produce(&c);
        break;
    case ACTOR_TRANSFORMER:
        ret = client_transform(&c);
        break;
    case ACTOR_CONSUMER:
        ret = client_consume(&c);
        break;
    default:
        assert(false);
        return 1;
    }

    if (ret != 0) {
        return 1;
    }
    return 0;
}
