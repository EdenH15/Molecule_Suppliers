//
// Created by eden on 6/3/25.

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
#include <sys/un.h>
#include "drinks_bar_v5.h"

#define MAX_CLIENTS 30
#define BUFFER_SIZE 1024

uint64_t hydrogen = 0, oxygen = 0, carbon = 0;
int timeout_seconds = 60;

char *uds_stream_path = NULL;
char *uds_dgram_path = NULL;
int uds_stream_fd = -1;
int uds_dgram_fd = -1;
int server_fd;
int client_sockets[MAX_CLIENTS] = {0};


/**
 * Prints current atom inventory to stdout.
 */
void print_inventory() {
    printf("Inventory => HYDROGEN: %lu, OXYGEN: %lu, CARBON: %lu\n", hydrogen, oxygen, carbon);
    fflush(stdout);
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

/**
 * Handles TCP commands of the form "ADD <ATOM> <AMOUNT>".
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
 * Handles UDP commands of the form "DELIVER <MOLECULE> <AMOUNT>".
 *
 * Returns:
 *  1  - delivery successful
 *  0  - insufficient atoms
 * -1  - invalid syntax
 * -2  - unknown molecule
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
 * Handles stdin commands: GEN SOFT DRINK / VODKA / CHAMPAGNE.
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
 * Handles SIGALRM signal (used to interrupt select()).
 */
void timeout_handler(int sig) {
    // Empty on purpose
}
// ... keep previous functions print_inventory(), handle_add_command(), deliver_molecules(), handle_console_command(), timeout_handler() unchanged ...

int main(int argc, char *argv[]) {
    int tcp_port = -1, udp_port = -1;
    int opt;

    struct option long_options[] = {
        {"oxygen", required_argument, NULL, 'o'},
        {"carbon", required_argument, NULL, 'c'},
        {"hydrogen", required_argument, NULL, 'h'},
        {"timeout", required_argument, NULL, 't'},
        {"tcp-port", required_argument, NULL, 'T'},
        {"udp-port", required_argument, NULL, 'U'},
        {"stream-path", required_argument, NULL, 's'},
        {"datagram-path", required_argument, NULL, 'd'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "o:c:h:t:T:U:s:d:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'o': oxygen = strtoull(optarg, NULL, 10); break;
            case 'c': carbon = strtoull(optarg, NULL, 10); break;
            case 'h': hydrogen = strtoull(optarg, NULL, 10); break;
            case 't': timeout_seconds = atoi(optarg); break;
            case 'T': tcp_port = atoi(optarg); break;
            case 'U': udp_port = atoi(optarg); break;
            case 's': uds_stream_path = strdup(optarg); break;
            case 'd': uds_dgram_path = strdup(optarg); break;
            default:
                fprintf(stderr, "Usage: %s -T <tcp-port> -U <udp-port> [-o oxygen] [-c carbon] [-h hydrogen] [-t timeout] [-s uds_stream] [-d uds_dgram]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    signal(SIGALRM, timeout_handler);

    int tcp_fd = -1, udp_fd = -1;
    struct sockaddr_in server_addr, client_addr;
    struct sockaddr_un uds_stream_addr, uds_dgram_addr;
    socklen_t addrlen = sizeof(client_addr);
    fd_set readfds;
    char buffer[BUFFER_SIZE];

    if (tcp_port > 0) {
        tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
        int sock_opt = 1;
        setsockopt(tcp_fd, SOL_SOCKET, SO_REUSEADDR, &sock_opt, sizeof(sock_opt));
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(tcp_port);
        bind(tcp_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
        listen(tcp_fd, 5);
    }

    if (udp_port > 0) {
        udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(udp_port);
        bind(udp_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    }

    if (uds_stream_path) {
        uds_stream_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        unlink(uds_stream_path);
        memset(&uds_stream_addr, 0, sizeof(uds_stream_addr));
        uds_stream_addr.sun_family = AF_UNIX;
        strncpy(uds_stream_addr.sun_path, uds_stream_path, sizeof(uds_stream_addr.sun_path) - 1);
        bind(uds_stream_fd, (struct sockaddr*)&uds_stream_addr, sizeof(uds_stream_addr));
        listen(uds_stream_fd, 5);
    }

    if (uds_dgram_path) {
        uds_dgram_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
        unlink(uds_dgram_path);
        memset(&uds_dgram_addr, 0, sizeof(uds_dgram_addr));
        uds_dgram_addr.sun_family = AF_UNIX;
        strncpy(uds_dgram_addr.sun_path, uds_dgram_path, sizeof(uds_dgram_addr.sun_path) - 1);
        bind(uds_dgram_fd, (struct sockaddr*)&uds_dgram_addr, sizeof(uds_dgram_addr));
    }

    printf("drinks_bar server started\n");
    print_inventory();
    signal(SIGINT, cleanup_and_exit);

    while (1) {
        alarm(timeout_seconds);
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        int max_sd = STDIN_FILENO;

        if (tcp_fd > 0) { FD_SET(tcp_fd, &readfds); if (tcp_fd > max_sd) max_sd = tcp_fd; }
        if (udp_fd > 0) { FD_SET(udp_fd, &readfds); if (udp_fd > max_sd) max_sd = udp_fd; }
        if (uds_stream_fd > 0) { FD_SET(uds_stream_fd, &readfds); if (uds_stream_fd > max_sd) max_sd = uds_stream_fd; }
        if (uds_dgram_fd > 0) { FD_SET(uds_dgram_fd, &readfds); if (uds_dgram_fd > max_sd) max_sd = uds_dgram_fd; }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_sockets[i];
            if (sd > 0) { FD_SET(sd, &readfds); if (sd > max_sd) max_sd = sd; }
        }

        if (select(max_sd + 1, &readfds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) {
                printf("Timeout expired - shutting down.\n");
                break;
            }
            perror("select"); continue;
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            if (fgets(buffer, sizeof(buffer), stdin)) {
                buffer[strcspn(buffer, "\n")] = '\0';
                handle_console_command(buffer);
            }
        }

        if (tcp_fd > 0 && FD_ISSET(tcp_fd, &readfds)) {
            int new_socket = accept(tcp_fd, (struct sockaddr *)&client_addr, &addrlen);
            if (new_socket >= 0) {
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (client_sockets[i] == 0) { client_sockets[i] = new_socket; break; }
                }
            }
        }

        if (uds_stream_fd > 0 && FD_ISSET(uds_stream_fd, &readfds)) {
            int new_socket = accept(uds_stream_fd, NULL, NULL);
            if (new_socket >= 0) {
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (client_sockets[i] == 0) { client_sockets[i] = new_socket; break; }
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_sockets[i];
            if (sd > 0 && FD_ISSET(sd, &readfds)) {
                int valread = read(sd, buffer, BUFFER_SIZE - 1);
                if (valread <= 0) { close(sd); client_sockets[i] = 0; }
                else {
                    buffer[valread] = '\0';
                    handle_add_command(buffer);
                }
            }
        }

        if (udp_fd > 0 && FD_ISSET(udp_fd, &readfds)) {
            int len = recvfrom(udp_fd, buffer, BUFFER_SIZE - 1, 0,
                               (struct sockaddr *)&client_addr, &addrlen);
            if (len > 0) {
                buffer[len] = '\0';
                int result = deliver_molecules(buffer);
                const char *reply = (result == 1) ? "DELIVERED" :
                                    (result == 0) ? "INSUFFICIENT ATOMS" :
                                    (result == -2) ? "UNKNOWN MOLECULE" :
                                                     "INVALID REQUEST";
                sendto(udp_fd, reply, strlen(reply), 0,
                       (struct sockaddr *)&client_addr, addrlen);
            }
        }

        if (uds_dgram_fd > 0 && FD_ISSET(uds_dgram_fd, &readfds)) {
            struct sockaddr_un client_addr;
            socklen_t len = sizeof(client_addr);
            int bytes = recvfrom(uds_dgram_fd, buffer, BUFFER_SIZE - 1, 0,
                                 (struct sockaddr*)&client_addr, &len);
            if (bytes > 0) {
                buffer[bytes] = '\0';
                int result = deliver_molecules(buffer);
                const char *reply = (result == 1) ? "DELIVERED" :
                                    (result == 0) ? "INSUFFICIENT ATOMS" :
                                    (result == -2) ? "UNKNOWN MOLECULE" :
                                                     "INVALID REQUEST";
                sendto(uds_dgram_fd, reply, strlen(reply), 0,
                       (struct sockaddr*)&client_addr, len);
                printf("[UDS DGRAM] Replied: %s\n", reply);
            }
        }
    }

    if (tcp_fd > 0) close(tcp_fd);
    if (udp_fd > 0) close(udp_fd);
    if (uds_stream_fd > 0) { close(uds_stream_fd); unlink(uds_stream_path); }
    if (uds_dgram_fd > 0) { close(uds_dgram_fd); unlink(uds_dgram_path); }

    return 0;
}