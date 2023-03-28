#ifndef COMMANDS_H
#define COMMANDS_H

#include <sys/socket.h>
#include <stdint.h>

struct ip_entry {
    struct sockaddr addr;
    uint16_t index;
    int family;
};

typedef int (*ip_callback_t)(struct ip_entry* adr, void* args);

// all the local ips for every interface
int get_local_ips(int family, ip_callback_t callback, void* args);

#endif
