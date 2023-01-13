#include <stdio.h>
#include <unistd.h>
#include "protocol/protocol.h"
#include "protocol/utils.h"
#include "socket.h"

int main()
{
    char *interface_name = "enp1s0";
    int socket = 0;
    // socket = create_raw_socket(interface_name);

    PACKAGE package;
    // DATA: 0110000101100010011000110110010001100101
    // CRC: 001
    // create_package(&package, TEXT, 0, "abcde");
    create_package(&package, TEXT, 0, "abcde");
    // generate_crc(&package);
    printf("crc: %x\n", package.crc);

    printf("DATA BEFORE: %s\n", package.data);
    printf("DATA AFTER: %s\n", package.data);
    int isValid = check_crc(&package);
    printf("PACKAGE IS VALID: %d\n", isValid);
    package.data[3] = 'h';
    int isValid2 = check_crc(&package);
    printf("PACKAGE IS VALID: %d\n", isValid2);

    // wait_for_packages(socket);
    return 0;
}