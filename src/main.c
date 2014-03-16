#include <stdio.h>

#include <string.h>

#include "crater.h"
#include "server.h"

/*
Ring buffer
Producer/Consumer
    -2 IPC sockets
    -Runs in own thread

*/

int main(int argc, char** argv) {
    if (argc > 1 && strlen(argv[1]) >= 2 && strncmp(argv[1], "-h", 2) == 0) {
        printf("Usage: ./crater xxx.xx.xx.xxx:yyyy\n");
        return 0;
    }
    Crater* c = crater_alloc(100, 1, 0);
    printf("Crater size: %llu\n", (long long unsigned)c->len);

    const char* hostname = argv[1];
    Addr addr;
    if (argc > 1) {
        if (addr_from_hostname(hostname, &addr) < 0) {
            printf("Invalid host: %s\n", hostname);
            return 1;
        }
        printf("Listening on %s\n", hostname);
    } else {
        memset(&addr, 0, sizeof(addr));
        addr.port = 0;
        addr.host.s_addr = INADDR_ANY;
        printf("Listening on random port\n");
    }
    int ret = server_run(addr, c);
    if (ret == 0) {
        crater_start(c);
    } else {
        printf("Server run failed\n");
    }
    crater_destroy(c);
    return 0;
}
