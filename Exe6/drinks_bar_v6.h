#ifndef DRINKS_BAR_V6_H
#define DRINKS_BAR_V6_H

#include <stddef.h>

extern struct Inventory inventory;
extern int tcp_socket;
extern int udp_socket;
extern int timed_out;
extern int timeout_seconds;
extern char *save_file_path;


void print_inventory();
void cleanup_and_exit(int sig);

void alarm_handler(int sig);

int load_inventory_from_file(const char *filename);

int save_inventory_to_file(const char *filename);

int process_tcp_command(const char *cmd, char *response, size_t resp_len);

int process_udp_command(const char *cmd, char *response, size_t resp_len);

int process_console_command(const char *cmd, char *response, size_t resp_len);

void cleanup();

int main(int argc, char *argv[]);

#endif // DRINKS_BAR_V6_H