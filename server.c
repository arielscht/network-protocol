#include <stdio.h>
#include <unistd.h>
#include "protocol/protocol.h"
#include "protocol/utils.h"
#include "socket.h"

int main()
{
    char *interface_name = "enp2s0f1";
    int socket = 0;
    socket = create_raw_socket(interface_name);

    wait_for_packages(socket);
    return 0;
}