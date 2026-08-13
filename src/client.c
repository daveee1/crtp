#include "headers/server.h"

#define FALSE 0
#define TRUE 1

/*
    Return -1 if : error in receive or server's ans = 'SERVER_SHUTDOWN'
    Return 1 if: otherwise
    */
static int handle_server_answer(int socket){
    unsigned int netLen;
    int len;
    
    /* 1. Receive message length */
    if (receive(socket, (char *)&netLen, sizeof(netLen)) < 0) {
        print_client("Server closed connection");
        return -1; /* Connection lost */
    }

    len = ntohl(netLen);
    char *answer = malloc(len + 1);
    if (!answer) return -1;

    /* 2. Receive message text */
    if (receive(socket, answer, len) < 0) {
        free(answer);
        print_client_error("Failed to receive full message from server.");
        return -1;
    }
    answer[len] = '\0';
    print_client(answer);

    if(!strcmp(answer, "SERVER_SHUTDOWN")){
        free(answer);
        return -1;
    }
    
    free(answer);
    return 1; // server sent other information
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
    int  client_socket;
    int port;
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
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        print_client_error("ERROR socket");
        exit(1);
    }
    
    /* connect the socket to the port and host
    specified in struct sockaddr_in */
    if (connect(client_socket,(struct sockaddr *)&sin, sizeof(sin)) == -1)
    {
        print_client_error("ERROR connect");
        exit(1);
    }

    while(1)
    {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        
        FD_SET(STDIN_FILENO, &read_fds);  // if keyboard detects input by user
        FD_SET(client_socket, &read_fds);

        fflush(stdout); /* Force prompt to display without waiting for newline */
        
        int max_fd = (client_socket > STDIN_FILENO) ? client_socket : STDIN_FILENO;
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if(activity < 0){
            print_client_error("SELECT error");
            exit(1);
            print_client("Enter command: ");
        }
        



        //1) activity detected: server answered! or...
        if(FD_ISSET(client_socket, &read_fds)){
            if(handle_server_answer(client_socket) < 0)
                break;
            print_client("enter command");
            fflush(stdout);
        }

        //2) input by user detected
        if(FD_ISSET(STDIN_FILENO, &read_fds) > 0){
            char command[80];
            if(scanf("%s", command) < 0){
                print_client_error("scanf error in STDIN_FILENO");
                break;
            }

            // send command lenght
            int len = strlen(command);
            unsigned int netLen = htonl(len);

            if(send(client_socket, &netLen, sizeof(netLen), 0) < 0){
                print_client_error("SENDing request # of chars to server");
                break;
            }
            if(send(client_socket, &command, len, 0) < 0){
                print_client_error("SENDing command to server");
                break;
            }

            // TODO handle ans based on tasks
        }

    }
    /* Close the socket */
    close(client_socket);
    return 0;
}
