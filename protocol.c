#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "protocol.h"

int is_able_to_write(int socket_fd, fd_set *write_fds, struct timeval *timeout)
{
    FD_ZERO(write_fds);
    FD_SET(socket_fd, write_fds);
    int ready_fds = select(socket_fd + 1, NULL, write_fds, NULL, timeout);
    if (ready_fds > 0 && FD_ISSET(socket_fd, write_fds))
        return 1;
    return 0;
}

int is_able_to_read(int socket_fd, fd_set *read_fds, struct timeval *timeout)
{
    FD_ZERO(read_fds);
    FD_SET(socket_fd, read_fds);
    int ready_fds = select(socket_fd + 1, read_fds, NULL, NULL, timeout);
    if (ready_fds > 0 && FD_ISSET(socket_fd, read_fds))
        return 1;
    return 0;
}

void create_package(PACKAGE *package, PACKAGE_TYPE type, short sequence, char *data)
{
    bzero(package, sizeof(*package));
    package->init_marker = INIT_MARKER;
    package->type = type;
    package->sequence = sequence;
    package->size = strlen(data);
    printf("LENGTH: %d\n", package->size);
    strcpy(package->data, data);
}

void send_text_message(int socket_fd, char *message)
{
    int client_disconnected = 0;

    fd_set write_fds, read_fds;

    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    int ready_fds;
    int message_length = strlen(message);
    int remaining_length = message_length;
    int sequence = 0;
    PACKAGE package, response;
    char message_slice[MAX_DATA_SIZE];
    int message_type = TEXT;

    while (!client_disconnected)
    {
        if (is_able_to_write(socket_fd, &write_fds, &timeout))
        {
            create_package(&package, INIT, 0, (char *)&message_type);
            if (write(socket_fd, &package, sizeof(package)) < 0)
            {
                client_disconnected = 1;
                printf("Client disconected!\n");
                continue;
            }
            else
            {
                if (is_able_to_read(socket_fd, &read_fds, &timeout))
                {
                    bzero(&response, sizeof(response));
                    if (read(socket_fd, &response, sizeof(response)) < 0)
                    {
                        client_disconnected = 1;
                        printf("Client disconected!\n");
                        continue;
                    }
                    else
                    {
                        if (response.type == ACK)
                        {
                            break;
                        }
                        continue;
                    }
                }
                else
                {
                    printf("Timeout occured on receving ACK from init, trying again\n");
                    continue;
                }
            }
        }
        else
        {
            printf("Timeout occured on writing init, trying again\n");
            continue;
        }
    }

    while (!client_disconnected)
    {
        if (is_able_to_write(socket_fd, &write_fds, &timeout))
        {
            create_package(&package, TEXT, 0, message);
            if (write(socket_fd, &package, sizeof(package)) < 0)
            {
                client_disconnected = 1;
                printf("Client disconected!\n");
                continue;
            }
            else
            {
                if (is_able_to_read(socket_fd, &read_fds, &timeout))
                {
                    bzero(&response, sizeof(response));
                    if (read(socket_fd, &response, sizeof(response)) < 0)
                    {
                        client_disconnected = 1;
                        printf("Client disconected!\n");
                        continue;
                    }
                    else
                    {
                        if (response.type == ACK)
                        {
                            break;
                        }
                        continue;
                    }
                }
                else
                {
                    printf("Timeout occured on receving ACK from MESSAGE, trying again\n");
                    continue;
                }
            }
        }
        else
        {
            printf("Timeout occured on writing message, trying again\n");
            continue;
        }
    }

    while (!client_disconnected)
    {
        if (is_able_to_write(socket_fd, &write_fds, &timeout))
        {
            create_package(&package, END, sequence, "");
            if (write(socket_fd, &package, sizeof(package)) < 0)
            {
                client_disconnected = 1;
                printf("Client disconected!\n");
                continue;
            }
            else
            {
                if (is_able_to_read(socket_fd, &read_fds, &timeout))
                {
                    bzero(&response, sizeof(response));
                    if (read(socket_fd, &response, sizeof(response)) < 0)
                    {
                        client_disconnected = 1;
                        printf("Client disconected!\n");
                        continue;
                    }
                    else
                    {
                        if (response.type == ACK)
                        {
                            break;
                        }
                        continue;
                    }
                }
                else
                {
                    printf("Timeout occured on receving ACK from END, trying again\n");
                    continue;
                }
            }
        }
        else
        {
            printf("Timeout occured on writing END, trying again\n");
            continue;
        }
    }

    // Send init of transmission package
    // bzero(&response, sizeof(response));
    // while (response.type != ACK)
    // {
    //     FD_ZERO(&write_fds);
    //     FD_SET(socket_fd, &write_fds);
    //     ready_fds = select(socket_fd + 1, NULL, &write_fds, NULL, &timeout);
    //     if(ready_fds > 0 && FD_ISSET(socket_fd, &write_fds)){
    //         create_package(&package, INIT, 0, (char *)&message_type);
    //         write(socket_fd, &package, sizeof(package));
    //     }

    //     FD_ZERO(&read_fds);
    //     FD_SET(socket_fd, &read_fds);
    //     ready_fds = select(socket_fd + 1, &read_fds, NULL, NULL, &timeout);
    //     if(ready_fds > 0 && FD_ISSET(socket_fd, &read_fds)){
    //         read(socket_fd, &response, sizeof(response));
    //     }
    // }

    // while (remaining_length > 0)
    // {
    //     bzero(message_slice, MAX_DATA_SIZE);
    //     int actual_sequence = sequence % MAX_SEQUENCE;
    //     int message_displacement = sequence * MAX_DATA_SIZE;
    //     int current_length = message_displacement + MAX_DATA_SIZE > message_length
    //                              ? message_length - message_displacement
    //                              : MAX_DATA_SIZE;

    //     strncpy(message_slice, message + message_displacement, current_length);

    //     FD_ZERO(&write_fds);
    //     FD_SET(socket_fd, &write_fds);
    //     ready_fds = select(socket_fd + 1, NULL, &write_fds, NULL, &timeout);
    //     if(ready_fds > 0 && FD_ISSET(socket_fd, &write_fds)){
    //         create_package(&package, TEXT, actual_sequence, message_slice);
    //         write(socket_fd, &package, sizeof(package));
    //     }

    //     printf("CHUNK %d: %s\n", sequence, message_slice);

    //     FD_ZERO(&read_fds);
    //     FD_SET(socket_fd, &read_fds);
    //     ready_fds = select(socket_fd + 1, &read_fds, NULL, NULL, &timeout);
    //     if(ready_fds > 0 && FD_ISSET(socket_fd, &read_fds)){
    //         read(socket_fd, &response, sizeof(response));
    //     }

    //     sequence++;
    //     remaining_length -= current_length;
    // };

    // FD_ZERO(&write_fds);
    // FD_SET(socket_fd, &write_fds);
    // ready_fds = select(socket_fd + 1, NULL, &write_fds, NULL, &timeout);
    // if(ready_fds > 0 && FD_ISSET(socket_fd, &write_fds)){
    //     create_package(&package, END, sequence, "");
    //     write(socket_fd, &package, sizeof(package));
    // }
}

// review logic
void get_text_message(int socket_fd)
{
    int client_disconnected = 0;

    fd_set write_fds, read_fds;
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    int ready_fds;

    int write_response;
    PACKAGE package, response;
    char message[MAX_TEXT_MESSAGE_SIZE];

    bzero(message, MAX_TEXT_MESSAGE_SIZE);
    bzero(&package, sizeof(package));
    while (!client_disconnected && package.type != END)
    {
        if (is_able_to_read(socket_fd, &read_fds, &timeout))
        {
            bzero(&package, sizeof(package));
            if (read(socket_fd, &package, sizeof(package)) < 0)
            {
                client_disconnected = 1;
                printf("Client disconnected!\n");
                continue;
            }
            else
            {
                if (package.init_marker != INIT_MARKER)
                    continue;

                if (package.type == TEXT)
                {
                    strcat(message, package.data);
                }

                if (package.type == TEXT || package.type == END)
                {
                    if (is_able_to_write(socket_fd, &write_fds, &timeout))
                    {
                        create_package(&response, ACK, package.sequence, "");
                        if (write(socket_fd, &response, sizeof(response)) < 0)
                        {
                            client_disconnected = 1;
                            printf("Client disconnected!\n");
                        }
                        continue;
                    }
                    else
                    {
                        printf("Timeout occurred when sending ACK\n");
                        continue;
                    }
                }
                // Do not forget to treat other errors
                continue;
            }
        }
        else
        {
            printf("Timeout occurred when receiving TEXT package\n");
            break;
        }
    }

    printf("MESSAGE: %s\n", message);
}

void wait_for_packages(int socket_fd)
{
    int client_disconnected = 0;
    fd_set write_fds, read_fds;
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    int ready_fds;

    PACKAGE package, response;
    int sequence;

    while (1)
    {
        while (!client_disconnected)
        {
            if (is_able_to_read(socket_fd, &read_fds, &timeout))
            {
                bzero(&package, sizeof(package));
                if (read(socket_fd, &package, sizeof(package)) < 0)
                {
                    client_disconnected = 1;
                    printf("Client disconnected!\n");
                    continue;
                }
                else
                {
                    if (package.init_marker == INIT_MARKER && package.type == INIT)
                    {
                        break;
                    }
                    continue;
                }
            }
            else
            {
                // printf("Timeout occurred when receiving INIT package\n");
                continue;
            }
        }

        sequence = package.sequence;

        while (!client_disconnected)
        {
            if (is_able_to_write(socket_fd, &write_fds, &timeout))
            {
                create_package(&response, ACK, 0, (char *)&sequence);
                if (write(socket_fd, &response, sizeof(response)) < 0)
                {
                    client_disconnected = 1;
                    printf("Client disconnected!\n");
                    continue;
                }
                else
                {
                    break;
                }
            }
            else
            {
                // printf("Timeout occurred when sending ACK of INIT package\n");
                continue;
            }
        }

        if (*package.data == TEXT)
        {
            printf("\n------ Begin Client sent you ------\n");
            get_text_message(socket_fd);
            printf("\n------ End Client sent you ------\n");
        }
    }
}