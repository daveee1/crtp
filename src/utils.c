#include "headers/utils.h"


/* Red error output for Server */
void print_server_error(const char *error) {
    fprintf(stderr, "%s[SERVER ERROR] %s%s\n", COLOR_RED, error, COLOR_RESET);
}

/* Blue error output for Client */
void print_client_error(const char *error) {
    fprintf(stderr, "%s[CLIENT ERROR] %s%s\n", COLOR_BLUE, error, COLOR_RESET);
}

void print_server(const char *s) {
    printf("%s[SERVER] %s%s\n", COLOR_ORANGE, s, COLOR_RESET);
}
void print_client(const char *s) {
    printf("%s[CLIENT] %s%s\n", COLOR_LIGHT_BLUE, s, COLOR_RESET);
}