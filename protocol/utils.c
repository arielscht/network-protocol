#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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

void create_package(PACKAGE *package, PACKAGE_TYPE type, short sequence, char *data)
{
    bzero(package, sizeof(*package));
    package->init_marker = INIT_MARKER;
    package->type = type;
    package->sequence = sequence;
    package->size = strlen(data);
    printf("LENGTH: %d\n", package->size);
    strcpy(package->data, data);
    generate_crc(package);
}

long int file_size(char *filepath)
{
    FILE *fp = fopen(filepath, "r"); // assuming the file exists

    fseek(fp, 0L, SEEK_END);
    long int res = ftell(fp);
    fclose(fp);

    return res;
}