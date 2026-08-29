#include "headers/utils.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>



#define FALSE 0
#define TRUE 1

/*
    Return -1 if : error in receive or server's ans = 'SERVER_SHUTDOWN'
    Return 1 if: otherwise
    */
static int handle_server_answer(int socket, int port_client){
    unsigned int netLen;
    int len;
    
    /* 1. Receive message length */
    if (receive(socket, (char *)&netLen, sizeof(netLen)) < 0) {
        print_client("Server closed connection");
        return -1; /* Connection lost */
    }

    len = ntohl(netLen);


    // Sanity check length against maximum expected command size
    if (len <= 0 || len > 256) {
        print_server_error("Invalid payload length received: %d\n", len);
        // close(socket); will be closed outside main loop
        return -1;
    }


    char *answer = malloc(len + 1);
    if (!answer) return -1;

    /* 2. Receive message text */
    if (receive(socket, answer, len) < 0) {
        free(answer);
        print_client_error("Failed to receive full message from server.");
        return -1;
    }
    answer[len] = '\0';
    print_client("SERVER answer to %d is: %s", port_client, answer);

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
    if ( !strcmp(input, "a 1") || !strcmp(input, "activate 1")) return CLIENT_CMD_ACTIVATE_TASK1;
    else if (!strcmp(input, "a 2") || !strcmp(input, "activate 2")) return CLIENT_CMD_ACTIVATE_TASK2;
    else if (!strcmp(input, "a 3") || !strcmp(input, "activate 3")) return CLIENT_CMD_ACTIVATE_TASK3;
    else if (!strcmp(input, "a 4") || !strcmp(input, "activate 4")) return CLIENT_CMD_ACTIVATE_TASK4;
    else if (!strcmp(input, "b 1") || !strcmp(input, "block 1")) return CLIENT_CMD_DEACTIVATE_TASK1;
    else if (!strcmp(input, "b 2") || !strcmp(input, "block 2")) return CLIENT_CMD_DEACTIVATE_TASK2;
    else if (!strcmp(input, "b 3") || !strcmp(input, "block 3")) return CLIENT_CMD_DEACTIVATE_TASK3;
    else if (!strcmp(input, "b 4") || !strcmp(input, "block 4")) return CLIENT_CMD_DEACTIVATE_TASK4;
    else if (!strcmp(input, "help")) return CLIENT_CMD_HELP;
    else if (!strcmp(input, "quit") || !strcmp(input, "exit")) return CLIENT_CMD_QUIT;
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
        case CLIENT_CMD_DEACTIVATE_TASK1: return "BLOCK 1";
        case CLIENT_CMD_DEACTIVATE_TASK2: return "BLOCK 2";
        case CLIENT_CMD_DEACTIVATE_TASK3: return "BLOCK 3";
        case CLIENT_CMD_DEACTIVATE_TASK4: return "BLOCK 4";
        case CLIENT_CMD_QUIT:  return "quit";
        case CLIENT_CMD_STOP:  return "stop";
        default:               return "help";
    }
}




// Helper to retrieve the ephemeral port assigned to this client socket
int get_my_client_port(int socket_fd) {
    struct sockaddr_in local_addr;
    socklen_t addr_len = sizeof(local_addr);
    if (getsockname(socket_fd, (struct sockaddr *)&local_addr, &addr_len) == 0) {
        return ntohs(local_addr.sin_port); // Returns port like 57820
    }
    return -1;
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
    int client_socket;
    int server_port;
    int len;
    unsigned int netLen;
    int my_client_port = 0;
    struct sockaddr_in sin;
    struct hostent *hp;

    /* Check number of arguments and get IP address, port and verbose flags*/
    if (argc < 4)
    {
        print_client_error("Usage: <ip_hostname> <port> <verbose>\n");
        exit(0);
    }

    // Parse hostname (argv[1])
    if (sscanf(argv[1], "%s", hostname) != 1) {
        print_client_error("Invalid hostname");
        exit(EXIT_FAILURE);
    }

    // get port
    if (sscanf(argv[2], "%d", &server_port) != 1 || server_port <= 0 || server_port > 65535) {
        print_client_error("Invalid port number");
        exit(EXIT_FAILURE);
    }

    // Parse VERBOSE 
    if (sscanf(argv[3], "%d", &verbose) == 1 && (verbose > 0 ))
        print_client(" Verbose set to %d", verbose);
    else 
        print_client(" Invalid verbose input, defaulting to 0");



    /* Resolve the passed name and store the resulting long representation
    in the struct hostent variable */
    if ((hp = gethostbyname(hostname)) == 0)
    {
        print_client_error(" ERROR gethostbyname");
        exit(1);
    }

    /* fill in the socket structure with host information */
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr_list[0]))->s_addr;
    sin.sin_port = htons(server_port);
    
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
        print_client_error(" ERROR connect");
        exit(1);
    }

    my_client_port = get_my_client_port(client_socket);
    print_client("[%d] Enter command:", my_client_port);

    while(1)
    {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        
        FD_SET(STDIN_FILENO, &read_fds);  // if keyboard detects input by user
        FD_SET(client_socket, &read_fds);

        fflush(stdout); /* Force prompt to display without waiting for newline */
        
        
        int max_fd = (client_socket > STDIN_FILENO) ? client_socket : STDIN_FILENO; //necessary to select to know highest socket
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if(activity < 0){
            print_client_error(" [%d] SELECT error", my_client_port);
            exit(1);
        }
        

        //1) activity detected: server answered!
        if(FD_ISSET(client_socket, &read_fds)){
            if(handle_server_answer(client_socket,my_client_port) == -1)  // SERVER ANS: if server stopped or it closed our client EXIT from loop
                break;    
            print_client(" [%d] enter command: request task {1-4}", my_client_port);
            fflush(stdout);
        }

        //2) input by user detected or bash file detected
        if(FD_ISSET(STDIN_FILENO, &read_fds) > 0){
            char input[50];

            if (fgets(input, sizeof(input), stdin) == NULL){
                print_client("[%d] STDIN reached EOF (script finished). Waiting for server answers...", my_client_port);
                // continue; // reached EOF, now we wait select option 1 (server answers)
                exit(1);
            }
            int len = (int)strlen(input);
            input[len - 1] = '\0'; // if input length >= 50 i dont care

            ClientCmdType cmd;  // manages the right request to the server
            cmd = parse_user_input(input);
            /* Filter out invalid commands early */
            if (cmd == CLIENT_CMD_INVALID) {
                print_client_warning("[%d] Invalid choice! Enter a/b 1-4 for tasks or 'help', 'quit', 'stop'.", my_client_port);
                continue;
            }

            const char *command = format_server_payload(cmd);

            // send command lenght
            len = strlen(command);
            unsigned int netLen = htonl(len);
            if(send(client_socket, &netLen, sizeof(netLen), 0) < 0){
                print_client_error("[%d] SENDing request # of chars to server", my_client_port);
                break;
            }
            if(send(client_socket, command, len, 0) < 0){
                print_client_error("[%d] SENDing command to server", my_client_port);
                break;
            }
        }

    }
    /* Close the socket */
    close(client_socket);
    return 0;
}
