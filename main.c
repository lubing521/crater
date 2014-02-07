#include <stdio.h>

#include "crater.h"
#include "server.h"

/*
Ring buffer
Producer/Consumer
    -2 IPC sockets
    -Runs in own thread

*/

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: ./crater xxx.xx.xx.xxx:yyyy\n");
        return 0;
    }
    Crater* c = crater_alloc(100);
    printf("Crater size: %llu\n", (long long unsigned)c->len);

    const char* hostname = argv[1];
    Addr addr;
    addr_from_hostname(hostname, &addr);
    printf("Listening on %s\n", hostname);
    int ret = server_run(addr, c);
    if (ret < 0) {
        printf("Listen failed\n");
    }
    crater_destroy(c);
    return 0;
}
