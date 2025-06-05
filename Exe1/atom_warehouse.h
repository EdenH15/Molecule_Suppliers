#ifndef ATOM_WAREHOUSE_H
#define ATOM_WAREHOUSE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <signal.h>
#include <ctype.h>
#include <getopt.h>

#define MAX_CLIENTS 30
#define BUFFER_SIZE 1024

extern volatile sig_atomic_t timeout_flag;
extern int server_fd;
extern int client_sockets[MAX_CLIENTS];

extern uint64_t hydrogen;
extern uint64_t oxygen;
extern uint64_t carbon;

void alarm_handler(int sig);
void cleanup_and_exit(int sig);
void handle_command(const char *cmd);

#endif // ATOM_WAREHOUSE_H
