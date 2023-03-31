#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <linux/rtnetlink.h>
#include <stdio.h>

#include "print_hooks.h"

int ip_callback(struct ip_entry* addr, void* args) {
    (void)args;

    // always INET6 because the length is bigger
    char ip[INET6_ADDRSTRLEN];
    inet_ntop(addr->family, &addr->addr, ip, sizeof(ip));

    char interface[IF_NAMESIZE];
    printf("%s -> ip: %s\n", if_indextoname(addr->index, interface), ip);

    return 0;
}


int route_callback(struct route_entry* route, void* args) {
    (void)args;

    char dst_ip[INET6_ADDRSTRLEN];
    inet_ntop(route->family, &route->dst_addr, dst_ip, sizeof(dst_ip));

    char src_ip[INET6_ADDRSTRLEN];
    inet_ntop(route->family, &route->src_addr, src_ip, sizeof(src_ip));

    char gateway_ip[INET6_ADDRSTRLEN];
    inet_ntop(route->family, &route->gtw_addr, gateway_ip, sizeof(gateway_ip));

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
    printf("dev %s scope %s src %s gateway %s\n", if_indextoname(route->output_interface, interface), scope, src_ip, gateway_ip);

    return 0;
}

