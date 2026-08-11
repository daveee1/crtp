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

Client clients[MAX_THREADS];
int free_position = 0;

pthread_mutex_t mutex;
pthread_cond_t roomAvailable, dataAvailable; // SEMAPHOREES: roomAvailable when there is space in the buffer, dataAvailable when buffer has clients already in it

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
    int exit_status = 0;
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
        receive(currSd, command, len);
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
            answer = strdup("closing server connection");
            exit_status = 1;
        }
        else 
            answer = strdup("invalid command (try help).");
            
        /* Convert to network byte order */
        len = strlen(answer);
        netLen = htonl(len);
        
        /* Send answer character length */
        if (send(currSd, &netLen, sizeof(netLen), 0) == -1){
            free(command);
            free(answer);
            break;
        }
        /* Send answer characters */
        if (send(currSd, answer, len, 0) == -1){
            free(command);
            free(answer);
            break;
        }
        
        free(command);
        free(answer);
        if (exit_status)  
            break;
    }
    
}

static void rmvclient(Client *client){
    pthread_mutex_lock(&mutex);

    int index = client->position;
    /* 1. Close the socket connection */
    if (clients[index].socket_fd >= 0) {
        char s[70];
        snprintf(s, sizeof(s),
                "Connection terminated: cl->%s, port->%d",
                client->ip,
                client->port);

        print_server(s);
        close(clients[index].socket_fd);
        clients[index].socket_fd = -1;
    }

    /* 2. Free dynamically allocated memory */
    if (clients[index].tasks != NULL) {
        free(clients[index].tasks);
        clients[index].tasks = NULL;
    }

    /* 3. Mark the slot as inactive */
    clients[index].active = 0;

    /* 4. Decrement active count or manage free slots */
    free_position--;

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
static void addclient(Client *client){
    
    // take the semaphore to write on 'clients' buffer
    pthread_mutex_lock(&mutex);
    
    // WAIT till 'clients' is full
    free_position = findFreePosition();
    while(free_position == -1){
        print_server_error("Max capacity reached. Waiting for space...\n");
        pthread_cond_wait(&roomAvailable, &mutex);
    }
    
    int free_pos = findFreePosition();
    clients[free_pos].active = 1;
    // since 'retSin.sin_addr' has the client id (in binary form)...
    clients[free_pos].port = client->port;
    clients[free_pos].socket_fd = client->socket_fd;
    // TODO clients[free_pos].tasks = (int*) malloc(ntasks*sizeof(int));
    strncpy(clients[free_pos].ip, client->ip, sizeof(clients[free_pos].ip) - 1);
    clients[free_pos].ip[sizeof(clients[free_pos].ip) - 1] = '\0';

    if(pthread_create(&clients[free_pos].thread_id, NULL, connectionHandler, &clients[free_pos]) == -1){
        print_server_error("pthread_create failed");
        clients[free_pos].active = 0;
    }
    
    pthread_mutex_unlock(&mutex);
    
    
    // debugging
    char s[100];
    snprintf(s, sizeof(s),
            "NEW CLIENT: cl->%s, port->%d",
            client->ip,
            client->port);
    print_server(s);
}

void init_client(Client *client, int current_socket, struct sockaddr_in *retSin) {
    client->active = 1;
    client->satisfied = 0;
    client->position = free_position;

    client->port = ntohs(retSin->sin_port);
    client->socket_fd = current_socket;

    inet_ntop(AF_INET, &retSin->sin_addr, client->ip, sizeof(client->ip));
}

int main(int argc, char *argv[]){
    int sock, port;
    int *currSock;
    int sAddrLen;
    struct sockaddr_in sin, retSin;

    pthread_t threads[MAX_THREADS];

    if(argc < 2){
        print_server_error("[SERVER] ERROR not enough arguments: specify PORT");
        exit(-1);
    }

    sscanf(argv[1], "%d", &port);
    
    if((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        print_server_error("socket not correct");
        exit(-1);
    }

    print_server("SOCKET ACTIVATED");
    // set REUSE ADDRESS option:
    // if i close the socket but there are still packets traversing 
    // then i wait till i receive all the traffic. ALSO even if
    // i close this socket create a new one on this port anyway
    int reuse = 1;
    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0)
        print_server_error("setsockopt(SO_REUSEADDR) failed");

    /* Initialize the address (struct sokaddr_in) fields */
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(port);
    
    // BIND socket to the port
    if(bind(sock, (struct sockaddr *) &sin, sizeof(sin)) == -1){
        print_server_error("BIND failed");
        exit(-1);
    }

    // TODO LISTEN how many clients?
    if(listen(sock, MAX_THREADS) == -1){
        print_server_error("LISTEN failed");
        exit(-1);
    }

    sAddrLen = sizeof(retSin);


    //ACCEPT connections
    // for(int i = 0; i < MAX_THREADS; i++)
    
    while(1)
    {
        currSock = (int*) malloc(sizeof(int));
        if((*currSock = accept(sock, (struct sockaddr *)&retSin, &sAddrLen)) == -1){
            print_server_error("ACCEPT failed");
            // exit(-1);
        }

        // initialize client structure
        Client client;
        init_client(&client, *currSock, &retSin);        
        addclient(&client);
    }

    return 0;
}