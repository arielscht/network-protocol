#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <fcntl.h>
#include "protocol.h"
#include "utils.h"
#include <libgen.h>

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
                create_package(&package, TEXT, sequence, message_slice);
                if (write(socket_fd, &package, sizeof(package)) < 0)
                {
                    client_disconnected = 1;
                    fprintf(stderr, "Client disconnected\n");
                }
                else
                {
                    await_ack_status = await_ack(socket_fd, sequence, &response, &read_fds, &timeout);
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
            int end_sequence = sequence + 1;
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

void get_media(int socket_fd)
{
    int client_disconnected = 0;

    fd_set write_fds, read_fds;
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    int ready_fds;

    char *filename;
    int packages_size, package_index, i;
    PACKAGE *packages, cur_package;

    packages_size = 5;
    package_index = 0;
    packages = calloc(packages_size, sizeof(PACKAGE));
    if (!packages)
    {
        // handle error
        exit(-1);
    }

    bzero(&cur_package, sizeof(cur_package));

    while (!client_disconnected && cur_package.type != END)
    {
        if (package_index == packages_size - 1)
        {
            packages_size += 5;
            packages = realloc(packages, packages_size * sizeof(PACKAGE));
            if (!packages)
            {
                // handle error
                exit(-1);
            }
        }

        if (is_able_to_read(socket_fd, &read_fds, &timeout))
        {
            bzero(&cur_package, sizeof(cur_package));
            if (read(socket_fd, &cur_package, sizeof(cur_package)) < 0)
            {
                client_disconnected = 1;
                fprintf(stderr, "Client disconnected!\n");
                continue;
            }
            else
            {
                if (cur_package.init_marker != INIT_MARKER)
                    continue;

                if (cur_package.type == MEDIA || cur_package.type == END)
                {
                    if (cur_package.type == MEDIA)
                    {
                        if (!check_duplicated(packages, &cur_package, package_index + 1))
                            packages[package_index] = cur_package;

                        package_index++;
                    }
                    else
                    {
                        filename = calloc(cur_package.size, sizeof(char));
                        if (!filename)
                        {
                            // handle error
                            exit(-1);
                        }

                        strcpy(filename, cur_package.data);
                    }

                    send_ack(socket_fd, &cur_package, &write_fds, &timeout);
                }
                // Do not forget to treat other errors
                continue;
            }
        }
        else
        {
            fprintf(stderr, "Timeout occurred when receiving MEDIA package\n");
            break;
        }
    }

    int packages_qnt = package_index + 1;
    sort_packages(packages, packages_qnt);

    int file = open(filename, O_CREAT | O_WRONLY);
    if (file < 0)
    {
        // handle error
        exit(-1);
    }

    for (i = 0; i < packages_qnt; i++)
        write(file, packages[i].data, packages[i].size);
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
            printf("\n------ TEXT - Begin Client sent you ------\n");
            get_text_message(socket_fd);
            printf("\n------ TEXT - End Client sent you ------\n");
        }
        else if (*package.data == MEDIA)
        {
            printf("\n------ MEDIA - Begin Client sent you ------\n");
            get_media(socket_fd);
            printf("\n------ MEDIA - End Client sent you ------\n");
        }
    }
}

void send_file(int socket_fd, char *filepath)
{
    int await_ack_status;

    int client_disconnected = 0;
    fd_set write_fds, read_fds;
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    int ready_fds;
    int message_type = MEDIA;

    char buffer[MAX_DATA_SIZE];
    int file, packages_size, package_index, package_qnt, i;
    PACKAGE *packages, init_package, end_package, response;

    bzero(buffer, MAX_DATA_SIZE);
    packages_size = 5;
    packages = calloc(packages_size, sizeof(PACKAGE));
    package_index = 0;

    if (!packages)
    {
        // handle error
        exit(-1);
    }

    file = open(filepath, O_RDONLY);
    if (file < 0)
    {
        // handle error
        exit(-1);
    }

    while (read(file, buffer, MAX_DATA_SIZE - 1) > 0)
    {
        if (package_index == packages_size - 1)
        {
            packages_size += 5;
            packages = realloc(packages, packages_size * sizeof(PACKAGE));
            if (!packages)
            {
                // handle error
                exit(-1);
            }
        }

        buffer[MAX_DATA_SIZE] = '\0';
        create_package(&packages[package_index], MEDIA, package_index, buffer);
        bzero(buffer, MAX_DATA_SIZE);
        package_index++;
    }

    package_qnt = package_index + 1;

    while (!client_disconnected)
    {
        if (is_able_to_write(socket_fd, &write_fds, &timeout))
        {
            create_package(&init_package, INIT, 0, (char *)&message_type);
            if (write(socket_fd, &init_package, sizeof(init_package)) < 0)
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

    int window_start = 0;
    int window_end = package_qnt > WINDOW_SIZE ? WINDOW_SIZE : package_qnt;

    while (!client_disconnected && window_start < package_qnt)
    {
        int ack_received_qnt = window_end - window_start + 1;
        int ack_received[ack_received_qnt];

        bzero(ack_received, ack_received_qnt);

        i = window_start;
        while (i < window_end)
        {
            if (!ack_received[i % ack_received_qnt])
            {
                if (is_able_to_write(socket_fd, &write_fds, &timeout))
                {
                    if (write(socket_fd, &packages[i], sizeof(packages[i])) < 0)
                    {
                        client_disconnected = 1;
                        fprintf(stderr, "Client disconnected!\n");
                    }
                }
                else
                {
                    fprintf(stderr, "Timeout occurred on writing package %d, trying again\n", i);
                    continue;
                }
            }

            i++;
        }

        int expected_ack = window_start;
        while (!client_disconnected)
        {
            await_ack_status = await_ack(socket_fd, expected_ack, &response, &read_fds, &timeout);
            if (await_ack_status == -1)
                client_disconnected = 1;
            else if (await_ack_status)
            {
                ack_received[expected_ack % ack_received_qnt] = 1;

                int all_acks_received = 1;
                for (i = window_start; i < window_end && all_acks_received; i++)
                {
                    if (!ack_received[i % ack_received_qnt])
                    {
                        all_acks_received = 0;
                    }
                }

                if (all_acks_received)
                {
                    window_start = window_end;
                    window_end = package_qnt > window_end + WINDOW_SIZE ? window_end + WINDOW_SIZE : package_qnt;
                    break;
                }

                expected_ack++;
            }
        }
    }

    while (!client_disconnected)
    {
        if (is_able_to_write(socket_fd, &write_fds, &timeout))
        {
            create_package(&end_package, END, 0, basename(filepath));
            if (write(socket_fd, &end_package, sizeof(end_package)) < 0)
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
            fprintf(stderr, "Timeout occurred on writing END, trying again\n");
    }
}