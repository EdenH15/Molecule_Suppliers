// drinks_bar.h
#ifndef DRINKS_BAR_H
#define DRINKS_BAR_H

#include <stdint.h>

void print_inventory();
void cleanup_and_exit(int sig);
void handle_add_command(const char *cmd);
int deliver_molecules(const char *cmd);
void handle_console_command(const char *cmd);
void timeout_handler(int sig);

#endif // DRINKS_BAR_H
