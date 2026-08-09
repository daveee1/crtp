#ifndef UTILS_H    /* 1. Is UTILS_H NOT defined yet? */
#define UTILS_H    /* 2. OK, define UTILS_H now! */

#include <stdio.h>

#define COLOR_RED   "\033[1;31m"
#define COLOR_BLUE  "\033[1;34m"
#define COLOR_LIGHT_BLUE "\033[1;36m"  /* Bright Cyan / Light Blue */
#define COLOR_PURPLE     "\033[1;35m"  /* Bright Magenta / Purple */
#define COLOR_ORANGE     "\033[38;5;208m" /* 256-color Extended ANSI Orange */
#define COLOR_RESET "\033[0m"

void print_server_error(const char *error);
void print_client_error(const char *error);
void print_server(const char *s);
void print_client(const char *s);

#endif             /* 3. End of guard */