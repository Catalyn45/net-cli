#include <sys/socket.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <stdio.h>

#include "commands.h"

int ip_callback(struct ip_entry* addr, void* args) {
    (void)args;

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr->addr, ip, sizeof(ip));

    char interface[IF_NAMESIZE];
    printf("%s -> ip: %s\n", if_indextoname(addr->index, interface), ip);

    return 0;
}

int main(int argc, char **argv) {
    (void)argv;
    (void)argc;

    if (get_local_ips(AF_INET, ip_callback, NULL) != 0) {
        return -1;
    }

    return 0;
}
