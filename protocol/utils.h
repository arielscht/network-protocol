#ifndef __UTILS__
#define __UTILS__

#include "protocol.h"

int is_able_to_write(int socket_fd, fd_set *write_fds, struct timeval *timeout);

int is_able_to_read(int socket_fd, fd_set *read_fds, struct timeval *timeout);

int await_ack(int socket_fd, int sequence, PACKAGE *response, fd_set *read_fds, struct timeval *timeout);

int send_ack(int socket_fd, PACKAGE *package, fd_set *write_fds, struct timeval *timeout);

int send_nack(int socket_fd, PACKAGE *package, fd_set *write_fds, struct timeval *timeout);

void get_binary_string(char *data, char *result, int data_size);

void get_crc_encoded_data(char *data, char *result, int data_size);

int check_crc(PACKAGE *package);

void generate_crc(PACKAGE *package);

void create_package(PACKAGE *package, PACKAGE_TYPE type, short sequence, char *data, int size);

long int size_of_file(char *filepath);

int check_duplicated(PACKAGE *packages, PACKAGE *cur_package, int start_index, int end_index);

void sort_packages(PACKAGE *packages, int packages_qnt);

int send_control_package(int socket_fd, PACKAGE_TYPE control_type, char *control_data, int data_size);

#endif