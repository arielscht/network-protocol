#ifndef __PROTOCOL__
#define __PROTOCOL__

#define INIT_MARKER 126
#define MAX_DATA_SIZE 63
#define MAX_SEQUENCE 16
#define WINDOW_SIZE 8
#define CRC_POLYNOMIAL "110011011"
#define MAX_TEXT_MESSAGE_SIZE 1000
#define BITS_IN_BYTE_QNT 8

#define VLAN_PROTOCOL_ONE 0x88
#define VLAN_PROTOCOL_TWO 0x81
#define ESCAPE 0xff

#define TIMEOUT_IN_SECONDS 5

#include <sys/select.h>

typedef enum
{
    TEXT = 0x01,
    MEDIA = 0x10,
    ACK = 0x0a,
    NACK = 0x00,
    ERROR = 0x1e,
    INIT = 0x1d,
    END = 0x0f,
    DATA = 0x0d,
} PACKAGE_TYPE;

typedef struct
{
    unsigned char init_marker;
    unsigned char type : 6;
    unsigned char sequence : 4;
    unsigned char size : 6;
    char data[MAX_DATA_SIZE];
    unsigned char crc;
} PACKAGE;

void send_text_message(int socket_fd, char *message);

void get_text_message(int socket_fd);

void *wait_for_packages(void *config_param);

void send_file(int socket_fd, char *filepath, long int file_size);

#endif