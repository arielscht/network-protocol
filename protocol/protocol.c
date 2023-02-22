#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <fcntl.h>
#include <libgen.h>
#include <time.h>
#include "protocol.h"
#include "utils.h"
#include "../interface.h"

void send_text_message(int socket_fd, char *message)
{
    int await_ack_status;

    int client_disconnected = 0;

    fd_set write_fds, read_fds;

    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_IN_SECONDS;

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

    time_t message_time = time(NULL);
    struct tm local_time = *localtime(&message_time);
    fd_set write_fds, read_fds;
    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_IN_SECONDS;

    PACKAGE package;
    char message[MAX_TEXT_MESSAGE_SIZE];
    int cur_sequence = 0;

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

                if (package.type == TEXT && package.sequence == cur_sequence)
                {
                    strncat(message, package.data, package.size);
                    if (cur_sequence == MAX_SEQUENCE - 1)
                        cur_sequence = 0;
                    else
                        cur_sequence += 1;
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

    printf("[%02d/%02d/%02d-%02d:%02d:%02d]: %s\n",
           local_time.tm_mday,
           local_time.tm_mon + 1,
           local_time.tm_year + 1900,
           local_time.tm_hour,
           local_time.tm_min,
           local_time.tm_sec,
           message);
}

void get_media(int socket_fd)
{
    int client_disconnected = 0;
    int crc_check, not_duplicated;

    fd_set write_fds, read_fds;
    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_IN_SECONDS;

    char *filename;
    int packages_size, package_index, window_index, i, j, start_index, end_index;
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
                crc_check = 0;
                if (cur_package.init_marker != INIT_MARKER || (cur_package.type != MEDIA && cur_package.type != END))
                    continue;

                if (cur_package.type == MEDIA)
                {
                    // printf("Before check duplicated ; index: %d; curr package sequence: %d\n", package_index, cur_package.sequence);
                    int window_is_odd = window_index % 2;
                    int lower_bound = window_is_odd ? WINDOW_SIZE : 0;
                    int upper_bound = window_is_odd ? WINDOW_SIZE * 2 - 1 : WINDOW_SIZE - 1;
                    not_duplicated = last_packages[cur_package.sequence % WINDOW_SIZE] != cur_package.sequence && cur_package.sequence >= lower_bound && cur_package.sequence <= upper_bound;

                    if (not_duplicated)
                    {
                        crc_check = check_crc(&cur_package);
                        if (crc_check)
                        {
                            last_packages[cur_package.sequence % WINDOW_SIZE] = cur_package.sequence;
                            // printf("Not duplicated ; index: %d\n", package_index);
                            packages[package_index] = cur_package;
                            // printf("Package sequence: %d\n", packages[package_index].sequence);
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
                        // printf("The package of sequence %d is duplicated", cur_package.sequence);
                    }
                }
                else
                {
                    // printf("Inside end ; index: %d\n", package_index);
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
        }
        else
        {
            fprintf(stderr, "Timeout occurred when receiving MEDIA package\n");
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
    char escape = ESCAPE;
    char vlan1 = VLAN_PROTOCOL_ONE;
    char vlan2 = VLAN_PROTOCOL_TWO;
    int is_vlan_byte;

    for (i = 0; i < packages_qnt; i++)
    {
        for (j = 0; j < packages[i].size; j++)
        {

            int is_escape = 0;
            if (packages[i].data[j] == escape)
                is_escape = 1;
            if (is_escape)
            {
                is_vlan_byte = 0;
                if (packages[i].data[j - 1] == vlan1 || packages[i].data[j - 1] == vlan2)
                    is_vlan_byte = 1;

                if (is_vlan_byte)
                    continue;
            }

            fwrite(&packages[i].data[j], 1, 1, file);
        }
    }

    fclose(file);
}

void *wait_for_packages(void *config_param)
{
    THREAD_PARAM *config = (THREAD_PARAM *)config_param;
    int client_disconnected = 0;
    fd_set write_fds, read_fds;
    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_IN_SECONDS;

    PACKAGE package;

    while (1)
    {
        while (!client_disconnected)
        {
            if (config->locked == 0 && is_able_to_read(config->socket_fd, &read_fds, &timeout))
            {
                bzero(&package, sizeof(package));
                if (read(config->socket_fd, &package, sizeof(package)) < 0)
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

        send_ack(config->socket_fd, &package, &write_fds, &timeout);

        if (*package.data == TEXT)
        {
            config->locked = 1;
            get_text_message(config->socket_fd);
            config->locked = 0;
        }
        else if (*package.data == MEDIA)
        {
            printf("\n------ MEDIA - Begin Client sent you ------\n");
            config->locked = 1;
            get_media(config->socket_fd);
            config->locked = 0;
            printf("\n------ MEDIA - End Client sent you ------\n");
        }
    }

    return NULL;
}

void send_file(int socket_fd, char *filepath, long int file_size)
{
    // Timeout and connection control
    int client_disconnected = 0;
    fd_set write_fds, read_fds;
    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_IN_SECONDS;

    // Data control
    FILE *file;
    PACKAGE *packages, response;
    char message_type = MEDIA;
    char buffer[MAX_DATA_SIZE], current_byte;
    int packages_size, package_index, package_qnt, is_vlan_byte, i;

    // Counters
    long int all_bytes = 0;
    long int vlan_bytes = 0;
    int bytes_count = 0;

    bzero(buffer, MAX_DATA_SIZE);
    bzero(&current_byte, sizeof(current_byte));

    packages_size = 5;
    package_index = 0;
    packages = calloc(packages_size, sizeof(PACKAGE));

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
        // Realloc the packages array after reaching its maximum size
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

        // Check if it is VLAN byte
        is_vlan_byte = 0;
        if (current_byte == vlan1 || current_byte == vlan2)
            is_vlan_byte = 1;

        // Creates a new package when the buffer is FULL
        if (bytes_count == MAX_DATA_SIZE)
        {
            create_package(&packages[package_index], MEDIA, package_index % MAX_SEQUENCE, buffer, bytes_count);
            bzero(buffer, MAX_DATA_SIZE);
            package_index++;
            bytes_count = 0;
        }

        // Creates a new package when the last byte is VLAN
        if (bytes_count == MAX_DATA_SIZE - 1 && is_vlan_byte)
        {
            create_package(&packages[package_index], MEDIA, package_index % MAX_SEQUENCE, buffer, bytes_count);
            bzero(buffer, MAX_DATA_SIZE);
            package_index++;
            bytes_count = 0;
        }

        // Append the new byte to the buffer
        memcpy(buffer + bytes_count, &current_byte, sizeof(current_byte));
        bytes_count++;
        all_bytes++;

        // Add escape byte after VLAN byte
        if (is_vlan_byte)
        {
            memcpy(buffer + bytes_count, &escape, sizeof(escape));
            bytes_count++;
            vlan_bytes++;
        }

        // Reads a new byte
        bytes_read = fread(&current_byte, 1, 1, file);
        if (all_bytes % 1000 == 0 || all_bytes >= file_size)
            show_progress(all_bytes, file_size, "Reading file");
    }

    // Create the last package with the remaining buffer
    if (bytes_count > 0)
    {
        create_package(&packages[package_index], MEDIA, package_index % MAX_SEQUENCE, buffer, bytes_count);
        package_index++;
    }

    fclose(file);

    // Count and print bytes
    // printf("ALL BYTES: %ld\n", all_bytes);
    // printf("VLAN BYTES: %ld\n", vlan_bytes);
    // long int packages_sum = 0;
    // for (int i = 0; i < package_index; i++)
    // {
    //     packages_sum += packages[i].size;
    // }
    // printf("PACKAGES: %ld\n", packages_sum);
    // printf("PACKAGES QUANTITY: %d", package_index);

    // Send INIT package
    char init_message[20];
    long int total_bytes = all_bytes + vlan_bytes;
    bzero(init_message, 20);
    memcpy(init_message, &message_type, 1);
    memcpy(&init_message[2], &total_bytes, sizeof(total_bytes));
    send_control_package(socket_fd, INIT, (char *)init_message, sizeof(message_type) + sizeof(file_size) + 1);

    package_qnt = package_index;
    // printf("INIT ACK RECEIVED SUCCESS\n");
    // printf("There are %d packages to be sent\n", package_qnt);

    int window_start = 0, window_end = 0, ack_received_qnt = 0;
    int update_window = 1;

    int *ack_received = calloc(WINDOW_SIZE, sizeof(int));
    if (!ack_received)
    {
        exit(-1);
    }
    bzero(ack_received, sizeof(int) * WINDOW_SIZE);

    int *expected_acks = calloc(WINDOW_SIZE, sizeof(int));
    if (!expected_acks)
    {
        exit(-1);
    }

    while (!client_disconnected)
    {
        // Update the window
        if (update_window)
        {
            window_start = window_end;
            if (window_start >= package_qnt)
                break;
            window_end = package_qnt > window_end + WINDOW_SIZE ? window_end + WINDOW_SIZE : package_qnt;
            ack_received_qnt = window_end - window_start;
            bzero(ack_received, sizeof(int) * WINDOW_SIZE);
            for (i = 0; i < ack_received_qnt; i++)
                expected_acks[i] = packages[i + window_start].sequence;
            update_window = 0;
            show_progress(window_end, package_qnt, "Sending file");
        }

        // printf("window start %d ; Window end: %d\n", window_start, window_end);

        // Send packages from the current window which did not receive an ACK yet
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
                    // else
                    // printf("Package %d was sent\n", i);
                }
                else
                {
                    // fprintf(stderr, "Timeout occurred on writing package %d, trying again\n", i);
                    continue;
                }
            }
            i++;
        }

        // printf("Waiting acks! %d\n", client_disconnected);

        while (!client_disconnected)
        {
            if (is_able_to_read(socket_fd, &read_fds, &timeout))
            {
                bzero(&response, sizeof(PACKAGE));
                if (read(socket_fd, &response, sizeof(response)) < 0)
                {
                    printf("Client disconnected!\n");
                }
                else
                {
                    // Remover esses prints
                    // printf("Expected acks: ");
                    // printf("RECEIVED QTD: %d", ack_received_qnt);
                    // for (i = 0; i < ack_received_qnt; i++)
                    // {
                    //     if (expected_acks[i] == -1)
                    //         continue;
                    //     printf("%d ", expected_acks[i]);
                    // }

                    // printf("\n");

                    // Ignores packages that are not ACK
                    if (response.type != ACK)
                        continue;

                    // Recognizes that the package has been acknowledged
                    int index = response.sequence % WINDOW_SIZE;
                    if (response.sequence == expected_acks[index])
                    {
                        expected_acks[index] = -1;
                        ack_received[index] = 1;
                    }

                    // Checks if all acks were received
                    int all_acks_received = 1;
                    for (i = 0; i < ack_received_qnt && all_acks_received; i++)
                        if (!ack_received[i % ack_received_qnt])
                            all_acks_received = 0;

                    if (all_acks_received)
                    {
                        update_window = 1;
                        // printf("All acks received\n");
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
    }

    // Send END package
    char *filename = basename(filepath);
    send_control_package(socket_fd, END, filename, strlen(filename));
}