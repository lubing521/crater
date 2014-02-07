#ifndef SERVER_H
#define SERVER_H

#include <arpa/inet.h>
#include <sys/types.h>
#include <stdint.h>

#include "crater.h"

typedef struct {
    struct in_addr host;
    uint16_t port;
} Addr;

int server_run(Addr addr, Crater* crater);
int addr_from_hostname(const char* hostname, Addr* addr);

#endif /* SERVER_H */
