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



typedef enum {
    CLIENT_CMD_ACTIVATE_TASK1,
    CLIENT_CMD_ACTIVATE_TASK2,
    CLIENT_CMD_ACTIVATE_TASK3,
    CLIENT_CMD_ACTIVATE_TASK4,
    CLIENT_CMD_DEACTIVATE_TASK1,
    CLIENT_CMD_DEACTIVATE_TASK2,
    CLIENT_CMD_DEACTIVATE_TASK3,
    CLIENT_CMD_DEACTIVATE_TASK4,
    CLIENT_CMD_HELP,
    CLIENT_CMD_QUIT,
    CLIENT_CMD_STOP,
    CLIENT_CMD_INVALID
} ClientCmdType;

/* Parse raw user input into an enum */
static ClientCmdType parse_user_input(const char *input) {
    if ( !strcmp(input, "act 1") || !strcmp(input, "activate 1")) return CLIENT_CMD_ACTIVATE_TASK1;
    else if (!strcmp(input, "act 2") || !strcmp(input, "activate 2")) return CLIENT_CMD_ACTIVATE_TASK2;
    else if (!strcmp(input, "act 3") || !strcmp(input, "activate 3")) return CLIENT_CMD_ACTIVATE_TASK3;
    else if (!strcmp(input, "act 4") || !strcmp(input, "activate 4")) return CLIENT_CMD_ACTIVATE_TASK4;
    else if (!strcmp(input, "deactivate 1") || !strcmp(input, "deac 1")) return CLIENT_CMD_DEACTIVATE_TASK1;
    else if (!strcmp(input, "deactivate 2") || !strcmp(input, "deac 2")) return CLIENT_CMD_DEACTIVATE_TASK2;
    else if (!strcmp(input, "deactivate 3") || !strcmp(input, "deac 3")) return CLIENT_CMD_DEACTIVATE_TASK3;
    else if (!strcmp(input, "deactivate 4") || !strcmp(input, "deac 4")) return CLIENT_CMD_DEACTIVATE_TASK4;
    else if (!strcmp(input, "help")) return CLIENT_CMD_HELP;
    else if (!strcmp(input, "quit")) return CLIENT_CMD_QUIT;
    else if (!strcmp(input, "stop")) return CLIENT_CMD_STOP;
    return CLIENT_CMD_INVALID;
}

/* Format the payload to send over TCP */
static const char* format_server_payload(ClientCmdType cmd) {
    switch (cmd) {
        case CLIENT_CMD_ACTIVATE_TASK1: return "ACTIVATE 1";
        case CLIENT_CMD_ACTIVATE_TASK2: return "ACTIVATE 2";
        case CLIENT_CMD_ACTIVATE_TASK3: return "ACTIVATE 3";
        case CLIENT_CMD_ACTIVATE_TASK4: return "ACTIVATE 4";
        case CLIENT_CMD_DEACTIVATE_TASK1: return "DEACTIVATE 1";
        case CLIENT_CMD_DEACTIVATE_TASK2: return "DEACTIVATE 2";
        case CLIENT_CMD_DEACTIVATE_TASK3: return "DEACTIVATE 3";
        case CLIENT_CMD_DEACTIVATE_TASK4: return "DEACTIVATE 4";
        case CLIENT_CMD_HELP:  return "help";
        case CLIENT_CMD_QUIT:  return "quit";
        case CLIENT_CMD_STOP:  return "stop";
        default:               return NULL;
    }
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
        }
        

        //1) activity detected: server answered! or...
        if(FD_ISSET(client_socket, &read_fds)){
            if(handle_server_answer(client_socket) < 0)
                break;
            print_client("enter command: request task {1-4}");
            fflush(stdout);
        }

        //2) input by user detected
        if(FD_ISSET(STDIN_FILENO, &read_fds) > 0){
            printf("> ");
            
            char input[50];
            if (fgets(input, sizeof(input), stdin) == NULL){
                print_client_error("fgets()");
                exit(1);
            }
            int len = (int)strlen(input);
            input[len - 1] = '\0'; // if input length >= 50 i dont care

            ClientCmdType cmd;  // manages the right request to the server
            cmd = parse_user_input(input);
            /* Filter out invalid commands early */
            if (cmd == CLIENT_CMD_INVALID) {
                print_client_warning("Invalid choice! Enter 1-4 for tasks or 'help', 'quit', 'stop'.");
                continue; // Don't send anything to server, prompt again
            }
            
            const char *command = format_server_payload(cmd);
            printf("COMMAND %s", command);
            // send command lenght
            len = strlen(command);
            unsigned int netLen = htonl(len);
            if(send(client_socket, &netLen, sizeof(netLen), 0) < 0){
                print_client_error("SENDing request # of chars to server");
                break;
            }
            if(send(client_socket, command, len, 0) < 0){
                print_client_error("SENDing command to server");
                break;
            }

        }

    }
    /* Close the socket */
    close(client_socket);
    return 0;
}
