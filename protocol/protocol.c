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
            create_package(&response, ACK, package->sequence, "", 0);
            if (write(socket_fd, &response, sizeof(response)) < 0)
            {
                client_disconnected = 1;
                fprintf(stderr, "Client disconnected!\n");
            }
            else
            {
                printf("ACK SENT %d\n", package->sequence);
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

int send_nack(int socket_fd, PACKAGE *package, fd_set *write_fds, struct timeval *timeout)
{
    PACKAGE response;
    bzero(&response, sizeof(response));
    int client_disconnected = 0;
    while (!client_disconnected)
    {
        if (is_able_to_write(socket_fd, write_fds, timeout))
        {
            create_package(&response, NACK, package->sequence, "", 0);
            if (write(socket_fd, &response, sizeof(response)) < 0)
            {
                client_disconnected = 1;
                fprintf(stderr, "Client disconnected!\n");
            }
            else
            {
                printf("NACK SENT %d\n", package->sequence);
                return 1;
            }
        }
        else
        {
            printf("Timeout occurred when sending NACK package\n");
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
            create_package(&package, INIT, 0, (char *)&message_type, sizeof(message_type));
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
                create_package(&package, TEXT, actual_sequence, message_slice, strlen(message_slice));
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
            create_package(&package, END, end_sequence, (char *)&message_type, sizeof(message_type));
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
    int packages_size, package_index, window_index, i, j, k, l, start_index, end_index;
    PACKAGE *packages, cur_package;
    int last_packages[WINDOW_SIZE];
    for (i = 0; i < WINDOW_SIZE; i++)
        last_packages[i] = -1;

    packages_size = 5;
    package_index = 0;
    window_index = 0;

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
                        printf("Before check duplicated ; index: %d; curr package sequence: %d\n", package_index, cur_package.sequence);
                        int window_is_odd = window_index % 2;
                        int lower_bound = window_is_odd ? 8 : 0;
                        int upper_bound = window_is_odd ? 15 : 7;

                        if (last_packages[cur_package.sequence % WINDOW_SIZE] != cur_package.sequence && cur_package.sequence >= lower_bound && cur_package.sequence <= upper_bound)
                        {
                            if (check_crc(&cur_package))
                            {
                                last_packages[cur_package.sequence % WINDOW_SIZE] = cur_package.sequence;
                                printf("Not duplicated ; index: %d\n", package_index);
                                packages[package_index] = cur_package;
                                printf("Package sequence: %d\n", packages[package_index].sequence);
                                package_index++;
                                if (package_index % WINDOW_SIZE == 0)
                                {
                                    window_index++;
                                    for (i = 0; i < WINDOW_SIZE; i++)
                                        last_packages[i] = -1;
                                }
                            }
                        }
                        else
                        {
                            printf("The package of sequence %d is duplicated", cur_package.sequence);
                        }
                    }
                    else
                    {
                        printf("Inside end ; index: %d\n", package_index);
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
            continue;
        }
    }

    int packages_qnt = package_index;
    start_index = 0;
    end_index = packages_qnt > WINDOW_SIZE ? WINDOW_SIZE - 1 : packages_qnt - 1;
    while (start_index < packages_qnt)
    {
        sort_packages(packages, start_index, end_index);
        start_index = end_index + 1;
        end_index = packages_qnt > (end_index + 1) + WINDOW_SIZE ? (end_index + 1) + WINDOW_SIZE - 1 : packages_qnt - 1;
    }

    FILE *file = fopen(filename, "wb");
    if (!file)
    {
        // handle error
        exit(-1);
    }

    int sorted = 1;
    for (i = 0; i < ceil((double)packages_qnt / WINDOW_SIZE) && sorted; i++)
    {
        int end = (packages_qnt > i * WINDOW_SIZE + WINDOW_SIZE ? i * WINDOW_SIZE + WINDOW_SIZE : packages_qnt);

        for (j = i * WINDOW_SIZE; j < end - 1 && sorted; j++)
            for (k = j + 1; k < end && sorted; k++)
                if (packages[k].sequence < packages[j].sequence)
                    sorted = 0;
    }

    if (sorted)
        printf("Sorted correctly!\n");
    else
        printf("Sorted incorrectly!\n");

    char escape = ESCAPE;
    char vlan1 = VLAN_PROTOCOL_ONE;
    char vlan2 = VLAN_PROTOCOL_TWO;
    int is_vlan_byte;

    int all_escapes_set = 1;
    i = 0;
    j = 0;
    for (; i < packages_qnt && all_escapes_set;)
    {
        // printf("i: %d\n", i);
        // printf("package size: %d\n", packages[i].size);
        for (; j < packages[i].size && all_escapes_set;)
        {
            is_vlan_byte = 0;
            if (packages[i].data[j] == vlan1)
                is_vlan_byte = 1;
            else if (packages[i].data[j] == vlan2)
                is_vlan_byte = 1;

            if (is_vlan_byte && j == packages[i].size - 1)
            {
                printf("WRONG!!!!!");
            }

            if (is_vlan_byte)
            {
                int package_index_to_search;
                int data_index_to_search;

                package_index_to_search = i;
                data_index_to_search = j + 1;
                j += 2; // jump supposed escape byte
                if (j >= packages[i].size)
                {
                    i++;
                    j = 0;
                }

                int is_escape = 0;
                if (packages[package_index_to_search].data[data_index_to_search] == escape)
                    is_escape = 1;

                if (!is_escape)
                    all_escapes_set = 0;
            }
            else
            {
                j++;
                if (j >= packages[i].size)
                {
                    i++;
                    j = 0;
                }
            }
        }
    }

    if (all_escapes_set)
        printf("All escapes were set\n");
    else
        printf("All escapes were not set");

    for (i = 0; i < packages_qnt; i++)
    {
        for (j = 0; j < packages[i].size; j++)
        {

            int is_escape = 0;
            if (packages[i].data[j] == escape)
                is_escape = 1;
            if (is_escape)
            {
                int package_index_to_search = i;
                int data_index_to_search = j - 1;

                is_vlan_byte = 0;
                if (packages[package_index_to_search].data[data_index_to_search] == vlan1)
                    is_vlan_byte = 1;
                else if (packages[package_index_to_search].data[data_index_to_search] == vlan2)
                    is_vlan_byte = 1;

                if (is_vlan_byte)
                    continue;
            }

            fwrite(&packages[i].data[j], 1, 1, file);
        }
    }

    fclose(file);
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
    FILE *file;
    char buffer[MAX_DATA_SIZE], current_byte;
    int packages_size, package_index, package_qnt, set_next_byte_as_escape, is_vlan_byte, bytes_count, i, j, k;
    PACKAGE *packages, init_package, end_package, response;
    long int all_bytes = 0;
    bzero(buffer, MAX_DATA_SIZE);
    bzero(&current_byte, sizeof(current_byte));
    packages_size = 5;
    packages = calloc(packages_size, sizeof(PACKAGE));
    package_index = 0;

    if (!packages)
    {
        // handle error
        exit(-1);
    }

    file = fopen(filepath, "rb");
    if (!file)
    {
        // handle error
        exit(-1);
    }

    char escape = ESCAPE;
    char vlan1 = VLAN_PROTOCOL_ONE;
    char vlan2 = VLAN_PROTOCOL_TWO;

    bytes_count = 0;
    int bytes_read = fread(&current_byte, 1, 1, file);
    while (bytes_read != 0)
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

        is_vlan_byte = 0;
        if (current_byte == vlan1)
            is_vlan_byte = 1;
        else if (current_byte == vlan2)
            is_vlan_byte = 1;

        if (bytes_count == MAX_DATA_SIZE - 1)
        {
            if (is_vlan_byte)
            {
                create_package(&packages[package_index], MEDIA, package_index % MAX_SEQUENCE, buffer, bytes_count);
                bzero(buffer, MAX_DATA_SIZE);
                package_index++;
                bytes_count = 0;

                memcpy(buffer, &current_byte, sizeof(current_byte));
                bytes_count++;
                all_bytes++;

                memcpy(buffer + bytes_count, &escape, sizeof(escape));
                bytes_count++;
            }
            else
            {
                memcpy(buffer + bytes_count, &current_byte, sizeof(current_byte));
                bytes_count++;
                all_bytes++;

                create_package(&packages[package_index], MEDIA, package_index % MAX_SEQUENCE, buffer, bytes_count);
                bzero(buffer, MAX_DATA_SIZE);
                package_index++;
                bytes_count = 0;
            }
        }
        else
        {
            memcpy(buffer + bytes_count, &current_byte, sizeof(current_byte));
            bytes_count++;
            all_bytes++;

            if (is_vlan_byte)
            {
                memcpy(buffer + bytes_count, &escape, sizeof(escape));
                bytes_count++;

                if (bytes_count == MAX_DATA_SIZE)
                {
                    create_package(&packages[package_index], MEDIA, package_index % MAX_SEQUENCE, buffer, bytes_count);
                    bzero(buffer, MAX_DATA_SIZE);
                    package_index++;
                    bytes_count = 0;
                }
            }
        }

        bzero(&current_byte, sizeof(current_byte));
        bytes_read = fread(&current_byte, 1, 1, file);
    }
    if (bytes_count > 0)
    {
        create_package(&packages[package_index], MEDIA, package_index % MAX_SEQUENCE, buffer, bytes_count);
        package_index++;
    }

    fclose(file);
    package_qnt = package_index;

    int all_escapes_set = 1;
    i = 0;
    j = 0;
    for (; i < package_qnt && all_escapes_set;)
    {
        // printf("i: %d\n", i);
        // printf("package size: %d\n", packages[i].size);
        for (; j < packages[i].size && all_escapes_set;)
        {
            // printf("j: %d\n", j);
            is_vlan_byte = 0;
            if (current_byte == vlan1)
                is_vlan_byte = 1;
            else if (current_byte == vlan2)
                is_vlan_byte = 1;

            if (is_vlan_byte)
            {
                int package_index_to_search;
                int data_index_to_search;

                package_index_to_search = i;
                data_index_to_search = j + 1;
                j += 2; // jump supposed escape byte
                if (j >= packages[i].size)
                {
                    i++;
                    j = 0;
                }

                int is_escape = 0;
                if (packages[package_index_to_search].data[data_index_to_search] == escape)
                    is_escape = 1;

                if (!is_escape)
                    all_escapes_set = 0;
            }
            else
            {
                j++;
                if (j >= packages[i].size)
                {
                    i++;
                    j = 0;
                }
            }
        }
    }

    if (all_escapes_set)
        printf("All escapes were set\n");
    else
        printf("All escapes were not set");

    // return;

    while (!client_disconnected)
    {
        if (is_able_to_write(socket_fd, &write_fds, &timeout))
        {
            create_package(&init_package, INIT, 0, (char *)&message_type, sizeof(message_type));
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

    printf("INIT ACK RECEIVED SUCCESS\n");
    printf("There are %d packages to be sent\n", package_qnt);

    int window_start = 0;
    int window_end = package_qnt > WINDOW_SIZE ? WINDOW_SIZE : package_qnt;
    int ack_received_qnt = window_end - window_start;
    int *ack_received = calloc(ack_received_qnt, sizeof(int));
    if (!ack_received)
    {
        // handle error
        exit(-1);
    }
    bzero(ack_received, sizeof(*ack_received));

    int *expected_acks = calloc(ack_received_qnt, sizeof(int));
    if (!expected_acks)
    {
        // handle error
        exit(-1);
    }
    for (i = window_start; i < window_end; i++)
        expected_acks[i % ack_received_qnt] = i % MAX_SEQUENCE;

    while (!client_disconnected && window_start < package_qnt)
    {
        printf("window start %d ; Window end: %d\n", window_start, window_end);

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
                    else
                        printf("Package %d was sent\n", i);
                }
                else
                {
                    fprintf(stderr, "Timeout occurred on writing package %d, trying again\n", i);
                    continue;
                }
            }

            i++;
        }

        printf("Waiting acks!\n");

        while (!client_disconnected)
        {
            if (is_able_to_read(socket_fd, &read_fds, &timeout))
            {
                bzero(&response, sizeof(response));
                if (read(socket_fd, &response, sizeof(response)) < 0)
                {
                    printf("Client disconnected!\n");
                }
                else
                {
                    printf("Expected acks: ");
                    for (i = 0; i < ack_received_qnt; i++)
                    {
                        if (expected_acks[i] == -1)
                            continue;
                        printf("%d ", expected_acks[i]);
                    }

                    printf("\n");

                    if (response.type == ACK)
                        for (i = 0; i < ack_received_qnt; i++)
                        {
                            if (expected_acks[i] == -1)
                                continue;
                            if (response.sequence == expected_acks[i])
                            {
                                printf("Received ack of the sequence %d\n", response.sequence);
                                expected_acks[i] = -1;
                                ack_received[i] = 1;
                            }
                        }

                    int all_acks_received = 1;
                    for (i = 0; i < ack_received_qnt && all_acks_received; i++)
                        if (!ack_received[i % ack_received_qnt])
                            all_acks_received = 0;

                    if (all_acks_received)
                    {
                        printf("All acks received\n");
                        window_start = window_end;
                        window_end = package_qnt > window_end + WINDOW_SIZE ? window_end + WINDOW_SIZE : package_qnt;
                        free(ack_received);
                        ack_received_qnt = window_end - window_start;
                        ack_received = calloc(ack_received_qnt, sizeof(int));
                        if (!ack_received)
                        {
                            // handle error
                            exit(-1);
                        }
                        bzero(ack_received, sizeof(*ack_received));

                        free(expected_acks);
                        expected_acks = calloc(ack_received_qnt, sizeof(int));
                        if (!expected_acks)
                        {
                            // handle error
                            exit(-1);
                        }
                        for (i = window_start; i < window_end; i++)
                            expected_acks[i % ack_received_qnt] = i % MAX_SEQUENCE;

                        break;
                    }
                }
            }
            else
            {
                // printf("Timeout occurred on receiving ACK from MESSAGE, trying again\n");
                break;
            }
        }

        // int expected_ack = window_start;
        // while (!client_disconnected)
        // {
        //     await_ack_status = await_ack(socket_fd, expected_ack % MAX_SEQUENCE, &response, &read_fds, &timeout);
        //     if (await_ack_status == -1)
        //         client_disconnected = 1;
        //     else if (await_ack_status)
        //     {
        //         ack_received[expected_ack % ack_received_qnt] = 1;

        //         int all_acks_received = 1;
        //         for (i = window_start; i < window_end && all_acks_received; i++)
        //         {
        //             if (!ack_received[i % ack_received_qnt])
        //             {
        //                 all_acks_received = 0;
        //             }
        //         }

        //         if (all_acks_received)
        //         {
        //             printf("All acks received\n");
        //             window_start = window_end;
        //             window_end = package_qnt > window_end + WINDOW_SIZE ? window_end + WINDOW_SIZE : package_qnt;
        //             break;
        //         }

        //         expected_ack++;
        //     }
        // }
    }

    while (!client_disconnected)
    {
        if (is_able_to_write(socket_fd, &write_fds, &timeout))
        {
            create_package(&end_package, END, 0, basename(filepath), strlen(basename(filepath)));
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