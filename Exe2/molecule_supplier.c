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

void print_inventory() {
    printf("Inventory => HYDROGEN: %lu, OXYGEN: %lu, CARBON: %lu\n", hydrogen, oxygen, carbon);
}

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

int deliver_molecules(const char *cmd) {
    char mol1[16], mol2[16];
    char mol[32];
    int amount;

    // ננסה לפרק לפי שני מילים
    if (sscanf(cmd, "DELIVER %15s %15s %d", mol1, mol2, &amount) == 3) {
        snprintf(mol, sizeof(mol), "%s %s", mol1, mol2);
    }
    // ננסה לפרק לפי מילה אחת
    else if (sscanf(cmd, "DELIVER %15s %d", mol1, &amount) == 2) {
        strncpy(mol, mol1, sizeof(mol));
    } else {
        return -1; // לא הצליח לפרש
    }

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
        return -2; // מולקולה לא מוכרת
    }

    if (hydrogen >= need_H && oxygen >= need_O && carbon >= need_C) {
        hydrogen -= need_H;
        oxygen -= need_O;
        carbon -= need_C;
        print_inventory();
        return 1; // סופק בהצלחה
    } else {
        return 0; // לא מספיק אטומים
    }
}


int main(int argc, char *argv[]) {
    int opt;
    int port = -1;

    // טיפול בפרמטרים עם getopt
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

    tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
    udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (tcp_fd < 0 || udp_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int optval = 1;
    setsockopt(tcp_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(tcp_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 ||
        bind(udp_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(tcp_fd, 5) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("molecule_supplier server started on port %d\n", port);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(tcp_fd, &readfds);
        FD_SET(udp_fd, &readfds);
        int max_sd = (tcp_fd > udp_fd) ? tcp_fd : udp_fd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_sockets[i];
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        // הוספת timeout ל-select: 10 שניות
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;

        int activity = select(max_sd + 1, &readfds, NULL, NULL, &timeout);

        if (activity < 0) {
            perror("select");
            continue;
        } else if (activity == 0) {
            // Timeout
            printf("select timeout, no activity for 10 seconds\n");
            continue;
        }

        // TCP new connection
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

        // TCP existing clients
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

        // UDP request
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
