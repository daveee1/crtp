// my libraries
#include "headers/utils.h"
#include "headers/task.h"
#include "headers/rsa.h"

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

static void handleConnection(int currSd);
