#include "headers/server.h"

#define MAX_THREADS 5

typedef struct {
    int socket_fd;
    int position; // id in the buffer
    int satisfied;  // were all its tasks managed? 1 yes, 0 no
    int ntasks;
    char ip[16];
    int port;
    pthread_t thread_id; // will be filled automatically by pthread_create()
    int active;               /* 1 if connected, 0 if disconnected */
} Client;

// our clients's buffer
Client clients[MAX_THREADS];


// global variable: if a client wants to close the server
int close_server = 0;

// SEMAPHOREES
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexstopserver = PTHREAD_MUTEX_INITIALIZER;  // necessary to overwrite correcly variable close_server
// roomAvailable when there is space in the buffer, dataAvailable when buffer has clients already in it
pthread_cond_t roomAvailable = PTHREAD_COND_INITIALIZER;
pthread_cond_t dataAvailable = PTHREAD_COND_INITIALIZER;



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
    if (!strcmp(cmd, "help")) return CMD_HELP;
    else if (!strcmp(cmd, "stop")) return CMD_STOP;
    else if (!strcmp(cmd, "quit")) return CMD_QUIT;
    else if (strstr(cmd, "ACTIVATE") != NULL) return CMD_ACTIVATE;
    else if (strstr(cmd, "BLOCK") != NULL ) return CMD_DEACTIVATE;
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


/* Thread routine. It calls routine handleConnection() */
static void *handling_active_task(void *task)
{
    ActiveTask *t = (ActiveTask*)task; // make it a 'Client' object
    // i want to activate t.task_id...
    int pos = t->position;
    int catalog_index = t->task_id - 1;
    
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

        if (is_active == 0 || close_server == 1) {
            print_server("STOP current task!");
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

    remove_active_task(pos);

    return NULL;
}

/*
    Given a task find the oldest instance's position 
*/
static int find_instance_to_deactivate(ActiveTask *at){
    int min = -1;
    int lowest_instance = INT_MAX;
    for(int i = 0; i < MAX_NUMBER_ACTIVE_TASKS; i++){
        ActiveTask *current = &tasks_active[i];
        if(current->active && 
            current->task_id == at->task_id &&
            current->instance_id < lowest_instance)
            {
                // UPDATE
                min = i;
                lowest_instance = current->instance_id;
            }
    }
    if(min == -1)
        return -1;
    return min;
}


/* 1. UNLOCKED HELPER: Expects mutex to ALREADY be locked by caller */
static void rmvclient_unlocked(Client *client) {
    // close socket, no more connection
    if (client->socket_fd >= 0) {
        shutdown(client->socket_fd, SHUT_RDWR); // graceful closure: warn the client that we shut down the read/write conversation with it
        close(client->socket_fd);
        client->socket_fd = -1;
    }

    /* 3. Mark the slot as inactive */
    client->active = 0;
    pthread_cond_signal(&roomAvailable);
    
    /* Debugging */
    char s[100];
    snprintf(
        s,
        sizeof(s),
        "CLOSED CLIENT: cl->%s, port->%d",
        client->ip,
        client->port
    );
    print_client(s);
}


/* 2. PUBLIC WRAPPER: Locks mutex, calls unlocked helper, unlocks mutex */
static void rmvclient(Client *client) {
    pthread_mutex_lock(&mutex);
    rmvclient_unlocked(client);
    pthread_mutex_unlock(&mutex);
}


/* Handle an established  connection
   routine receive is listed in the previous example.
   The following cases are possible therefore managed:
    CMD_HELP: send help message
    CMD_QUIT: stop the current connection of the current client
    CMD_STOP: put close_server to 1
    CMD_ACTIVATE: activate according task
    CMD_DEACTIVATE: deactivate according task
*/
static void handleConnection(int currSd)
{
    unsigned int netLen;
    int len;
    char *command, *answer;
    int quit = 0; // close current client
    int task_number = -1;
    ActiveTask candidate_task;
    
    while(1)
    {
        /* Get the command string length
        If receive fails, the client most likely exited */
        if(receive(currSd, (char *)&netLen, sizeof(netLen))){
            // an error occured in the recv() method
            print_server_warning("receive failed before command: client exited");
            char s[50];
            snprintf(
                s,
                sizeof(s),
                "currSd value was: %d", currSd
            );
            print_server_warning(s);         
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
        
        print_server(command);
        
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

                if(is_schedulable(candidate_task) == -1){
                    // snprintf(
                        //     s,
                        //     sizeof(s),
                        //     "CLOSED CLIENT: cl->%s, port->%d",
                        //     client->ip,
                        //     client->port
                        // );
                    
                    // task NOT SCHEDULABLE
                    print_server("[SCHEDULER] Task rejected: System unschedulable");
                    answer = strdup("TASK_REJECTED: System unschedulable (Deadline miss risk)");

                }
                else{
                    // task SCHEDULABLE: add it to 'active_tasks' array!
                    int free_pos = add_active_task(&candidate_task);
                    // generate a thread for it
                    if(free_pos == -1){
                        print_server("FULL CAPACITY, retry or deactivate!");
                        answer = strdup("FULL CAPACITY tasks_active[]");
                        break;
                    }

                    if(pthread_create(&tasks_active[free_pos].thread_id, NULL, handling_active_task, &tasks_active[free_pos])){
                        print_server_error("Failed to spawn worker thread for task");
                        answer = strdup("ERROR: Thread creation failed");
                        break;
                    }

                    // Detach thread so resources auto-reclaim on completion
                    pthread_detach(tasks_active[free_pos].thread_id);

                    char buf[128];
                    snprintf(buf, sizeof(buf), "TASK %d ACTIVATED in slot %d", task_number, free_pos);
                    print_server(buf);
                    
                    answer = strdup("TASK_ACTIVATED");
                }
                break;
            
            case CMD_DEACTIVATE:
                task_number = parse_task_number(command);
                if(task_number < 1 || task_number > 4){
                    print_server_error("DEACTIVATE no number detected in the client command");
                    answer = strdup("ERROR invalid number detected: must be in [1-4] ");
                    break;
                }
                candidate_task.task_id = task_number;
                candidate_task.client_owner_fd = currSd;
               
                // must disable the thread
                    // associated to this task
                        // the lowest instance (first one that arrived)
                // how? by setting that activetask in tasks_active to NOT active
                pthread_mutex_lock(&active_tasks_mutex);

                // scan for all tasks_active, chose the position where the lowest
                //  instance of this client is active
                int pos = find_instance_to_deactivate(&candidate_task);
                if (pos == -1) {
                    pthread_mutex_unlock(&active_tasks_mutex);
                    print_server_error("ERROR handleConnection(): NO INSTANCE TO DEACTIVATE");
                    answer = strdup("ERROR task not active");
                    break;
                }
                tasks_active[pos].active = 0;   // immediately set it to not active, make it exit
                                                // from the loop in handling_active_task()
                pthread_mutex_unlock(&active_tasks_mutex);

                answer = strdup("task DEACTIVATED");
                break;
        }
        
        if (!answer) {
            print_server_error("Memory allocation failed in strdup");
            free(command);
            break;
        }

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
        
        
        /* 5. Trigger Shutdown Broadcast if 'stop' was issued */
        if (close_server || quit) {
            /*'close_server': Notify all other clients, close their sockets, and wake select() */
            /*'quit': close client*/
            break;
        }
    }
    
}


/* Thread routine. Calls routine handleConnection() to handle current client
   [ENDING] remove current client from clients array
*/
static void *connectionHandler(void *client)
{
    Client *c = (Client*)client; // make it a 'Client' object
    handleConnection(c->socket_fd);

    rmvclient(c);

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
    pthread_mutex_lock(&mutex);

    int free_position = findFreePosition();

    while (free_position == -1) {
        print_server_error("Max capacity reached. Waiting for space...\n");
        pthread_cond_wait(&roomAvailable, &mutex);
        free_position = findFreePosition();
    }

    /* Initialize client slot */
    Client *client = &clients[free_position];
    client->active = 1;
    client->position = free_position;
    client->socket_fd = current_socket;
    /* Convert port from network byte order */
    client->port = ntohs(retSin->sin_port);

    /* Convert binary IPv4 address to ASCII string */
    inet_ntop(
        AF_INET,
        &retSin->sin_addr,
        client->ip,
        sizeof(client->ip)
    );
    
    /* Create thread for this client */
    if (pthread_create(
            &client->thread_id,
            NULL,
            connectionHandler,
            client
        ) != 0)
    {
        pthread_mutex_unlock(&mutex);
        print_server_error("pthread_create failed");
        /* Undo the allocation of this slot */
        client->active = 0;
        return;
    }
    pthread_mutex_unlock(&mutex);


    /* Debugging */
    char s[100];
    snprintf(
        s,
        sizeof(s),
        "NEW CLIENT: cl->%s, port->%d",
        client->ip,
        client->port
    );
    print_client(s);
}


/*
At the end of the program, before closing, the server sends to each
active client a SHUTDOWN message.
*/
static void closing_broadcast_to_clients(){
    pthread_mutex_lock(&mutex);
    
    const char *shutdown_msg = "SERVER_SHUTDOWN";
    int msg_len = strlen(shutdown_msg);

    print_server("SERVER_SHUTDOWN: activated");

    for(int i = 0; i < MAX_THREADS; i++){
        Client *current_client = &clients[i];

        // if the client is NOT active go to the next
        if(current_client->active == 0)
            continue;

        if(current_client->active == 1){
            unsigned int netLen = htonl(msg_len);
            if(send(current_client->socket_fd, &netLen, sizeof(netLen), 0) == -1){
                print_server_error(" SEND in closing_broadcast (number of chars)");
            }
            if(send(current_client->socket_fd, shutdown_msg, msg_len, 0) == -1){
                print_server_error(" SEND in closing_broadcast (ans)");
            }
            rmvclient_unlocked(current_client);
        }
    }

    print_server("SERVER_SHUTDOWN: end");
    pthread_mutex_unlock(&mutex);
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
    c->thread_id = 0;
}

static void init_clients_buffer(){
    pthread_mutex_lock(&mutex);
    print_server("init CLIENTS BUFFER");
    for(int i = 0; i < MAX_THREADS; i++)
        init_client(&clients[i]);
    
    pthread_mutex_unlock(&mutex);
}




int main(int argc, char *argv[]){
    int server_socket, server_port;
    int *currSock;
    int sAddrLen;
    struct sockaddr_in sin, retSin;

    pthread_t threads[MAX_THREADS];

    if(argc < 2){
        print_server_error("[SERVER] ERROR not enough arguments: specify PORT");
        exit(-1);
    }

    sscanf(argv[1], "%d", &server_port);
    
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

    while(!close_server)
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
            if (errno == EINTR) {
                continue;
            }

            /* Print the EXACT system error using strerror(errno) */
            char err_buf[128];
            snprintf(err_buf, sizeof(err_buf), "SELECT error: %s", strerror(errno));
            print_server_error(err_buf);
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
    closing_server(server_socket);

    // Destroy mutexes and condition variables
    pthread_mutex_destroy(&mutex);
    pthread_mutex_destroy(&mutexstopserver);
    pthread_cond_destroy(&roomAvailable);
    pthread_cond_destroy(&dataAvailable);
    pthread_mutex_destroy(&active_tasks_mutex);
    sem_destroy(&free_slots_sem);
    
    // close server
    print_server("CLOSING cleanly");

    return 0;
}