#ifndef DRINKS_BAR_V6_H
#define DRINKS_BAR_V6_H

#include <netinet/in.h>
#include <sys/un.h>

#define BUFFER_SIZE 1024
#define MAX_CLIENTS 100
#define MAX_PATH 108

// Atom inventory (shared between functions)
extern uint64_t hydrogen;
extern uint64_t oxygen;
extern uint64_t carbon;

typedef struct {
    int water;
    int oxygen;
    int sugar;
} Inventory;

extern Inventory *inventory;
extern int inventory_fd;
extern const char *save_file;

void print_inventory();

void load_or_create_inventory(const char *path, int default_water, int default_oxygen, int default_sugar);
void update_inventory(const char *item, int amount);

void handle_client(int client_sock);

int create_tcp_socket(int port);
int create_udp_socket(int port);
int create_uds_socket(const char *path, int type);

#endif // DRINKS_BAR_V6_H
