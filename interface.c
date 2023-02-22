#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "protocol/protocol.h"
#include "protocol/utils.h"
#include "interface.h"

void listen_message_mode(THREAD_PARAM *config)
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
        config->locked = 1;
        send_text_message(config->socket_fd, message);
        config->locked = 0;
        bzero(&message, sizeof(message));
        printf("> ");
        fgets(message, MAX_TEXT_MESSAGE_SIZE, stdin);
        message[strlen(message) - 1] = '\0';
        if (config->locked)
            break;
    }

    return;
}

void *listen_to_commands(void *config_param)
{
    THREAD_PARAM *config = (THREAD_PARAM *)config_param;
    char command[MAX_COMMAND_SIZE];

    fgets(command, MAX_COMMAND_SIZE, stdin);
    command[strlen(command) - 1] = '\0';
    while (strcmp(command, ":q") != 0)
    {
        if (strcmp(command, "i") == 0)
        {
            listen_message_mode(config);
        }
        else if (strstr(command, ":send") != NULL)
        {
            char *filepath;
            filepath = strtok(command, " ");

            while (filepath != NULL)
            {
                if (strcmp(filepath, ":send") != 0)
                {
                    break;
                }

                filepath = strtok(NULL, " ");
            }

            if (access(filepath, R_OK) == 0)
            {
                long int file_size = size_of_file(filepath);
                config->locked = 1;
                send_file(config->socket_fd, filepath, file_size);
                config->locked = 0;
            }
            else
                printf("The file %s does not exists or it does not have the read permission\n", filepath);
        }
        else
        {
            fprintf(stderr, "Error 203: Invalid command\n");
        }

        fgets(command, MAX_COMMAND_SIZE, stdin);
        command[strlen(command) - 1] = '\0';
    }

    return NULL;
}

void show_progress(int current, int total, char *message)
{
    float progress = (current / (float)total) * 100;
    char progress_full[21] = {'=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '=', '\0'};
    char progress_empty[21] = {'.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '\0'};
    int progress_index = progress / 5;
    progress_full[progress_index] = '\0';
    progress_empty[20 - progress_index] = '\0';
    fflush(stdout);
    printf("\r\33[2K%s [", message);
    printf("%s", progress_full);
    printf("%s", progress_empty);
    printf("] %3.2f%%", progress);
    if (progress >= 100)
        printf("\n");
}