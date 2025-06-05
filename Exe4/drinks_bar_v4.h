

#ifndef DRINKS_BAR_V4_H
#define DRINKS_BAR_V4_H

#include <stdint.h>

// Buffer size for I/O operations
#define BUFFER_SIZE 1024

// Maximum number of TCP clients
#define MAX_CLIENTS 30


extern uint64_t hydrogen;
extern uint64_t oxygen;
extern uint64_t carbon;

void print_inventory();
void cleanup_and_exit(int sig);

void handle_add_command(const char *cmd);

int deliver_molecules(const char *cmd);

void handle_console_command(const char *cmd);

void timeout_handler(int sig);

#endif // DRINKS_BAR_V4_H
