#include <stdlib.h>
#include <stdio.h>
#include <argp.h>
#include <string.h>

#include "commands.h"
#include "print_hooks.h"

int main(int argc, char *argv[]) {
    int family = AF_INET;
    const char* command = "routes";

    for (int i = 1; i < argc; ++i) {
        if(argv[i][0] == '-') {
            if(argv[i][1] == '6') {
                family = AF_INET6;
            }
        } else {
            command = argv[i];
        }
    }

    int res = 0;
    if (strcmp(command, "ips") == 0) {
        res = get_local_ips(family, ip_callback, NULL);
    } else if (strcmp(command, "routes") == 0) {
        res = get_routes(family, route_callback, NULL);
    } else if (strcmp(command, "interfaces") == 0) {

    }

    if (res != 0) {
        puts("error");
        return -1;
    }

    return 0;
}
