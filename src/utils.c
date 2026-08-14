#include "headers/utils.h"


/* Red error output for Server */
void print_server_error(const char *error) {
    fprintf(stderr, "%s[SERVER ERROR] %s%s\n", COLOR_RED, error, COLOR_RESET);
}
void print_server(const char *s) {
    printf("%s[SERVER] %s%s\n", COLOR_ORANGE, s, COLOR_RESET);
}
void print_server_warning(const char *s) {
    printf("%s[SERVER] %s%s\n", COLOR_YELLOW, s, COLOR_RESET);
}

/* Blue error output for Client */
void print_client_error(const char *error) {
    fprintf(stderr, "%s[CLIENT ERROR] %s%s\n", COLOR_BLUE, error, COLOR_RESET);
}
void print_client(const char *s) {
    printf("%s[CLIENT] %s%s\n", COLOR_LIGHT_BLUE, s, COLOR_RESET);
}
void print_client_warning(const char *s) {
    printf("%s[CLIENT WARNING] %s%s\n", COLOR_PURPLE, s, COLOR_RESET);
}

/* Receive routine: use recv to receive from socket and manage
   the fact that recv may return after having read less bytes than
   the passed buffer size
   In most cases recv will read ALL requested bytes, and the loop body
   will be executed once. This is not however guaranteed and must
   be handled by the user program. The routine returns 0 upon
   successful completion, -1 otherwise */
int receive(int sd, char *retBuf, int size){
    
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