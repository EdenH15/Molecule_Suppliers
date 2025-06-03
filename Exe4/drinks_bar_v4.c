// drinks_bar_v4.c
// Created by eden on 6/3/25

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <strings.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <errno.h>
#include "drinks_bar_v4.h"

#define MAX_CLIENTS 30
#define BUFFER_SIZE 1024

// Atom inventory - מתחילים מ-0
uint64_t hydrogen = 0, oxygen = 0, carbon = 0;

int timeout_seconds = 60;  // ברירת מחדל ל-timeout, אפשר לשנות מהקונפיג

/**
 * Prints the current atom inventory.
 */
void print_inventory() {
    printf("Inventory => HYDROGEN: %lu, OXYGEN: %lu, CARBON: %lu\n", hydrogen, oxygen, carbon);
    fflush(stdout);
}

/**
 * Handles "ADD <ATOM> <AMOUNT>" commands from TCP clients.
 */
void handle_add_command(const char *cmd) {
    char type[16];
    uint64_t amount;

    if (sscanf(cmd, "ADD %15s %lu", type, &amount) == 2) {
        if (strcasecmp(type, "HYDROGEN") == 0) hydrogen += amount;
        else if (strcasecmp(type, "OXYGEN") == 0) oxygen += amount;
        else if (strcasecmp(type, "CARBON") == 0) carbon += amount;
        else printf("ERROR: Unknown atom type '%s'\n", type);
    } else {
        printf("ERROR: Invalid ADD command '%s'\n", cmd);
    }
    print_inventory();
}

/**
 * Handles "DELIVER <MOLECULE> <AMOUNT>" commands from UDP clients.
 *
 * Supported molecules:
 * - WATER = H2O
 * - CARBON DIOXIDE = CO2
 * - GLUCOSE = C6H12O6
 * - ALCOHOL = C2H6O
 *
 * Returns:
 *  -  1 on successful delivery
 *  -  0 if there are insufficient atoms
 *  - -1 for invalid command syntax
 *  - -2 for unknown molecule name
 */
int deliver_molecules(const char *cmd) {
    const char *prefix = "DELIVER ";
    if (strncmp(cmd, prefix, strlen(prefix)) != 0) return -1;

    const char *args = cmd + strlen(prefix);
    char mol_full[64];
    strncpy(mol_full, args, sizeof(mol_full) - 1);
    mol_full[sizeof(mol_full) - 1] = '\0';

    char *last_space = strrchr(mol_full, ' ');
    if (!last_space) return -1;

    *last_space = '\0';
    const char *mol = mol_full;
    const char *amount_str = last_space + 1;

    int amount = atoi(amount_str);
    if (amount <= 0) return -1;

    uint64_t need_H = 0, need_O = 0, need_C = 0;

    if (strcasecmp(mol, "WATER") == 0) {
        need_H = 2 * amount;
        need_O = 1 * amount;
    } else if (strcasecmp(mol, "CARBON DIOXIDE") == 0) {
        need_C = 1 * amount;
        need_O = 2 * amount;
    } else if (strcasecmp(mol, "GLUCOSE") == 0) {
        need_C = 6 * amount;
        need_H = 12 * amount;
        need_O = 6 * amount;
    } else if (strcasecmp(mol, "ALCOHOL") == 0) {
        need_C = 2 * amount;
        need_H = 6 * amount;
        need_O = 1 * amount;
    } else {
        return -2;
    }

    if (hydrogen >= need_H && oxygen >= need_O && carbon >= need_C) {
        hydrogen -= need_H;
        oxygen -= need_O;
        carbon -= need_C;
        print_inventory();
        return 1;
    } else {
        return 0;
    }
}

/**
 * Handles console commands (via stdin).
 * Supported commands:
 * - GEN SOFT DRINK
 * - GEN VODKA
 * - GEN CHAMPAGNE
 *
 * Each command prints how many drinks of the requested type can be produced.
 */
void handle_console_command(const char *cmd) {
    int soft = 0, vodka = 0, champagne = 0;

    if (strcasecmp(cmd, "GEN SOFT DRINK") == 0) {
        int water = hydrogen / 2 < oxygen ? hydrogen / 2 : oxygen;
        int co2 = carbon < oxygen / 2 ? carbon : oxygen / 2;
        soft = water < co2 ? water : co2;
        printf("SOFT DRINKS that can be made: %d\n", soft);
    } else if (strcasecmp(cmd, "GEN VODKA") == 0) {
        int water = hydrogen / 2 < oxygen ? hydrogen / 2 : oxygen;
        int alcohol = hydrogen / 6 < carbon / 2 ? hydrogen / 6 : carbon / 2;
        int glucose;
        {
            int c6 = carbon / 6;
            int h12 = hydrogen / 12;
            int o6 = oxygen / 6;
            glucose = c6 < h12 ? c6 : h12;
            glucose = glucose < o6 ? glucose : o6;
        }
        vodka = water;
        if (vodka > alcohol) vodka = alcohol;
        if (vodka > glucose) vodka = glucose;
        printf("VODKA that can be made: %d\n", vodka);
    } else if (strcasecmp(cmd, "GEN CHAMPAGNE") == 0) {
        int water = hydrogen / 2 < oxygen ? hydrogen / 2 : oxygen;
        int co2 = carbon < oxygen / 2 ? carbon : oxygen / 2;
        int glucose;
        {
            int c6 = carbon / 6;
            int h12 = hydrogen / 12;
            int o6 = oxygen / 6;
            glucose = c6 < h12 ? c6 : h12;
            glucose = glucose < o6 ? glucose : o6;
        }
        champagne = water;
        if (champagne > co2) champagne = co2;
        if (champagne > glucose) champagne = glucose;
        printf("CHAMPAGNE that can be made: %d\n", champagne);
    } else {
        printf("Unknown console command: %s\n", cmd);
    }

    fflush(stdout);
}

/**
 * Signal handler for SIGALRM.
 * Used to interrupt select() calls to prevent blocking indefinitely.
 */
void timeout_handler(int sig) {
    // Do nothing, just interrupt select
}

int main(int argc, char *argv[]) {
    int tcp_port = -1;
    int udp_port = -1;
    int opt;
    struct option long_options[] = {
        {"oxygen",    required_argument, NULL, 'o'},
        {"carbon",    required_argument, NULL, 'c'},
        {"hydrogen",  required_argument, NULL, 'h'},
        {"timeout",   required_argument, NULL, 't'},
        {"tcp-port",  required_argument, NULL, 'T'},
        {"udp-port",  required_argument, NULL, 'U'},
        {0, 0, 0, 0}
    };

    // אתחול אטומים ל-0 כבר ב-global, אבל אפשר להגדיר פה במידה ורוצים
    hydrogen = 0;
    oxygen = 0;
    carbon = 0;

    while ((opt = getopt_long(argc, argv, "o:c:h:t:T:U:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'o':
                oxygen = strtoull(optarg, NULL, 10);
                break;
            case 'c':
                carbon = strtoull(optarg, NULL, 10);
                break;
            case 'h':
                hydrogen = strtoull(optarg, NULL, 10);
                break;
            case 't':
                timeout_seconds = atoi(optarg);
                if (timeout_seconds <= 0) {
                    fprintf(stderr, "Timeout must be positive integer\n");
                    exit(EXIT_FAILURE);
                }
                break;
            case 'T':
                tcp_port = atoi(optarg);
                break;
            case 'U':
                udp_port = atoi(optarg);
                break;
            default:
                fprintf(stderr,
                        "Usage: %s -T <tcp-port> -U <udp-port> [-o oxygen] [-c carbon] [-h hydrogen] [-t timeout]\n",
                        argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (tcp_port <= 0 || udp_port <= 0) {
        fprintf(stderr,
                "Error: both TCP (-T) and UDP (-U) ports are required and must be > 0\n");
        fprintf(stderr,
                "Usage: %s -T <tcp-port> -U <udp-port> [-o oxygen] [-c carbon] [-h hydrogen] [-t timeout]\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }

    signal(SIGALRM, timeout_handler);

    int tcp_fd, udp_fd, client_sockets[MAX_CLIENTS] = {0};
    struct sockaddr_in server_addr, client_addr;
    socklen_t addrlen = sizeof(client_addr);
    fd_set readfds;
    char buffer[BUFFER_SIZE];

    tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
    udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (tcp_fd < 0 || udp_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int sock_opt = 1;
    setsockopt(tcp_fd, SOL_SOCKET, SO_REUSEADDR, &sock_opt, sizeof(sock_opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // TCP bind and listen
    server_addr.sin_port = htons(tcp_port);
    if (bind(tcp_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind tcp");
        exit(EXIT_FAILURE);
    }
    if (listen(tcp_fd, 5) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // UDP bind
    server_addr.sin_port = htons(udp_port);
    if (bind(udp_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind udp");
        exit(EXIT_FAILURE);
    }

    printf("drinks_bar server started on TCP port %d and UDP port %d\n", tcp_port, udp_port);
    printf("Starting inventory => HYDROGEN: %lu, OXYGEN: %lu, CARBON: %lu\n", hydrogen, oxygen, carbon);

    while (1) {
        // Timeout עבור select עם alarm
        alarm(timeout_seconds);

        FD_ZERO(&readfds);
        FD_SET(tcp_fd, &readfds);
        FD_SET(udp_fd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);

        int max_sd = STDIN_FILENO > tcp_fd ? STDIN_FILENO : tcp_fd;
        if (udp_fd > max_sd) max_sd = udp_fd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_sockets[i];
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if (activity < 0) {
            if (errno == EINTR) {
                // Timeout expired - אין קלט בזמן - סוגרים
                printf("Timeout expired - no input detected, shutting down server.\n");
                break;
            } else {
                perror("select");
                continue;
            }
        }

        // Console command from stdin
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            if (fgets(buffer, sizeof(buffer), stdin)) {
                buffer[strcspn(buffer, "\n")] = '\0';
                handle_console_command(buffer);
            }
        }

        // New TCP client connection
        if (FD_ISSET(tcp_fd, &readfds)) {
            int new_socket = accept(tcp_fd, (struct sockaddr *)&client_addr, &addrlen);
            if (new_socket >= 0) {
                printf("New TCP connection from %s:%d\n",
                       inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                int added = 0;
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (client_sockets[i] == 0) {
                        client_sockets[i] = new_socket;
                        added = 1;
                        break;
                    }
                }
                if (!added) {
                    printf("Too many clients connected, rejecting new connection\n");
                    close(new_socket);
                }
            }
        }

        // Read from TCP clients
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_sockets[i];
            if (sd > 0 && FD_ISSET(sd, &readfds)) {
                int valread = read(sd, buffer, BUFFER_SIZE - 1);
                if (valread <= 0) {
                    close(sd);
                    client_sockets[i] = 0;
                    printf("TCP client disconnected\n");
                } else {
                    buffer[valread] = '\0';
                    handle_add_command(buffer);
                }
            }
        }

        // Handle UDP request
        if (FD_ISSET(udp_fd, &readfds)) {
            int len = recvfrom(udp_fd, buffer, BUFFER_SIZE - 1, 0,
                               (struct sockaddr *)&client_addr, &addrlen);
            if (len > 0) {
                buffer[len] = '\0';
                int result = deliver_molecules(buffer);
                const char *reply;
                if (result == 1) reply = "DELIVERED";
                else if (result == 0) reply = "INSUFFICIENT ATOMS";
                else if (result == -2) reply = "UNKNOWN MOLECULE";
                else reply = "INVALID REQUEST";

                sendto(udp_fd, reply, strlen(reply), 0,
                       (struct sockaddr *)&client_addr, addrlen);
                printf("UDP request: '%s' → %s\n", buffer, reply);
            }
        }
    }

    close(tcp_fd);
    close(udp_fd);
    return 0;
}
