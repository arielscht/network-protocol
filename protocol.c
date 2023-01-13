#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include "protocol.h"

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

    printf("BINARY DATA: %s\n", binary_data);
    printf("BINARY CRC: %s\n", binary_crc);
    printf("CRC LEN: %ld\n", strlen(binary_crc));

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
    char data[MAX_DATA_SIZE], current_char;

    char_bits_qnt = sizeof(char) * BITS_IN_BYTE_QNT;
    poly_size = strlen(CRC_POLYNOMIAL);
    data_size = package->size;
    strcpy(data, package->data);

    int current_index = 0;
    int binary_data_size = data_size * char_bits_qnt;
    char binary_data[binary_data_size + 1];
    bzero(binary_data, binary_data_size + 1);

    for (int i = 0; i < data_size; i++)
    {
        current_char = data[i];
        for (int j = char_bits_qnt - 1; j >= 0; j--)
        {
            binary_data[current_index] = (current_char & 1) ? '1' : '0';
            current_char = current_char >> 1;
            current_index++;
        }
    }

    int encoded_data_size = binary_data_size + (poly_size - 1);
    char encoded_data[encoded_data_size + 1];
    bzero(encoded_data, encoded_data_size + 1);

    strcat(encoded_data, binary_data);
    for (int i = 0; i < poly_size - 1; i++)
        strcat(encoded_data, "0");

    int i = 0;
    while (i < encoded_data_size - poly_size + 1)
    {
        for (int j = 0; j < poly_size; j++)
        {
            if (encoded_data[i + j] == CRC_POLYNOMIAL[j])
                encoded_data[i + j] = '0';
            else
                encoded_data[i + j] = '1';
        }

        while (i < encoded_data_size && encoded_data[i] != '1')
            i++;
    }

    int crc_code_size = poly_size - 1;
    char crc_code[crc_code_size + 1];
    bzero(crc_code, crc_code_size + 1);
    strncat(crc_code, encoded_data + encoded_data_size - poly_size + 1, poly_size - 1);

    char crc;
    bzero(&crc, sizeof(char));

    for (int i = BITS_IN_BYTE_QNT - 1; i >= 0; i--)
    {
        if (crc_code[BITS_IN_BYTE_QNT - i - 1] == '1')
            crc = crc | (1 << (BITS_IN_BYTE_QNT - i - 1));
    }

    package->crc = crc;
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