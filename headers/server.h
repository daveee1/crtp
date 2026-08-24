// my libraries
#include "headers/utils.h"
#include "headers/task.h"
#include "headers/rta.h"

#include <limits.h> // needed to use INT_MAX as lowest_instance in 'find_instance_to_deactivate()'
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h> 



typedef struct {
    int socket_fd;
    int position; // id in the buffer
    int satisfied;  // were all its tasks managed? 1 yes, 0 no
    int ntasks;
    char ip[16];
    int port;
    pthread_t thread; // will be filled automatically by pthread_create()
    int active;               /* 1 if connected, 0 if disconnected */
} Client;

static void handleConnection(Client *c);
