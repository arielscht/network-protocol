#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "socket.h"
#include "protocol/protocol.h"
#include "interface.h"

int main()
{
    char *interface_name = "enp1s0";
    int socket = 0;
    pthread_t threads_id[2];
    THREAD_PARAM config;

    socket = create_raw_socket(interface_name);

    config.socket_fd = socket;
    config.locked = 0;

    pthread_create(&threads_id[0], NULL, &listen_to_commands, (void *)&config);
    pthread_create(&threads_id[1], NULL, &wait_for_packages, (void *)&config);

    pthread_join(threads_id[0], NULL);
    pthread_join(threads_id[1], NULL);

    close(socket);
    return 0;
}