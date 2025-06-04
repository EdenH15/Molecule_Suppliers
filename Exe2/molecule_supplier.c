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

#define MAX_CLIENTS 30
#define BUFFER_SIZE 1024

uint64_t hydrogen = 0, oxygen = 0, carbon = 0;

/**
 * @brief Prints the current inventory of atoms (hydrogen, oxygen, carbon).
 */
void print_inventory() {
    printf("Inventory => HYDROGEN: %lu, OXYGEN: %lu, CARBON: %lu\n", hydrogen, oxygen, carbon);
}

/**
 * @brief Parses and handles an ADD command, updating atom inventory accordingly.
 *
 * @param cmd The received command string, expected format: "ADD <ATOM_TYPE> <AMOUNT>"
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
 * @brief Processes a DELIVER command and attempts to create and deliver the requested molecule(s).
 *
 * @param cmd The command string, expected format: "DELIVER <MOLECULE_NAME> <AMOUNT>"
 * @return int
 *         1 - Successfully delivered molecules.
 *         0 - Insufficient atoms in inventory.
 *        -1 - Invalid command format or amount.
 *        -2 - Unknown molecule name.
 */
int deliver_molecules(const char *cmd) {
    char mol1[16], mol2[16];
    char mol[32];
    int amount;

    // Try parsing a two-word molecule
    if (sscanf(cmd, "DELIVER %15s %15s %d", mol1, mol2, &amount) == 3) {
        snprintf(mol, sizeof(mol), "%s %s", mol1, mol2);
    }
    // Try parsing a single-word molecule
    else if (sscanf(cmd, "DELIVER %15s %d", mol1, &amount) == 2) {
        strncpy(mol, mol1, sizeof(mol));
    } else {
        return -1; // Failed to parse command
    }

    if (amount <= 0) return -1; // Invalid amount

    uint64_t need_H = 0, need_O = 0, need_C = 0;

    // Define requirements for known molecules
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
        return -2; // Unknown molecule
    }

    // Check if inventory is sufficient
    if (hydrogen >= need_H && oxygen >= need_O && carbon >= need_C) {
        hydrogen -= need_H;
        oxygen -= need_O;
        carbon -= need_C;
        print_inventory();
        return 1; // Delivery successful
    } else {
        return 0; // Insufficient atoms
    }
}

/**
 * @brief Main function that starts a server to handle TCP and UDP communications
 *        for adding atoms and delivering molecules.
 *
 * @param argc Number of command-line arguments.
 * @param argv Command-line arguments. Expected format: -p <port>
 * @return int Exit status.
 */
int main(int argc, char *argv[]) {
    int opt;
    int port = -1;

    // Parse port argument
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

    if (port == -1) {
        fprintf(stderr, "Port not specified\n");
        exit(EXIT_FAILURE);
    }

    int tcp_fd, udp_fd, client_sockets[MAX_CLIENTS] = {0};
    struct sockaddr_in server_addr, client_addr;
    socklen_t addrlen = sizeof(client_addr);
    fd_set readfds;
    char buffer[BUFFER_SIZE];

    // Create TCP and UDP sockets
    tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
    udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (tcp_fd < 0 || udp_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Allow address reuse
    int optval = 1;
    setsockopt(tcp_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    // Setup server address struct
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Bind sockets to the port
    if (bind(tcp_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 ||
        bind(udp_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // Start listening on TCP socket
    if (listen(tcp_fd, 5) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("molecule_supplier server started on port %d\n", port);

    // Main server loop
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(tcp_fd, &readfds);
        FD_SET(udp_fd, &readfds);
        int max_sd = (tcp_fd > udp_fd) ? tcp_fd : udp_fd;

        // Add client sockets to the read set
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_sockets[i];
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        // Set 10-second timeout
        struct timeval timeout = {10, 0};

        int activity = select(max_sd + 1, &readfds, NULL, NULL, &timeout);

        if (activity < 0) {
            perror("select");
            continue;
        } else if (activity == 0) {
            printf("select timeout, no activity for 10 seconds\n");
            continue;
        }

        // Handle new TCP connection
        if (FD_ISSET(tcp_fd, &readfds)) {
            int new_socket = accept(tcp_fd, (struct sockaddr *)&client_addr, &addrlen);
            if (new_socket < 0) {
                perror("accept");
                continue;
            }
            printf("New TCP connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = new_socket;
                    break;
                }
            }
        }

        // Handle TCP client messages
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

        // Handle UDP requests
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
