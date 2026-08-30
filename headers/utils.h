#ifndef UTILS_H    /* 1. Is UTILS_H NOT defined yet? */
#define UTILS_H    /* 2. OK, define UTILS_H now! */




#include <stdarg.h> // ex print_server("Port %d client %s thread %d", ...)
#include <stdio.h>
#include <pthread.h> // to 'mutex' output
#include <sys/select.h>
#include <sys/time.h>
#include <sys/socket.h>   /* Provides recv(), send(), socket(), accept() */
#include <netinet/in.h>   /* Provides struct sockaddr_in, htons(), ntohs() */
#include <arpa/inet.h>    /* Provides inet_ntop(), inet_pton() */

// server
#define COLOR_RED   "\033[1;31m"
#define COLOR_YELLOW "\033[1;33m" 
#define COLOR_ORANGE     "\033[38;5;208m" /* 256-color Extended ANSI Orange */

// client
#define COLOR_BLUE  "\033[1;34m"
#define COLOR_LIGHT_BLUE "\033[1;36m"  /* Bright Cyan / Light Blue */
#define COLOR_PURPLE     "\033[1;35m"  /* Bright Magenta / Purple */

#define COLOR_RESET "\033[0m"

// #define COLOR_LIGHT_GREEN  "\033[1;32m" 
#define COLOR_LIGHT_GREEN "\033[1;32m" 
#define COLOR_LIGHT_GREEN "\033[1;32m" 
// #define COLOR_CYAN "\033[0;36m" // print active tasks


void print_server_error(const char *format, ...);
void print_server(const char *format, ...);
void print_server_warning(const char *format, ...);

void print_client_error(const char *format, ...);
void print_client(const char *format, ...);
void print_client_warning(const char *format, ...);


void print_server_error_unlocked(const char *format, ...);
void print_server_unlocked(const char *format, ...);

void print_client_unlocked(const char *format, ...);


void print_analysis(const char *format, ...);
void print_printingTASK(const char *format, ...);

int receive(int sd, char *retBuf, int size);

extern int verbose;
extern pthread_mutex_t log_mutex;


#endif             /* 3. End of guard */