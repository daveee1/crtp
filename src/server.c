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


/* Receive routine: use recv to receive from socket and manage
   the fact that recv may return after having read less bytes than
   the passed buffer size
   In most cases recv will read ALL requested bytes, and the loop body
   will be executed once. This is not however guaranteed and must
   be handled by the user program. The routine returns 0 upon
   successful completion, -1 otherwise */
static int receive(int sd, char *retBuf, int size){
    
    int totSize, currSize;
    totSize = 0;
    
    while(totSize < size)
    {
        currSize = recv(sd, &retBuf[totSize], size - totSize, 0);
        if(currSize <= 0)
            /* An error occurred */
            return -1;
        totSize += currSize;
    }

    return 0;
}

/* Handle an established  connection
   routine receive is listed in the previous example */
static void handleConnection(int currSd)
{
    unsigned int netLen;
    int len;
    char *command, *answer;
    
    while(1)
    {
        /* Get the command string length
        If receive fails, the client most likely exited */
        if(receive(currSd, (char *)&netLen, sizeof(netLen))) break;
        
        /* Convert from network byte order */
        len = ntohl(netLen);
        command = malloc(len+1);

        /* Get the command and write terminator */
        if (receive(currSd, command, len) == -1) {
            free(command);
            break;
        }
        command[len] = 0; //to end the command string
        
        // TODO DEBUGGING
        print_server(command);
        
        /* Execute the command and get the answer character string */    
        if(strcmp(command,"help") == 0)
            answer = strdup(
                "server is active.\n\n"
                "    commands:\n"
                "       help: print this help\n"
                "       quit: stop client connection\n"
                "       stop: force stop server connection\n"
                );
        else if (strcmp(command,"stop") == 0) {
            pthread_mutex_lock(&mutexstopserver);
            answer = strdup("closing server connection");
            close_server = 1;
            pthread_mutex_unlock(&mutexstopserver);
        }
        else 
            answer = strdup("invalid command (try help).");
            
        /* Convert to network byte order */
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
        if (close_server)  
            break;
    }
    
}

static void rmvclient(Client *client){
    pthread_mutex_lock(&mutex);

    Client *c = &clients[client->position];

    /* 1. Close the socket connection */
    if (c->socket_fd >= 0) {
        char s[70];
        snprintf(s, sizeof(s),
                "Connection terminated: cl->%s, port->%d",
                client->ip,
                client->port);
        print_server(s);
        close(c->socket_fd);
        c->socket_fd = -1;
    }

    /* 2. Free dynamically allocated memory */
    if (c->tasks != NULL) {
        free(c->tasks);
        c->tasks = NULL;
    }

    /* 3. Mark the slot as inactive */
    c->active = 0;

    pthread_cond_signal(&roomAvailable);
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
    clients[free_position].satisfied = 0;
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

    
    pthread_mutex_unlock(&mutex);
    
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
        pthread_mutex_unlock(&mutex);
        return;
    }

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

void closing_broadcast_to_clients(){
    pthread_mutex_lock(&mutex);
    const char *shutdown_msg = "SERVER_SHUTDOWN: The server is shutting down now. Goodbye!\n";
    int msg_len = strlen(shutdown_msg);

    char *s = "SERVER_SHUTDOWN: beginning\n";
    print_server(s);

    for(int i = 0; i < MAX_THREADS; i++){
        printf("INSIDE\n");
        Client *current_client = &clients[i];
        printf("current client pos in buffer: %d, active %d\n", i, current_client->active);
        if(current_client->active == 0)
            continue;
        if(current_client->active == 1){
            send(current_client->socket_fd, shutdown_msg, msg_len, 0);
            rmvclient(current_client);
        }
    }

    s = "SERVER_SHUTDOWN: end\n";
    print_server(s);
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
        FD_ZERO(&read_fds);   // set all files to check to zero
        FD_SET(server_socket, &read_fds);  // add current potential client
        
        // timer
        struct timeval timeout;
        timeout.tv_sec = 3;
        timeout.tv_sec = 0;

        // IMP: check if server_socket has a client waiting in the connection queue
        int activity = select(server_socket + 1, &read_fds, NULL, NULL, &timeout); // server_soc + 1: highest fd + 1
        if(activity < 0){
            print_server_error("SELECT error");
        }
        // timer out
        if(activity == 0){
            continue;   // no new connection has arrived: 
                        //      SELECT takes out server_socket from read_fds
        }
        
        // activity > 0: new connection established!
        // NOTE socket readable iff a new client has completed the TCP handshake and is sitting in the connection queue.
        if(FD_ISSET(server_socket, &read_fds)){
            
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