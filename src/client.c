#include "headers/server.h"

#define FALSE 0
#define TRUE 1

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

/* Main client program. The IP address and the port number of
   the server are passed in the command line. After establishing
   a connection, the program will read commands from the terminal
   and send them to the server. The returned answer string is
   then printed. */
int main(int argc, char **argv)
{
    char hostname[100];
    char command[256];
    char *answer;
    int  sd;
    int port;
    int stopped = FALSE;
    int len;
    unsigned int netLen;
    struct sockaddr_in sin;
    struct hostent *hp;
    /* Check number of arguments and get IP address and port */
    if (argc < 3)
    {
        printf("Usage: <ip_hostname> <port>\n");
        exit(0);
    }
    sscanf(argv[1], "%s", hostname);
    sscanf(argv[2], "%d", &port);

    /* Resolve the passed name and store the resulting long representation
    in the struct hostent variable */
    if ((hp = gethostbyname(hostname)) == 0)
    {
        print_client_error("ERROR gethostbyname");
        exit(1);
    }

    /* fill in the socket structure with host information */
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr_list[0]))->s_addr;
    sin.sin_port = htons(port);
    
    /* create a new socket */
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        print_client_error("ERROR socket");
        exit(1);
    }
    /* connect the socket to the port and host
    specified in struct sockaddr_in */
    if (connect(sd,(struct sockaddr *)&sin, sizeof(sin)) == -1)
    {
        print_client_error("ERROR connect");
        exit(1);
    }

    while(!stopped)
    {
        /* Get a string command from terminal */
        printf("Enter command: ");
        if(!scanf("%s", command))
            print_client_error("SCANF ERROR sending request");
        if(!strcmp(command, "quit")) break;
        
        /* Send first the number of characters in the command and then
        the command itself */
        len = strlen(command);
        
        /* Convert the integer number into network byte order */
        netLen = htonl(len);

        /* Send number of characters */
        if(send(sd, &netLen, sizeof(netLen), 0) == -1)
        {
            print_client_error("ERROR send number of characters");
            exit(1);
        }
        
        /* Send the command */
        if (send(sd, command, len, 0) == -1)
        {
            print_client_error("ERROR send command");
            exit(0);
        }
        
        /* Receive the answer: first the number of characters
        and then the answer itself */
        if(receive(sd, (char *)&netLen, sizeof(netLen)))
        {
            print_client_error("ERROR recv number of characters");
            exit(0);
        }
        
        /* Convert from Network byte order */
        len = ntohl(netLen);
        /* Allocate and receive the answer */
        answer = malloc(len + 1);
        if(receive(sd, answer, len))
        {
            print_client_error("ERROR recv answer");
            exit(1);
        }
        answer[len] = 0;
        printf("%s\n", answer);
        free(answer);
        if(!strcmp(command, "stop")) break;
    }
    /* Close the socket */
    close(sd);
    return 0;
}
