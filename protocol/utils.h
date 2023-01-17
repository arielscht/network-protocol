#ifndef __UTILS__
#define __UTILS__

#include "protocol.h"

void get_binary_string(char *data, char *result, int data_size);

void get_crc_encoded_data(char *data, char *result, int data_size);

int check_crc(PACKAGE *package);

void generate_crc(PACKAGE *package);

void create_package(PACKAGE *package, PACKAGE_TYPE type, short sequence, char *data);

long int file_size(char *filepath);

#endif