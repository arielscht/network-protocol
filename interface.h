#ifndef __INTERFACE__
#define __INTERFACE__

#include <pthread.h>

#define MAX_COMMAND_SIZE 32

typedef struct
{
    int socket_fd;
    int locked;
} THREAD_PARAM;

void listen_message_mode(THREAD_PARAM *config);

void *listen_to_commands(void *config_param);

void show_progress(int current, int total, char *message);

#endif