#include <stdio.h>
#include <unistd.h>
#include "protocol.h"
#include "socket.h"

int main()
{
    char *interface_name = "lo";
    int socket = 0;
    PACKAGE response;

    socket = create_raw_socket(interface_name);

    printf("SOCKET: %d\n", socket);

    while (1)
    {
        if (read(socket, &response, sizeof(response)) < 0)
            fprintf(stderr, "Error reading package.");

        printf("PACKAGE TYPE: %d\n", response.type);
    }

    return 0;
}