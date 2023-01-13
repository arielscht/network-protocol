#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "socket.h"
#include "protocol.h"

#define MAX_COMMAND_SIZE 32

void listen_message_mode(int sock_fd)
{
    char message[MAX_TEXT_MESSAGE_SIZE];
    bzero(&message, sizeof(message));

    printf("You are now on messaging mode. Usage:\n");
    printf("- Type any message and press ENTER to send it.\n");
    printf("- Use :e to exit messaging mode.\n\n");

    printf("> ");
    fgets(message, MAX_TEXT_MESSAGE_SIZE, stdin);
    message[strlen(message) - 1] = '\0';
    while (strcmp(message, ":e") != 0)
    {
        send_text_message(sock_fd, message);
        bzero(&message, sizeof(message));
        printf("> ");
        fgets(message, MAX_TEXT_MESSAGE_SIZE, stdin);
        message[strlen(message) - 1] = '\0';
    }

    return;
}

int main()
{
    char *interface_name = "vboxnet0";
    int socket = 0;
    char command[MAX_COMMAND_SIZE];
    pthread_t threads_id[2];

    socket = create_raw_socket(interface_name);

    fgets(command, MAX_COMMAND_SIZE, stdin);
    command[strlen(command) - 1] = '\0';
    while (strcmp(command, ":q") != 0)
    {
        if (strcmp(command, "i") == 0)
        {
            listen_message_mode(socket);
        }
        else
        {
            fprintf(stderr, "Error 203: Invalid command\n");
        }

        fgets(command, MAX_COMMAND_SIZE, stdin);
        command[strlen(command) - 1] = '\0';
    }

    close(socket);
    return 0;
}