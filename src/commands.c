#include "commands.h"
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>


static int netlink_route(struct sockaddr_nl* out_sa) {
    // create the netlink socket with the NETLINK_ROUTE protocol
    int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (fd < 0)
        return -1;

    // create the netlink sockaddr
    *out_sa = (struct sockaddr_nl) {
        .nl_family = AF_NETLINK
    };

    // bind the file descriptor to the address
    int res = bind(fd, (const struct sockaddr*)out_sa, sizeof(*out_sa));
    if (res != 0) {
        close(fd);
        return -1;
    }

    return fd;
}


int get_local_ips(int family, ip_callback_t callback, void* args) {
    struct sockaddr_nl sa;
    int fd = netlink_route(&sa);
    if (fd == -1)
        return -1;

    // calculate the total buffer length
    const size_t buffer_length = NLMSG_ALIGN(NLMSG_LENGTH(sizeof(struct ifaddrmsg)));
    unsigned char buffer[buffer_length];

    // create the header of the message
    struct nlmsghdr* nlh = (void*)buffer;
    *nlh = (struct nlmsghdr){
        .nlmsg_len = buffer_length,
        .nlmsg_type = RTM_GETADDR,
        .nlmsg_flags = NLM_F_REQUEST | NLM_F_ROOT
    };

    // create the payload of the message
    struct ifaddrmsg* ifa = NLMSG_DATA(nlh);
    *ifa = (struct ifaddrmsg){
        .ifa_family = family
    };

    // create the iovec structure containing the buffer and length
    struct iovec iov = {
        .iov_base = buffer,
        .iov_len = buffer_length
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
    if (sent < 0)
        goto close_fd;

    iov.iov_len = 0;

    // peek the data to see how many bytes are there to read
    ssize_t received = recvmsg(fd, &msg, MSG_PEEK | MSG_TRUNC);
    if (received < 0)
        goto close_fd;

    size_t len = (size_t)received;

    // allocate the needed memory
    unsigned char* recv_buffer = malloc(len);
    if (!recv_buffer)
        goto close_fd;

    iov.iov_base = (void*)recv_buffer;
    iov.iov_len = len;

    // receive the actual messages
    received = recvmsg(fd, &msg, 0);
    if (received < 0)
        goto free_buf;

    len = (size_t)received;

    // iterate from every message available
    for (nlh = (void*)recv_buffer; NLMSG_OK(nlh, len); nlh = NLMSG_NEXT(nlh, len)) {
        struct ifaddrmsg* ifa = (void*)NLMSG_DATA(nlh);

        size_t rta_len = IFA_PAYLOAD(nlh);

        // iterate from every attribute of the message
        for (struct rtattr* rta = (void*)IFA_RTA(ifa); RTA_OK(rta, rta_len); rta = RTA_NEXT(rta, rta_len)) {
            // if the attribute is an address, create the structure
            // and call the callback provided by the user
            if (rta->rta_type == IFA_ADDRESS) {
                struct ip_entry entry = {
                    .addr = *((struct sockaddr*)RTA_DATA(rta)),
                    .index = ifa->ifa_index,
                    .family = family
                };

                callback(&entry, args);
            }
        }
    }

    free(recv_buffer);

    close(fd);

    return 0;

free_buf:
    free(recv_buffer);

close_fd:
    close(fd);

    return -1;
}
