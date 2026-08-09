#include "headers/server.h"



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
        if (send(currSd, &netLen, sizeof(netLen), 0) == -1)
            break;
        /* Send answer characters */
        if (send(currSd, answer, len, 0) == -1)
            break;
        free(command);
        free(answer);
        if (exit_status)  
            break;
    }
    /* The loop is most likely exited when the connection is terminated */
    printf("Connection terminated\n");
    close(currSd);
}

void print_error(char *error){
    printf("%s", error);
}

int main(int argc, char *argv[]){
    int sock, port, currSock;
    int nclients = 5;
    int sAddrLen;
    struct sockaddr_in sin, retSin;

    if(argc < 2){
        print_error("[SERVER] ERROR not enough arguments: specify PORT");
        exit(-1);
    }

    sscanf(argv[1], "%d", &port);
    
    if((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        print_error("[SERVER] socket not correct");
        exit(-1);
    }

    // set REUSE ADDRESS option
    int reuse = 1;
    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0)
        print_error("[SERVER] ERROR setsockopt(SO_REUSEADDR) failed");

    /* Initialize the address (struct sokaddr_in) fields */
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(port);
    
    // BIND socket to the port
    if(bind(sock, (struct sockaddr *) &sin, sizeof(sin)) == -1){
        print_error("[SERVER] ERROR bind failed");
        exit(-1);
    }

    // TODO LISTEN how many clients?
    if(listen(sock, nclients) == -1){
        print_error("[SERVER] ERROR listen failed");
        exit(-1);
    }

    sAddrLen = sizeof(retSin);


    //ACCEPT connections
    while(1)
    {
        //TODO error or just full?
        if((currSock = accept(sock, (struct sockaddr *) &retSin, &sAddrLen)) == -1){
            print_error("[SERVER] ERROR ACCEPT failed");
            exit(-1);
        }
        handleConnection(currSock);
    }
}