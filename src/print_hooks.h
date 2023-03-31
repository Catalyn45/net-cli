#ifndef PRINT_HOOKS_H
#define PRINT_HOOKS_H

#include "commands.h"

int ip_callback(struct ip_entry* addr, void* args);
int route_callback(struct route_entry* route, void* args);
int link_callback(struct link_entry* entry, void* args);

#endif
