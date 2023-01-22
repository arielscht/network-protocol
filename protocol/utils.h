#ifndef __UTILS__
#define __UTILS__

#include "protocol.h"

void get_binary_string(char *data, char *result, int data_size);

void get_crc_encoded_data(char *data, char *result, int data_size);

int check_crc(PACKAGE *package);

void generate_crc(PACKAGE *package);

void create_package(PACKAGE *package, PACKAGE_TYPE type, short sequence, char *data, int size);

long int size_of_file(char *filepath);

int check_duplicated(PACKAGE *packages, PACKAGE *cur_package, int start_index, int end_index);

void sort_packages(PACKAGE *packages, int start_index, int end_index);

#endif