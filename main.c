#include "crater.h"
#include <stdio.h>

/*
Ring buffer
Producer/Consumer
    -2 IPC sockets
    -Runs in own thread

*/

int main(int argc, char** argv) {
    Crater* c = crater_alloc(100);
    printf("Crater size: %llu\n", (long long unsigned)c->len);
    crater_destroy(c);
}
