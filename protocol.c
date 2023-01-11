#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "protocol.h"

void create_package(PACKAGE *package, PACKAGE_TYPE type, short sequence, char *data)
{
    bzero(package, sizeof(*package));
    package->init_marker = INIT_MARKER;
    package->type = type;
    package->sequence = sequence;
    package->size = strlen(data);
    strcpy(package->data, data);
}

void send_text_message(int socket_fd, char *message)
{
    int message_length = strlen(message);
    int remaining_length = message_length;
    int sequence = 0;
    PACKAGE package, response;
    char message_slice[MAX_DATA_SIZE];

    // Send init of transmission package
    while (1)
    {
        create_package(&package, INIT, 0, "");
        write(socket_fd, &package, sizeof(package));
        if (read(socket_fd, &response, sizeof(response)) < 0)
        {
            fprintf(stderr, "Error reading init response.\n");
            continue;
        }
        if (response.type == ACK)
        {
            break;
        }
    }

    while (remaining_length > 0)
    {
        bzero(message_slice, MAX_DATA_SIZE);
        int actual_sequence = sequence % MAX_SEQUENCE;
        int message_displacement = sequence * MAX_DATA_SIZE;
        int current_length = message_displacement + MAX_DATA_SIZE > message_length
                                 ? message_length - message_displacement
                                 : MAX_DATA_SIZE;

        strncpy(message_slice, message + message_displacement, current_length);
        create_package(&package, TEXT, actual_sequence, message_slice);
        printf("CHUNK %d: %s\n", sequence, message_slice);

        sequence++;
        remaining_length -= current_length;
    };
}