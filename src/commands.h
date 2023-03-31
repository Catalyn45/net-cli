#ifndef COMMANDS_H
#define COMMANDS_H

#include <sys/socket.h>
#include <stdint.h>
#include <stdbool.h>

struct ip_entry {
    struct sockaddr addr;
    uint16_t output_interface;
    int family;
};

typedef int (*ip_callback_t)(struct ip_entry* adr, void* args);

// all the local ips for every interface
int get_local_ips(int family, ip_callback_t callback, void* args);

struct route_entry {
    int family;

    struct sockaddr src_addr;
    struct sockaddr dst_addr;
    struct sockaddr gtw_addr;

    int output_interface;

    size_t prefix_len;

    unsigned char scope;
};

typedef int (*route_callback_t)(struct route_entry* entry, void* args);

// all routes
int get_routes(int family, route_callback_t callback, void* args);

struct link_entry {
    unsigned char mac_address[6];
    int output_interface;
    unsigned int mtu;
    bool up;
    int device_type;
};

typedef int (*link_callback_t)(struct link_entry* entry, void* args);

// all interfaces
int get_links(link_callback_t callback, void* args);

#endif
