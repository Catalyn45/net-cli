#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <linux/if.h>
#include <string.h>
#include <errno.h>

#include "commands.h"

static int netlink_route(struct sockaddr_nl* out_sa) {
    // create the netlink socket with the NETLINK_ROUTE protocol
    int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (fd < 0) {
        fprintf(stderr, "error creating socket: %d\n", errno);
        return -1;
    }

    // create the netlink sockaddr
    *out_sa = (struct sockaddr_nl) {
        .nl_family = AF_NETLINK
    };

    // bind the file descriptor to the address
    int res = bind(fd, (const struct sockaddr*)out_sa, sizeof(*out_sa));
    if (res != 0) {
        fprintf(stderr, "error binding address to netlink socket: %d\n", errno);
        close(fd);
        return -1;
    }

    return fd;
}


static unsigned char* query_netlink(void* buffer, size_t length, size_t* out_length) {
    // create the netlink socket
    struct sockaddr_nl sa;
    int fd = netlink_route(&sa);
    if (fd == -1)
        return NULL;

    // create the iovec structure containing the buffer and length
    struct iovec iov = {
        .iov_base = buffer,
        .iov_len = length
    };

    // create the message header structure
    struct msghdr msg = {
        .msg_name = &sa,
        .msg_namelen = sizeof(sa),
        .msg_iov = &iov,
        .msg_iovlen = 1
    };

    // send the message to kernel
    ssize_t sent = sendmsg(fd, &msg, 0);
    if (sent < 0) {
        fprintf(stderr, "error sending message to netlink: %d\n", errno);
        goto close_fd;
    }

    size_t total_length = 0;
    unsigned char* recv_buffer = NULL;

    while (1) {
        iov.iov_len = 0;

        // peek the data to see how many bytes are there to read
        ssize_t received = recvmsg(fd, &msg, MSG_PEEK | MSG_TRUNC);
        if (received < 0) {
            fprintf(stderr, "error receiving message from netlink: %d\n", errno);
            goto free_buf;
        }

        size_t len = (size_t)received;
        size_t old_len = total_length;
        total_length += len;

        // allocate the needed memory
        unsigned char* new_recv_buffer = realloc(recv_buffer, total_length);
        if (!new_recv_buffer) {
            fprintf(stderr, "memory allocation error\n");
            goto free_buf;
        }

        recv_buffer = new_recv_buffer;

        iov.iov_base = (void*)(recv_buffer + old_len);
        iov.iov_len = len;

        // receive the actual messages
        received = recvmsg(fd, &msg, 0);
        if (received < 0) {
            fprintf(stderr, "error receiving message from netlink: %d\n", errno);
            goto free_buf;
        }

        if (((struct nlmsghdr*)(iov.iov_base))->nlmsg_type == NLMSG_DONE) {
            total_length -= len;
            break;
        }

        if (((struct nlmsghdr*)(iov.iov_base))->nlmsg_type == NLMSG_ERROR) {
            struct nlmsgerr* err = (struct nlmsgerr*)NLMSG_DATA(iov.iov_base);
            fprintf(stderr, "netlink error, code: %d\n", err->error);
            goto free_buf;
        }
    }

    *out_length = total_length;

    return recv_buffer;

free_buf:
    free(recv_buffer);

close_fd:
    close(fd);

    return NULL;
}

#define MESSAGE_SIZE(message_type) \
    NLMSG_ALIGN(NLMSG_LENGTH(sizeof(message_type)))

int get_local_ips(int family, ip_callback_t callback, void* args) {
    // calculate the total buffer length
    const size_t buffer_length = MESSAGE_SIZE(struct ifaddrmsg);
    unsigned char buffer[buffer_length];

    // create the header of the message
    struct nlmsghdr* nlh = (void*)buffer;
    *nlh = (struct nlmsghdr){
        .nlmsg_len = buffer_length,
        .nlmsg_type = RTM_GETADDR,
        .nlmsg_flags = NLM_F_REQUEST | NLM_F_ROOT // Use the NLM_F_ROOT flag to get all the entries
    };

    // create the payload of the message
    struct ifaddrmsg* ifa = NLMSG_DATA(nlh);
    *ifa = (struct ifaddrmsg){
        .ifa_family = family
    };

    // query netlink
    size_t len;
    void* recv_buffer = query_netlink(buffer, buffer_length, &len);
    if (recv_buffer == NULL)
        return -1;

    // iterate from every message available
    for (nlh = (void*)recv_buffer; NLMSG_OK(nlh, len); nlh = NLMSG_NEXT(nlh, len)) {
        if (nlh->nlmsg_type == NLMSG_ERROR) {
            struct nlmsgerr* err = (struct nlmsgerr*)NLMSG_DATA(nlh);
            fprintf(stderr, "netlink error, code: %d\n", err->error);
            goto free_buf;
        }

        // get the pointer to data
        struct ifaddrmsg* ifa = (void*)NLMSG_DATA(nlh);

        // get the payload length
        size_t rta_len = IFA_PAYLOAD(nlh);

        // iterate from every attribute of the message
        for (struct rtattr* rta = (void*)IFA_RTA(ifa); RTA_OK(rta, rta_len); rta = RTA_NEXT(rta, rta_len)) {
            // if the attribute is an address, create the structure
            if (rta->rta_type == IFA_ADDRESS) {
                struct ip_entry entry = {
                    .addr = *((struct sockaddr*)RTA_DATA(rta)),
                    .output_interface = ifa->ifa_index,
                    .family = family
                };

                // call the callback provided by the user
                if (callback(&entry, args) != 0)
                    goto free_buf;
            }
        }
    }

    free(recv_buffer);

    return 0;

free_buf:
    free(recv_buffer);

    return -1;
}


int get_routes(int family, route_callback_t callback, void* args) {
    // calculate the total buffer length
    const size_t buffer_length = MESSAGE_SIZE(struct rtmsg);
    unsigned char buffer[buffer_length];

    // create the header of the message
    struct nlmsghdr* nlh = (void*)buffer;
    *nlh = (struct nlmsghdr){
        .nlmsg_len = buffer_length,
        .nlmsg_type = RTM_GETROUTE,
        .nlmsg_flags = NLM_F_REQUEST | NLM_F_ROOT  // Use the NLM_F_ROOT flag to get all the entries
    };

    // create the payload of the message
    struct rtmsg* rtm = NLMSG_DATA(nlh);
    *rtm = (struct rtmsg){
        .rtm_family = family,
    };

    // query netlink
    size_t len;
    void* recv_buffer = query_netlink(buffer, buffer_length, &len);
    if (recv_buffer == NULL)
        return -1;

    // iterate from every message available
    for (nlh = (void*)recv_buffer; NLMSG_OK(nlh, len); nlh = NLMSG_NEXT(nlh, len)) {
        if (nlh->nlmsg_type == NLMSG_ERROR) {
            struct nlmsgerr* err = (struct nlmsgerr*)NLMSG_DATA(nlh);
            fprintf(stderr, "netlink error, code: %d\n", err->error);
            goto free_buf;
        }
        // get the pointer to data
        struct rtmsg* rtm = (void*)NLMSG_DATA(nlh);

        // get the payload length
        size_t rta_len = RTM_PAYLOAD(nlh);

        // create the entry structure
        struct route_entry entry = {
            .prefix_len = rtm->rtm_dst_len,
            .scope = rtm->rtm_scope,
            .family = rtm->rtm_family
        };

        // iterate from every attribute of the message
        for (struct rtattr* rta = (void*)RTM_RTA(rtm); RTA_OK(rta, rta_len); rta = RTA_NEXT(rta, rta_len)) {
            // poppulate the structure
            if (rta->rta_type == RTA_DST) {
                entry.dst_addr = *((struct sockaddr*)RTA_DATA(rta));
            } else if (rta->rta_type == RTA_PREFSRC) {
                entry.src_addr = *((struct sockaddr*)RTA_DATA(rta));
            } else if (rta->rta_type == RTA_GATEWAY) {
                entry.gtw_addr = *((struct sockaddr*)RTA_DATA(rta));
            } else if (rta->rta_type == RTA_OIF) {
                entry.output_interface = *((int*)RTA_DATA(rta));
            }
        }

        // call the callback provided by the user
        if (callback(&entry, args) != 0)
            goto free_buf;
    }

    free(recv_buffer);

    return 0;

free_buf:
    free(recv_buffer);

    return -1;
}

int get_links(link_callback_t callback, void* args) {
    // calculate the total buffer length
    const size_t buffer_length = MESSAGE_SIZE(struct ifinfomsg);
    unsigned char buffer[buffer_length];

    // create the header of the message
    struct nlmsghdr* nlh = (void*)buffer;
    *nlh = (struct nlmsghdr){
        .nlmsg_len = buffer_length,
        .nlmsg_type = RTM_GETLINK,
        .nlmsg_flags = NLM_F_REQUEST | NLM_F_ROOT  // Use the NLM_F_ROOT flag to get all the entries
    };

    // query netlink
    size_t len;
    void* recv_buffer = query_netlink(buffer, buffer_length, &len);
    if (recv_buffer == NULL)
        return -1;

    // iterate from every message available
    for (nlh = (void*)recv_buffer; NLMSG_OK(nlh, len); nlh = NLMSG_NEXT(nlh, len)) {
        if (nlh->nlmsg_type == NLMSG_ERROR) {
            struct nlmsgerr* err = (struct nlmsgerr*)NLMSG_DATA(nlh);
            fprintf(stderr, "netlink error, code: %d\n", err->error);
            goto free_buf;
        }
        // get the pointer to data
        struct ifinfomsg* ifm = (void*)NLMSG_DATA(nlh);

        // get the payload length
        size_t rta_len = IFLA_PAYLOAD(nlh);

        struct link_entry entry = {
            .output_interface = ifm->ifi_index,
            .device_type = ifm->ifi_type,
            .up = ifm->ifi_flags && IFF_UP
        };

        // iterate from every attribute of the message
        for (struct rtattr* rta = (void*)IFLA_RTA(ifm); RTA_OK(rta, rta_len); rta = RTA_NEXT(rta, rta_len)) {
            if (rta->rta_type == IFLA_MTU) {
                entry.mtu = *((unsigned int*)RTA_DATA(rta));
            } else if (rta->rta_type == IFLA_ADDRESS) {
                memcpy(entry.mac_address, RTA_DATA(rta), sizeof(entry.mac_address));
            }
        }

        // call the callback provided by the user
        if (callback(&entry, args) != 0)
            goto free_buf;
    }

    free(recv_buffer);

    return 0;

free_buf:
    free(recv_buffer);

    return -1;
}
