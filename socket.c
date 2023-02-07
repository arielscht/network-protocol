#include <arpa/inet.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <stdlib.h>
#include <stdio.h>

#include "socket.h"
#include "protocol/protocol.h"

int create_raw_socket(char *interface_name)
{
    // Create socket file without any protocol
    int raw_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

    if (raw_socket == -1)
    {
        fprintf(stderr, "Error creating socket: Please check if you have root permissions!\n");
    }

    int ifindex = if_nametoindex(interface_name);

    struct sockaddr_ll address = {0};
    address.sll_family = AF_PACKET;
    address.sll_protocol = htons(ETH_P_ALL);
    address.sll_ifindex = ifindex;

    // Initialize socket
    if (bind(raw_socket, (struct sockaddr *)&address, sizeof(address)) == -1)
    {
        fprintf(stderr, "Error binding socket\n");
        exit(-1);
    }

    struct packet_mreq mr = {0};
    mr.mr_ifindex = ifindex;
    mr.mr_type = PACKET_MR_PROMISC;

    if (setsockopt(raw_socket, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof(mr)) == -1)
    {
        fprintf(stderr, "Error executing setsockopt: Check if the network interface was correctly specified.\n");
        exit(-1);
    }

    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_IN_SECONDS;
    timeout.tv_usec = 0;

    if (setsockopt(raw_socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
    {
        fprintf(stderr, "Error in set timeout to send\n");
        exit(-1);
    }

    if (setsockopt(raw_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
    {
        fprintf(stderr, "Error in set timeout to receive\n");
        exit(-1);
    }

    return raw_socket;
}