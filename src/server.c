// my libraries
#include "headers/utils.h"
#include "headers/task.h"
#include "headers/rta.h"

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


#define MAX_THREADS 10

typedef struct {
    int socket_fd;
    int position; // id in the buffer
    char ip[16];
    int port;
    pthread_t thread; // will be filled automatically by pthread_create()
    int active;               /* 1 if connected, 0 if disconnected */
} Client;

// our clients's buffer
Client clients[MAX_THREADS];

// global variable: if a client wants to close the server
int close_server = 0;

// MUTEXes
pthread_mutex_t mutex_clients_array = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexstopserver = PTHREAD_MUTEX_INITIALIZER;  // necessary to overwrite correcly variable close_server

// SEMAPHORES
pthread_cond_t roomAvailable = PTHREAD_COND_INITIALIZER; // roomAvailable when there is space 
                                                        // in the buffer when buffer has clients
                                                        //  already in it



/* Command Enum Definition to help with tasks activation*/
typedef enum {
    CMD_HELP,
    CMD_STOP,
    CMD_QUIT,
    CMD_ACTIVATE,
    CMD_DEACTIVATE
} CommandType;

/* Helper function to map command strings to enum values */
static CommandType parse_command(const char *cmd) {
    if (!strcmp(cmd, "stop")) return CMD_STOP;
    else if (!strcmp(cmd, "quit")) return CMD_QUIT;
    else if (strstr(cmd, "ACTIVATE") != NULL) return CMD_ACTIVATE;
    else if (strstr(cmd, "BLOCK") != NULL ) return CMD_DEACTIVATE;
    return CMD_HELP;
}


// get task's number out of command
static int parse_task_number(const char *command) {
    int task_num = -1;
    // Tries to read "ACTIVATE <number>"
    if (sscanf(command, "ACTIVATE %d", &task_num) == 1) 
        return task_num;
    else if (sscanf(command, "BLOCK %d", &task_num) == 1) 
        return task_num;
    return -1; // Return -1 if parsing failed
}

// TODO explain better
/* Thread routine. It gets called by routine handleConnection() */
static void *handling_active_task(void *task)
{
    pthread_detach(pthread_self());
    ActiveTask *t = (ActiveTask*)task;
    
    // i want to activate t.task_id...
    // those fields could be overwritten therefore i save them!
    int pos = t->position;
    int catalog_index = t->task_id - 1;    
    int current_task_id = t->task_id;
    int current_task_client_port = t->client_port;
    
    if (catalog_index < 0 || catalog_index >= 4) 
        return NULL;

    if (!TASK_CATALOG[catalog_index].routine) 
        return NULL;

    TASK_CATALOG[catalog_index].routine();
    

    // Retrieve period in milliseconds from catalog
    int period_ms = TASK_CATALOG[catalog_index].period;

    struct timespec next_execution;
    clock_gettime(CLOCK_MONOTONIC, &next_execution);

    // Loop periodically until the task is deactivated or server shuts down
    while (1) {
        // Lock mutex to safely check if task was deactivated
        pthread_mutex_lock(&active_tasks_mutex);
        int is_active = tasks_active[pos].active;
        pthread_mutex_unlock(&active_tasks_mutex);
        
        // task has been blocked
        if (is_active == 0) {
            print_server("BLOCKED current task %d, client port %d!", current_task_id, current_task_client_port);
            break;
        }
        
        // task has been stopped
        if (close_server == 1) {
            print_server("STOP current task %d, client port %d!", current_task_id, current_task_client_port);
            break;
        }

        /* 1. Execute workload */
        TASK_CATALOG[catalog_index].routine();

        /* 2. Calculate next absolute wake-up time: next_execution += period */
        // NOTE nsec necessay for tasks with period < 1 sec
        next_execution.tv_sec += period_ms / 1000;
        next_execution.tv_nsec += (period_ms % 1000) * 1000000L;    // must be transformed into nanosecs!
                        

        // Handle nanosecond overflow
        if (next_execution.tv_nsec >= 1000000000L) {
            next_execution.tv_sec += 1;
            next_execution.tv_nsec -= 1000000000L;
        }

        /* 3. Sleep until absolute next activation time */
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_execution, NULL);
    }

    return NULL;
}


/* 1. UNLOCKED HELPER: Expects mutex to ALREADY be locked by caller */
static void rmvclient_unlocked(Client *client) {
    // close socket, no more connection
    if (client->socket_fd >= 0) {
        shutdown(client->socket_fd, SHUT_RDWR); // graceful closure: warn the client that we shut down the read/write conversation with it
        close(client->socket_fd);
        client->socket_fd = -1;
    }

    /* Mark the slot as inactive */
    client->active = 0;
    client->position = -1;
    pthread_cond_signal(&roomAvailable);

}


/* 2. PUBLIC WRAPPER: Locks mutex, calls unlocked helper, unlocks mutex */
static int rmvclient(Client *client) {
    if(client->active == 0)
        return -1;    // this client has been deactivated by a stop command 
                      // by another client
    pthread_mutex_lock(&mutex_clients_array);
    
    rmvclient_unlocked(client);
    
    pthread_mutex_unlock(&mutex_clients_array);

    return 1;
}


/* Handle an established  connection
   routine receive is listed in the previous example.
   The following cases are possible therefore managed:
    CMD_HELP: send help message
    CMD_QUIT: stop the current connection of the current client by exiting the connection by a break, rmvclient() will close the current client
    CMD_STOP: set close_server to 1
    CMD_ACTIVATE: activate according task
    CMD_DEACTIVATE: deactivate according task
*/
static void handleConnection(Client *c)
{
    unsigned int netLen;
    int len;
    char *command, *answer;
    int quit = 0; // close current client
    int task_number = -1;
    int currSd = c->socket_fd;
    ActiveTask candidate_task;
    
    while(c->active && quit == 0)
    {
        /* Get the command string length
        If receive fails, the client most likely exited */
        if(receive(currSd, (char *)&netLen, sizeof(netLen))){
            // an error occured in the recv() method
            if(close_server == 0)
                print_server_error("receive failed before command: client %d exited", c->port);
            print_server("[CLIENT %d] probably exited", c->port);
            break;
        } 
        /* Convert from network byte order */
        len = ntohl(netLen);
        command = malloc(len+1);
        if (!command) {
            print_server_error("Memory allocation failed for command in handleConnection()");
            break;
        }

        /* Get the command */
        if (receive(currSd, command, len) == -1) {
            free(command);
            break;
        }
        command[len] = '\0'; //to end the command string
        
        print_server("[CLIENT %d] COMMAND: %s",c->port, command);
        
        /* Execute the command using switch */
        CommandType cmd_type = parse_command(command);

        switch (cmd_type) {
            case CMD_HELP:
                answer = strdup(    // NOTE strdup = malloc + strcpy -> must be checked for malloc failures!
                    "server is active.\n\n"
                    "    commands:\n"
                    "       help: print this help\n"
                    "       quit: stop client connection\n"
                    "       stop: force stop server connection\n"
                    "       a [NUMBER]: activate task [NUMBER]\n"
                    "       b [NUMBER]: deactivate/block task [NUMBER]\n"
                    "       verbose: [0,3) \n"
                );
                break;

            case CMD_STOP:
                pthread_mutex_lock(&mutexstopserver);
                answer = strdup("closing SERVER connection...");
                close_server = 1;
                pthread_mutex_unlock(&mutexstopserver);
                break;

            case CMD_QUIT:
                answer = strdup("closing CLIENT connection...");
                quit = 1;
                break;

            case CMD_ACTIVATE:
                // which tasks we refer to?
                task_number = parse_task_number(command);
                if(task_number < 1 || task_number > 4){
                    print_server_error("ACTIVATE no number detected in the client command");
                    answer = strdup("ERROR invalid number detected: must be in [1-4] ");
                    break;
                }
                // declare new task to be potentially added
                candidate_task.task_id = task_number;
                candidate_task.client_owner_fd = currSd;
                candidate_task.client_port = c->port;

                if(is_schedulable(&candidate_task) == -1){
                    // task NOT SCHEDULABLE
                    print_server_warning("[CLIENT %d] TASK %d NOT SCHEDULABLE", c->port, task_number);

                    answer = strdup("TASK_REJECTED: System unschedulable (Deadline miss risk)");
                }
                else{
                    // task SCHEDULABLE: add it to 'active_tasks' array!
                    int free_pos = add_active_task_and_return_position(&candidate_task);
                    if(free_pos == -1){
                        print_server_warning("[CLIENT %d] FULL CAPACITY for tasks_active array, retry or deactivate!", c->port);
                        answer = strdup("FULL CAPACITY tasks_active[]");
                        break;
                    }
                    
                    // generate a thread for the task
                    if(pthread_create(&tasks_active[free_pos].thread_id, NULL, handling_active_task, &tasks_active[free_pos]) != 0){
                        print_server_error("Failed to spawn worker thread for task");
                        answer = strdup("ERROR: Thread creation failed");
                        tasks_active[free_pos].active = 0;
                        break;
                    }

                    print_server("[CLIENT %d] TASK %d ACTIVATED in slot %d", c->port, task_number, free_pos);                    
                    answer = strdup("task ACTIVATED");

                }
                break;
            
            case CMD_DEACTIVATE:
                task_number = parse_task_number(command);
                if(task_number < 1 || task_number > 4){
                    print_server_error("DEACTIVATE no number detected in the client command");
                    answer = strdup("ERROR invalid number detected: must be in [1-4] ");
                    break;
                }
                
                // define the candidate to find and then deactivate
                candidate_task.task_id = task_number;
                candidate_task.client_owner_fd = currSd;
                candidate_task.client_port = c->port;

                
                if (find_and_remove_active_task(&candidate_task) == -1) {
                    answer = strdup("task was NOT active");
                    // print_server_warning("[CLIENT %d] NO INSTANCE TO DEACTIVATE for task %d", c->port, task_number);
                    break;
                }

                answer = strdup("task DEACTIVATED");

                break;
        }
        
        if (!answer) {
            print_server_error("Memory allocation failed in strdup");
            free(command);
            break;
        }

        /* another client stopped the server: OUT */
            
        
        /* we must send 'ans' to Client */
        len = strlen(answer);
        netLen = htonl(len);
        
        /* Send answer character length to client */
        if (send(currSd, &netLen, sizeof(netLen), 0) == -1){
            free(command);
            free(answer);
            break;
        }
        /* Send answer characters to client*/
        if (send(currSd, answer, len, 0) == -1){
            free(command);
            free(answer);
            break;
        }
        
        free(command);
        free(answer);        

    }
    
}


/* Thread routine. Calls routine handleConnection() to handle current client
   [ENDING] remove current client from clients array
*/
static void *connectionHandler(void *client)
{
    // once finished this routine release resources
    pthread_detach(pthread_self());
    Client *c = (Client*)client; // make it a 'Client' object
    
    handleConnection(c);

    int port = c->port;
    if(rmvclient(c) == -1)
        return NULL;
    
    /* Debugging */
    print_client("CLOSED CLIENT: port->%d",
        port
    );
    return NULL;
}


/* Searches for the first index in 'clients' where active == 0.
   Returns the slot index if found, or -1 if the server is full. */
static int findFreePosition(void) {
    for (int i = 0; i < MAX_THREADS; i++) {
        if (clients[i].active == 0) {
            return i; /* Found an empty slot */
        }
    }
    return -1; /* Server is full */
}

/*
    Add a client to our buffer 'clients', then create a separate thread
    for it which will be managed by connectionHandler()
*/
static void addclient(int current_socket, struct sockaddr_in *retSin)
{
    int port = ntohs(retSin->sin_port);
    print_server("[CLIENT MUTEX] [CLIENT %d] WAITING ", port);
    
    
    pthread_mutex_lock(&mutex_clients_array);
    print_server("[CLIENT MUTEX] [CLIENT %d] ACQUIRED ", port);

    int free_position = findFreePosition();

    while (free_position == -1) {
        print_server_warning("Max clients capacity reached. Waiting for space...\n");
        pthread_cond_wait(&roomAvailable, &mutex_clients_array);
        free_position = findFreePosition();
    }

    /* Initialize client slot */
    Client *client = &clients[free_position];
    client->active = 1;
    client->position = free_position;
    client->socket_fd = current_socket;
    /* Convert port from network byte order */
    client->port = port;

    /* Convert binary IPv4 address to ASCII string */
    inet_ntop(
        AF_INET,
        &retSin->sin_addr,
        client->ip,
        sizeof(client->ip)
    );
    
    /* Create thread for this client */
    if (pthread_create(
            &client->thread,
            NULL,
            connectionHandler,
            client
        ) != 0)
    {
        pthread_mutex_unlock(&mutex_clients_array);
        print_server_error("pthread_create failed");
        print_server("[CLIENT MUTEX] [CLIENT %d] RELEASED ", port);
        /* Undo the allocation of this slot */
        client->active = 0;
        return;
    }

    // Detach thread so resources auto-reclaim on completion
    pthread_detach(client->thread);

    pthread_mutex_unlock(&mutex_clients_array);
    print_server("[CLIENT MUTEX] [CLIENT %d] RELEASED ", port);

    /* Debugging */
    print_client("NEW CLIENT: port->%d",
        client->port);
}


/*
At the end of the program, before closing, the server sends to each
active client a SHUTDOWN message.
*/
static void closing_broadcast_to_clients(){
    print_server("SERVER_SHUTDOWN: waiting");

    const char *shutdown_msg = "SERVER_SHUTDOWN";
    int msg_len = strlen(shutdown_msg);
    
    pthread_mutex_lock(&mutex_clients_array);
    pthread_mutex_lock(&log_mutex);    

    print_server_unlocked("SERVER_SHUTDOWN: activated");

    for(int i = 0; i < MAX_THREADS; i++){
        Client *current_client = &clients[i];

        // if the client is NOT active go to the next
        if(current_client->active == 0)
            continue;

        if(current_client->active == 1){
            unsigned int netLen = htonl(msg_len);
            if(send(current_client->socket_fd, &netLen, sizeof(netLen), 0) == -1){
                print_server_error_unlocked(" SEND in closing_broadcast (number of chars)");
            }
            if(send(current_client->socket_fd, shutdown_msg, msg_len, 0) == -1){
                print_server_error_unlocked(" SEND in closing_broadcast (ans)");
            }
            int client_port = current_client->port;
            rmvclient_unlocked(current_client);
            print_client_unlocked("SHUTDOWN [CLIENT %d]", client_port);
        }
    }

    print_server_unlocked("SERVER_SHUTDOWN: end");
    pthread_mutex_unlock(&log_mutex);
    pthread_mutex_unlock(&mutex_clients_array);

}

static int closing_server(int sock){
    close(sock);
}

static void init_client(Client *c){
    c->active = 0;
    c->port = 0;
    c->position = 0;
    c->ip[0] = '\0';
    c->socket_fd = -1;
    c->thread = 0;
}

static void init_clients_buffer(){
    pthread_mutex_lock(&mutex_clients_array);
    print_server("init CLIENTS BUFFER");
    for(int i = 0; i < MAX_THREADS; i++)
        init_client(&clients[i]);
    
    pthread_mutex_unlock(&mutex_clients_array);
}

static int check_number_of_active_clients(){
    pthread_mutex_lock(&mutex_clients_array);
    int tot = 0;
    for(int i = 0; i < MAX_THREADS; i++){
        if(clients[i].active == 1)
            tot++;
    }
    pthread_mutex_unlock(&mutex_clients_array);

    return tot;
}



int main(int argc, char *argv[]){
    int server_socket, server_port;
    int *currSock;
    int sAddrLen;
    struct sockaddr_in sin, retSin;

    pthread_t threads[MAX_THREADS];

    if (argc < 3) {
        print_server_error("[SERVER] ERROR: Not enough arguments. Usage: ./server <PORT> <VERBOSE>");
        exit(EXIT_FAILURE);
    }

    // Parse PORT (argv[1])
    if (sscanf(argv[1], "%d", &server_port) != 1 || server_port <= 0 || server_port > 65535) {
        print_server_error("[SERVER] ERROR: Invalid port number");
        exit(EXIT_FAILURE);
    }

    // Parse VERBOSE (argv[2])
    if (sscanf(argv[2], "%d", &verbose) == 1 && (verbose > 0 ))
        print_server("Verbose set to %d", verbose);
    else 
        print_server("[SERVER] Invalid verbose input, defaulting to 0\n");


    if((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        print_server_error("socket not correct");
        exit(-1);
    }
    print_server("SOCKET ACTIVATED");
    
    // Allow the server to reuse the local address/port after the socket is closed,
    // particularly when the previous TCP connection is in TIME_WAIT.
    int reuse = 1;
    if(setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0)
        print_server_error("setsockopt(SO_REUSEADDR) failed");

    /* Initialize the address (struct sokaddr_in) fields */
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(server_port);
    
    // BIND socket to the port
    if(bind(server_socket, (struct sockaddr *) &sin, sizeof(sin)) == -1){
        print_server_error("BIND failed");
        exit(-1);
    }

    // listen and accept MAX_THREADS clients
    if(listen(server_socket, MAX_THREADS) == -1){
        print_server_error("LISTEN failed");
        exit(-1);
    }

    sAddrLen = sizeof(retSin);
    init_clients_buffer();
    init_active_tasks();

    while(close_server == 0)
    {
        fd_set read_fds;    // necessary for SELECT
        FD_ZERO(&read_fds);   // as theory says...
        FD_SET(server_socket, &read_fds);  // keep checking server_socket
        
        // timer
        struct timeval timeout;
        timeout.tv_sec = 3;
        timeout.tv_usec = 0;

        // IMP: check if server_socket has a client waiting in the connection queue
        int activity = select(server_socket + 1, &read_fds, NULL, NULL, &timeout); // server_soc + 1: highest fd + 1
        if(activity < 0){
            /* If select was interrupted by a signal, don't crash — just retry */
            if (errno == EINTR) { //TODO
                continue;
            }

            /* Print the EXACT system error using strerror(errno) */
            print_server_error("SELECT error: %s", strerror(errno));
            break; // Exit loop cleanly instead of immediate exit(1)
        }

        // timer out
        else if(activity == 0){
            continue;   // no new connection has arrived: 
                        //      SELECT takes out server_socket from read_fds
        }
        
        // activity > 0: new connection established!
        // NOTE socket readable iff a new client has completed the TCP handshake and is sitting in the connection queue.
        if(FD_ISSET(server_socket, &read_fds)){
            // SUMMARY add a client only if server_socket notes that a client wants to connect!
            
            int currSock;

            //ACCEPT connections            
            if((currSock = accept(server_socket, (struct sockaddr *)&retSin, &sAddrLen)) == -1)
                print_server_error("ACCEPT failed");
            
            // add client to the buffer, manage its connection
            addclient(currSock, &retSin);
        }
    }

    // close each client still connected to the server
    closing_broadcast_to_clients();
    
    // print remaining clients in the client structure
    int res = check_number_of_active_clients();
    if(res != 0){
        print_server_error("CLIENTS not all released : %d remaining", res);
        exit(1);
    }

    // Destroy mutexes and condition variables
    pthread_mutex_destroy(&mutex_clients_array);
    pthread_mutex_destroy(&mutexstopserver);
    pthread_cond_destroy(&roomAvailable);
    pthread_mutex_destroy(&active_tasks_mutex);
    sem_destroy(&free_slots_sem);

    closing_server(server_socket);    
    print_server("CLOSING cleanly");

    return 0;
}