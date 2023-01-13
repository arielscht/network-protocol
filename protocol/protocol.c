#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include "protocol.h"
#include "utils.h"

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

int await_ack(int socket_fd, int sequence, PACKAGE *response, fd_set *read_fds, struct timeval *timeout)
{
    if (is_able_to_read(socket_fd, read_fds, timeout))
    {
        bzero(response, sizeof(*response));
        if (read(socket_fd, response, sizeof(response)) < 0)
        {
            printf("Client disconnected!\n");
            return -1;
        }
        else
        {
            if (response->type == ACK && response->sequence == sequence)
            {
                return 1;
            }
            return 0;
        }
    }
    else
    {
        printf("Timeout occurred on receiving ACK from MESSAGE, trying again\n");
        return 0;
    }
}

int send_ack(int socket_fd, PACKAGE *package, fd_set *write_fds, struct timeval *timeout)
{
    PACKAGE response;
    bzero(&response, sizeof(response));
    int client_disconnected = 0;
    while (!client_disconnected)
    {
        if (is_able_to_write(socket_fd, write_fds, timeout))
        {
            create_package(&response, ACK, package->sequence, "");
            if (write(socket_fd, &response, sizeof(response)) < 0)
            {
                client_disconnected = 1;
                fprintf(stderr, "Client disconnected!\n");
            }
            else
            {
                return 1;
            }
        }
        else
        {
            printf("Timeout occurred when sending ACK package\n");
            continue;
        }
    }

    return -1;
}

void send_text_message(int socket_fd, char *message)
{
    int await_ack_status;

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
                fprintf(stderr, "Client disconnected!\n");
            }
            else
            {
                await_ack_status = await_ack(socket_fd, 0, &response, &read_fds, &timeout);
                if (await_ack_status == -1)
                    client_disconnected = 1;
                else if (await_ack_status)
                    break;
            }
        }
        else
            fprintf(stderr, "Timeout occurred on writing init, trying again\n");
    }

    while (!client_disconnected && remaining_length > 0)
    {
        bzero(message_slice, MAX_DATA_SIZE);
        int actual_sequence = sequence % MAX_SEQUENCE;
        int message_displacement = sequence * MAX_DATA_SIZE;
        int current_length = message_displacement + MAX_DATA_SIZE > message_length
                                 ? message_length - message_displacement
                                 : MAX_DATA_SIZE;

        strncpy(message_slice, message + message_displacement, current_length);

        printf("CHUNK %d: %s\n", sequence, message_slice);

        while (!client_disconnected)
        {
            if (is_able_to_write(socket_fd, &write_fds, &timeout))
            {
                create_package(&package, TEXT, actual_sequence, message_slice);
                if (write(socket_fd, &package, sizeof(package)) < 0)
                {
                    client_disconnected = 1;
                    fprintf(stderr, "Client disconnected\n");
                }
                else
                {
                    await_ack_status = await_ack(socket_fd, actual_sequence, &response, &read_fds, &timeout);
                    if (await_ack_status == -1)
                        client_disconnected = 1;
                    else if (await_ack_status)
                        break;
                }
            }
            else
                fprintf(stderr, "Timeout occurred on writing message of sequence %d, trying again\n", sequence);
        }

        printf("CHUNK SENT AND RECEIVED.\n");

        sequence++;
        remaining_length -= current_length;
    };

    while (!client_disconnected)
    {
        if (is_able_to_write(socket_fd, &write_fds, &timeout))
        {
            int end_sequence = sequence % MAX_SEQUENCE;
            create_package(&package, END, end_sequence, (char *)&message_type);
            if (write(socket_fd, &package, sizeof(package)) < 0)
            {
                client_disconnected = 1;
                fprintf(stderr, "Client disconnected!\n");
            }
            else
            {
                await_ack_status = await_ack(socket_fd, end_sequence, &response, &read_fds, &timeout);
                if (await_ack_status == -1)
                    client_disconnected = 1;
                else if (await_ack_status)
                    break;
            }
        }
        else
            fprintf(stderr, "Timeout occurred on writing END, trying again\n");
    }
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
    PACKAGE package;
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
                fprintf(stderr, "Client disconnected!\n");
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
                    send_ack(socket_fd, &package, &write_fds, &timeout);
                }
                // Do not forget to treat other errors
                continue;
            }
        }
        else
        {
            fprintf(stderr, "Timeout occurred when receiving TEXT package\n");
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

    PACKAGE package;

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
                    fprintf(stderr, "Client disconnected!\n");
                }
                else
                {
                    if (package.init_marker == INIT_MARKER && package.type == INIT)
                    {
                        break;
                    }
                }
            }
        }

        send_ack(socket_fd, &package, &write_fds, &timeout);

        if (*package.data == TEXT)
        {
            printf("\n------ Begin Client sent you ------\n");
            get_text_message(socket_fd);
            printf("\n------ End Client sent you ------\n");
        }
    }
}