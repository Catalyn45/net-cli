#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <stdio.h>
#include <errno.h>
#include <linux/if_arp.h>

#include "print_hooks.h"

int ip_callback(struct ip_entry* addr, void* args) {
    (void)args;

    // always INET6 because the length is bigger
    char ip[INET6_ADDRSTRLEN];
    if (inet_ntop(addr->family, &addr->addr, ip, sizeof(ip)) == NULL) {
        fprintf(stderr, "error at converting address to string: %d\n", errno);
        return -1;
    }

    char interface[IF_NAMESIZE];
    if (if_indextoname(addr->output_interface, interface) == NULL) {
        fprintf(stderr, "error at converting interface index to string: %d\n", errno);
        return -1;
    }

    printf("%s -> ip: %s\n", interface, ip);

    return 0;
}


int route_callback(struct route_entry* route, void* args) {
    (void)args;

    char dst_ip[INET6_ADDRSTRLEN];
    if (inet_ntop(route->family, &route->dst_addr, dst_ip, sizeof(dst_ip)) == NULL) {
        fprintf(stderr, "error at converting address to string: %d\n", errno);
        return -1;
    }

    char src_ip[INET6_ADDRSTRLEN];
    if (inet_ntop(route->family, &route->src_addr, src_ip, sizeof(src_ip)) == NULL) {
        fprintf(stderr, "error at converting address to string: %d\n", errno);
        return -1;
    }

    char gateway_ip[INET6_ADDRSTRLEN];
    if(inet_ntop(route->family, &route->gtw_addr, gateway_ip, sizeof(gateway_ip)) == NULL) {
        fprintf(stderr, "error at converting address to string: %d\n", errno);
        return -1;
    }

    printf("%s/%zu ", dst_ip, route->prefix_len);

    const char* scope = "nowhere";

    switch (route->scope)
    {
    case RT_SCOPE_UNIVERSE:
        scope = "universe";
        break;

    case RT_SCOPE_SITE:
        scope = "site";
        break;

    case RT_SCOPE_LINK:
        scope = "link";
        break;

    case RT_SCOPE_HOST:
        scope = "host";
        break;

    default:
        break;
    }

    char interface[IF_NAMESIZE];
    if (if_indextoname(route->output_interface, interface) == NULL) {
        fprintf(stderr, "error at converting interface index to string: %d\n", errno);
        return -1;
    }

    printf("dev %s scope %s src %s gateway %s\n", interface, scope, src_ip, gateway_ip);

    return 0;
}

static void print_mac(unsigned char mac_address[6]) {
    printf("%02x:%02x:%02x:%02x:%02x:%02x ",
        mac_address[0], mac_address[1], mac_address[2],
        mac_address[3], mac_address[4], mac_address[5]
    );
}

int link_callback(struct link_entry* entry, void* args) {
    (void)args;

    char interface[IF_NAMESIZE];
    if_indextoname(entry->output_interface, interface);

    const char* device_type = "unknown";
    switch (entry->device_type)
    {
    case ARPHRD_ETHER:
        device_type = "ether";
        break;

    case ARPHRD_LOOPBACK:
        device_type = "loopback";
        break;

    default:
        break;
    }

    print_mac(entry->mac_address);
    printf("%s state %s mtu %d %s\n",
        interface,
        entry->up ? "UP" : "DOWN",
        entry->mtu,
        device_type
    );

    return 0;
}
