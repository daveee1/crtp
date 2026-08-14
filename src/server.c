#include "headers/server.h"

#define MAX_THREADS 2

typedef struct {
    int socket_fd;
    int position; // id in the buffer
    int satisfied;  // were all its tasks managed? 1 yes, 0 no
    int *tasks; // which tasks is going to request
    int ntasks;
    char ip[16];
    int port;
    pthread_t thread_id; // will be filled automatically by pthread_create()
    int active;               /* 1 if connected, 0 if disconnected */
} Client;

// our clients's buffer
Client clients[MAX_THREADS];
 

// SEMAPHOREES
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexstopserver = PTHREAD_MUTEX_INITIALIZER;
// roomAvailable when there is space in the buffer, dataAvailable when buffer has clients already in it
pthread_cond_t roomAvailable = PTHREAD_COND_INITIALIZER;
pthread_cond_t dataAvailable = PTHREAD_COND_INITIALIZER;

// if a client wants to close the server
int close_server = 0;




/* Command Enum Definition to help with tasks activation*/
typedef enum {
    CMD_HELP,
    CMD_STOP,
    CMD_QUIT,
    CMD_ACTIVATE,
    CMD_DEACTIVATE,
    CMD_UNKNOWN
} CommandType;

/* Helper function to map command strings to enum values */
static CommandType parse_command(const char *cmd) {
    if (!strcmp(cmd, "help")) return CMD_HELP;
    if (!strcmp(cmd, "stop")) return CMD_STOP;
    if (!strcmp(cmd, "quit")) return CMD_QUIT;
    if (!strncmp(cmd, "ACTIVATE", 8)) return CMD_ACTIVATE;
    if (!strncmp(cmd, "DEACTIVATE", 10) ) return CMD_DEACTIVATE;
    return CMD_UNKNOWN;
}



/* Handle an established  connection
   routine receive is listed in the previous example */
static void handleConnection(int currSd)
{
    unsigned int netLen;
    int len;
    char *command, *answer;
    int quit = 0; // close current client
    
    while(1)
    {
        /* Get the command string length
        If receive fails, the client most likely exited */
        if(receive(currSd, (char *)&netLen, sizeof(netLen))) break;
        
        /* Convert from network byte order */
        len = ntohl(netLen);
        command = malloc(len+1);
        if (!command) {
            print_server_error("Memory allocation failed for command in handleConnection()");
            break;
        }

        /* Get the command and write terminator */
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

            case CMD_UNKNOWN:
            default:
                answer = strdup("invalid command (try help).");
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

/* 1. UNLOCKED HELPER: Expects mutex to ALREADY be locked by caller */
static void rmvclient_unlocked(Client *client) {
    Client *c = &clients[client->position];

    // close socket, no more connection
    if (c->socket_fd >= 0) {
        shutdown(c->socket_fd, SHUT_RDWR);
        close(c->socket_fd);
        c->socket_fd = -1;
    }

    // free dynamically allocated memory
    if (c->tasks != NULL) {
        free(c->tasks);
        c->tasks = NULL;
    }

    /* 3. Mark the slot as inactive */
    c->active = 0;
    pthread_cond_signal(&roomAvailable);
    
    /* Debugging */
    char s[100];
    snprintf(
        s,
        sizeof(s),
        "CLOSED CLIENT: cl->%s, port->%d",
        c->ip,
        c->port
    );
    print_client(s);
}


/* 2. PUBLIC WRAPPER: Locks mutex, calls unlocked helper, unlocks mutex */
static void rmvclient(Client *client) {
    pthread_mutex_lock(&mutex);
    rmvclient_unlocked(client);
    pthread_mutex_unlock(&mutex);
}


/* Thread routine. It calls routine handleConnection() */
static void *connectionHandler(void *client)
{
    Client *c = (Client*)client; // make it a 'Client' object
    handleConnection(c->socket_fd);
    rmvclient(c);
    return NULL;
}

/* Searches for the first index where active == 0.
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
    Add a client to our buffer 'clients'.
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
    clients[free_position].active = 1;
    clients[free_position].position = free_position;
    clients[free_position].socket_fd = current_socket;
    /* Convert port from network byte order */
    clients[free_position].port = ntohs(retSin->sin_port);

    /* Convert binary IPv4 address to ASCII string */
    inet_ntop(
        AF_INET,
        &retSin->sin_addr,
        clients[free_position].ip,
        sizeof(clients[free_position].ip)
    );
    
    /* Create thread for this client */
    if (pthread_create(
            &clients[free_position].thread_id,
            NULL,
            connectionHandler,
            &clients[free_position]
        ) != 0)
    {
        print_server_error("pthread_create failed");
        /* Undo the allocation of this slot */
        clients[free_position].active = 0;
        return;
    }

    pthread_mutex_unlock(&mutex);

    /* Debugging */
    char s[100];
    snprintf(
        s,
        sizeof(s),
        "NEW CLIENT: cl->%s, port->%d",
        clients[free_position].ip,
        clients[free_position].port
    );
    print_client(s);
}

static void closing_broadcast_to_clients(){
    pthread_mutex_lock(&mutex);
    
    const char *shutdown_msg = "SERVER_SHUTDOWN";
    int msg_len = strlen(shutdown_msg);

    print_server("SERVER_SHUTDOWN: activated\n");

    for(int i = 0; i < MAX_THREADS; i++){
        Client *current_client = &clients[i];

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

int closing_server(int sock){
    close(sock);
}

void init_client(Client *c){
    c->active = 0;
    c->port = 0;
    c->position = 0;
    c->ip[0] = '\0';
    c->ntasks = 0;
    c->satisfied = 0;
    c->socket_fd = -1;
    c->tasks = NULL;
    c->thread_id = 0;
}

void init_clients_buffer(){
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
    // set REUSE ADDRESS option:
    // if i close the socket but there are still packets traversing 
    // then i wait till i receive all the traffic. ALSO even if
    // i close this socket create a new one on this port anyway
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

    while(!close_server)
    {
        fd_set read_fds;    // necessary for SELECT
        FD_ZERO(&read_fds);   // as theory says...
        FD_SET(server_socket, &read_fds);  // keep checking server_socket
        
        // timer
        struct timeval timeout;
        timeout.tv_sec = 3;
        timeout.tv_sec = 0;

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
    pthread_cond_destroy(&roomAvailable);

    // close server
    print_server("CLOSING cleanly");

    return 0;
}