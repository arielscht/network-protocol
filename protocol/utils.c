#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "protocol.h"
#include "utils.h"

void create_package(PACKAGE *package, PACKAGE_TYPE type, short sequence, char *data, int size)
{
    bzero(package, sizeof(PACKAGE));
    package->init_marker = INIT_MARKER;
    package->type = type;
    package->sequence = sequence;
    package->size = size;
    memcpy(package->data, data, size);
    generate_crc(package);
}

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
        // printf("Timeout occurred on receiving ACK from MESSAGE, trying again\n");
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
                return 1;
            }
        }
        else
        {
            // printf("Timeout occurred when sending ACK package\n");
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
            // printf("Timeout occurred when sending NACK package\n");
            continue;
        }
    }

    return -1;
}

void get_binary_string(char *data, char *result, int data_size)
{
    int current_index = 0;
    char current_char;

    for (int i = 0; i < data_size; i++)
    {
        current_char = data[i];
        for (int j = BITS_IN_BYTE_QNT - 1; j >= 0; j--)
        {
            result[current_index] = (current_char & 1) ? '1' : '0';
            current_char = current_char >> 1;
            current_index++;
        }
    }
}

void get_crc_encoded_data(char *data, char *result, int data_size)
{
    int poly_size = strlen(CRC_POLYNOMIAL);
    int binary_data_size = data_size;
    int encoded_data_size = binary_data_size;
    memcpy(result, data, data_size);

    int i = 0;
    while (i < encoded_data_size - poly_size + 1)
    {
        for (int j = 0; j < poly_size; j++)
        {
            if (result[i + j] == CRC_POLYNOMIAL[j])
                result[i + j] = '0';
            else
                result[i + j] = '1';
        }

        while (i < encoded_data_size && result[i] != '1')
            i++;
    }
};

int check_crc(PACKAGE *package)
{
    int poly_size = strlen(CRC_POLYNOMIAL);
    int binary_data_size = package->size * BITS_IN_BYTE_QNT;
    int crc_size = poly_size - 1;
    int encoded_data_size = binary_data_size + crc_size;
    char binary_data[binary_data_size + 1];
    char binary_crc[crc_size + 1];
    char encoded_data[encoded_data_size + 1];

    binary_data[binary_data_size] = '\0';
    binary_crc[crc_size] = '\0';
    bzero(encoded_data, encoded_data_size + 1);

    get_binary_string(package->data, binary_data, package->size);
    get_binary_string((char *)&package->crc, binary_crc, 1);

    strcat(encoded_data, binary_data);
    strcat(encoded_data, binary_crc);
    get_crc_encoded_data(encoded_data, encoded_data, encoded_data_size);

    for (int i = encoded_data_size - crc_size; i < encoded_data_size; i++)
    {
        if (encoded_data[i] != '0')
            return 0;
    }

    return 1;
}

void generate_crc(PACKAGE *package)
{
    int data_size, char_bits_qnt, poly_size;

    char_bits_qnt = sizeof(char) * BITS_IN_BYTE_QNT;
    poly_size = strlen(CRC_POLYNOMIAL);
    data_size = package->size;

    int binary_data_size = data_size * char_bits_qnt;
    char binary_data[binary_data_size + 1];
    bzero(binary_data, binary_data_size + 1);

    get_binary_string(package->data, binary_data, data_size);

    int encoded_data_size = binary_data_size + (poly_size - 1);
    char encoded_data[encoded_data_size + 1];
    bzero(encoded_data, encoded_data_size + 1);

    strcat(encoded_data, binary_data);
    for (int i = 0; i < poly_size - 1; i++)
        strcat(encoded_data, "0");

    get_crc_encoded_data(encoded_data, encoded_data, encoded_data_size);

    int crc_code_size = poly_size - 1;
    char crc_code[crc_code_size + 1];
    bzero(crc_code, crc_code_size + 1);
    strncat(crc_code, encoded_data + encoded_data_size - poly_size + 1, poly_size - 1);

    char crc;
    bzero(&crc, sizeof(char));

    for (int i = BITS_IN_BYTE_QNT - 1; i >= 0; i--)
        if (crc_code[BITS_IN_BYTE_QNT - i - 1] == '1')
            crc = crc | (1 << (BITS_IN_BYTE_QNT - i - 1));

    package->crc = crc;
}

long int size_of_file(char *filepath)
{
    FILE *fp = fopen(filepath, "r"); // assuming the file exists

    fseek(fp, 0L, SEEK_END);
    long int res = ftell(fp);
    fclose(fp);

    return res;
}

int check_duplicated(PACKAGE *packages, PACKAGE *cur_package, int start_index, int end_index)
{
    int i;

    for (i = start_index; i <= end_index; i++)
        if (packages[i].sequence == cur_package->sequence)
            return 1;

    return 0;
}

void sort_by_sequence(PACKAGE *packages, int start_index, int end_index)
{
    PACKAGE aux;
    int i, j, min_index;

    for (i = start_index; i <= end_index - 1; i++)
    {
        min_index = i;

        for (j = i + 1; j <= end_index; j++)
            if (packages[j].sequence < packages[min_index].sequence)
                min_index = j;

        aux = packages[i];
        packages[i] = packages[min_index];
        packages[min_index] = aux;
    }
}

void sort_packages(PACKAGE *packages, int packages_qnt)
{
    int start_index, end_index;

    start_index = 0;
    end_index = packages_qnt > WINDOW_SIZE ? WINDOW_SIZE - 1 : packages_qnt - 1;
    while (start_index < packages_qnt)
    {
        sort_by_sequence(packages, start_index, end_index);
        start_index = end_index + 1;
        end_index = packages_qnt > (end_index + 1) + WINDOW_SIZE ? (end_index + 1) + WINDOW_SIZE - 1 : packages_qnt - 1;
    }
}

int send_control_package(int socket_fd, PACKAGE_TYPE control_type, char *control_data, int data_size)
{
    PACKAGE init_package, response;

    int client_disconnected = 0;
    int await_ack_status;

    fd_set write_fds, read_fds;
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    while (!client_disconnected)
    {
        if (is_able_to_write(socket_fd, &write_fds, &timeout))
        {
            create_package(&init_package, control_type, 0, control_data, data_size);
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
        // else
        // fprintf(stderr, "Timeout occurred on writing init, trying again\n");
    }

    return client_disconnected;
}