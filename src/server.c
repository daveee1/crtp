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
    
    for(;;)
    {
        /* Get the command string length
        If receive fails, the client most likely exited */
        if(receive(currSd, (char *)&netLen, sizeof(netLen)))    break;
        /* Convert from network byte order */
        len = ntohl(netLen);
        command = malloc(len+1);
        /* Get the command and write terminator */
        receive(currSd, command, len);
        command[len] = 0;
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
            /* Send the answer back */
            len = strlen(answer);
            /* Convert to network byte order */
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
    int socket;

    if(argc < 2){
        print_error("Bad potato");
    }


}