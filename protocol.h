
#define INIT_MARKER 126
#define MAX_DATA_SIZE 63
#define MAX_SEQUENCE 15
#define CRC_POLYNOMIAL 0x9b
#define MAX_TEXT_MESSAGE_SIZE 1000

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

void create_package(PACKAGE *package, PACKAGE_TYPE type, short sequence, char *data);

void send_text_message(int socket_fd, char *message);

void get_text_message(int socket_fd);

void wait_for_packages(int socket_fd);