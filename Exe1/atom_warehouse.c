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

volatile sig_atomic_t timeout_flag = 0;
int server_fd;
int client_sockets[MAX_CLIENTS] = {0};


void alarm_handler(int sig) {
    (void)sig;
    timeout_flag = 1;
}

void cleanup_and_exit(int sig) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] > 0)
            close(client_sockets[i]);
    }
    if (server_fd > 0)
        close(server_fd);
    printf("\nServer exiting cleanly.\n");
    exit(0);
}

unsigned int hydrogen = 0;
unsigned int oxygen = 0;
unsigned int carbon = 0;

void handle_command(const char *cmd) {
    char type[16];
    unsigned int amount;

    if (sscanf(cmd, "ADD %15s %u", type, &amount) == 2) {
        // make type uppercase for case-insensitive comparison
        for (int i = 0; type[i]; i++) type[i] = toupper(type[i]);

        if (strcmp(type, "HYDROGEN") == 0) {
            hydrogen += amount;
        } else if (strcmp(type, "OXYGEN") == 0) {
            oxygen += amount;
        } else if (strcmp(type, "CARBON") == 0) {
            carbon += amount;
        } else {
            printf("ERROR: Unknown atom type '%s'\n", type);
            return;
        }
    } else {
        printf("ERROR: Invalid command '%s'\n", cmd);
        return;
    }

    printf("Inventory => HYDROGEN: %u, OXYGEN: %u, CARBON: %u\n", hydrogen, oxygen, carbon);
}

int main(int argc, char *argv[]) {
   signal(SIGINT, cleanup_and_exit);
    int opt;
    int port = 0;

    while ((opt = getopt(argc, argv, "p:")) != -1) {
        switch (opt) {
            case 'p':
                port = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s -p <port>\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (port <= 0) {
        fprintf(stderr, "Port must be specified and > 0\nUsage: %s -p <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    signal(SIGALRM, alarm_handler);

    int new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    fd_set readfds;
    char buffer[BUFFER_SIZE];

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Define address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Bind
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("atom_warehouse server started on port %d\n", port);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        int max_sd = server_fd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_sockets[i];
            if (sd > 0)
                FD_SET(sd, &readfds);
            if (sd > max_sd)
                max_sd = sd;
        }

        // Timeout for select (10 seconds)
        struct timeval tv;
        tv.tv_sec = 10;
        tv.tv_usec = 0;

        timeout_flag = 0;
        alarm(10); // set alarm for 10 sec

        int activity = select(max_sd + 1, &readfds, NULL, NULL, &tv);

        alarm(0); // cancel alarm if select returns early

        if (activity < 0 && !timeout_flag) {
            perror("select error");
            exit(EXIT_FAILURE);
        }

        if (timeout_flag) {
            // timeout happened
            printf("select timeout, no activity for 10 seconds\n");
            continue;
        }

        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }
            printf("New connection from %s:%d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = new_socket;
                    break;
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_sockets[i];
            if (sd > 0 && FD_ISSET(sd, &readfds)) {
                int valread = read(sd, buffer, BUFFER_SIZE - 1);
                if (valread == 0) {
                    close(sd);
                    client_sockets[i] = 0;
                    printf("Client disconnected.\n");
                } else if (valread > 0) {
                    buffer[valread] = '\0';
                    handle_command(buffer);
                }
            }
        }
    }

    return 0;
}
