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

static char* serialize_configure_msg(ConfigureMessage m, size_t* len) {
    *len = sizeof(uint64_t) + sizeof(uint8_t) + sizeof(uint8_t);
    char* buf = malloc(*len);
    size_t r = 0;
    *(uint64_t*)buf = (uint64_t)(sizeof(uint8_t));
    r += sizeof(uint64_t);
    *(uint8_t*)&buf[r] = (uint8_t)MSG_CONFIGURE;
    r += sizeof(uint8_t);
    *(uint8_t*)&buf[r] = (uint8_t)m.actor_type;
    r += sizeof(uint8_t);
    return buf;
}

int client_send_config(Client* c) {
    ConfigureMessage m;
    m.actor_type = c->type;
    size_t len = 0;
    char* buf = serialize_configure_msg(m, &len);
    if (buf == NULL) {
        return -1;
    }
    size_t wrote = 0;
    printf("Sending %lu bytes as configuration\n", len);
    while (wrote < len) {
        ssize_t n = send(c->client, &buf[wrote], len, 0);
        if (n < 0) {
            perror("send failed: ");
            free(buf);
            return -1;
        }
        wrote += (size_t)n;
    }
    free(buf);
    return 0;
}

int client_give_data(Client* c) {
    return -1;
}

int client_get_data(Client* c) {
    return -1;
}

int client_produce(Client* c) {
    return -1;
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

    sleep(100);

    switch (c.type) {
    case ACTOR_PRODUCER:
        client_produce(&c);
        break;
    case ACTOR_TRANSFORMER:
        client_transform(&c);
        break;
    case ACTOR_CONSUMER:
        client_consume(&c);
        break;
    default:
        assert(false);
        return 1;
    }

    return 0;
}
