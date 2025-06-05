#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include "drinks_bar_v6.h"

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

struct Inventory {
    unsigned long long carbon;
    unsigned long long oxygen;
    unsigned long long hydrogen;
} inventory = {0, 0, 0};

int tcp_socket = -1, udp_socket = -1;
int timed_out = 0;
int timeout_seconds = 0;
char *save_file_path = NULL;
int server_fd;
int client_sockets[MAX_CLIENTS] = {0};

/**
 * Prints the current inventory to stdout.
 */
void print_inventory() {
    printf("Inventory => CARBON: %llu, OXYGEN: %llu, HYDROGEN: %llu\n",
           inventory.carbon, inventory.oxygen, inventory.hydrogen);
}

/**
 * @brief Cleans up resources and exits the server gracefully.
 * Closes all active client sockets and the server socket.
 *
 * @param sig The signal number that triggered the handler.
 */
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
 * Alarm signal handler. Sets the timed_out flag.
 */
void alarm_handler(int sig) {
    (void)sig;
    timed_out = 1;
}

/**
 * Loads the inventory from a file using a shared (read) lock.
 * @param filename Path to the file containing the inventory.
 * @return 0 on success, -1 on failure.
 */
int load_inventory_from_file(const char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) return -1;

    // Acquire shared lock for reading
    if (flock(fd, LOCK_SH) != 0) {
        close(fd);
        return -1;
    }

    FILE *f = fdopen(fd, "r");
    if (!f) {
        flock(fd, LOCK_UN);
        close(fd);
        return -1;
    }

    unsigned long long c, o, h;
    if (fscanf(f, "%llu %llu %llu", &c, &o, &h) != 3) {
        flock(fd, LOCK_UN);
        fclose(f);
        return -1;
    }

    inventory.carbon = c;
    inventory.oxygen = o;
    inventory.hydrogen = h;

    flock(fd, LOCK_UN);
    fclose(f);
    return 0;
}

/**
 * Saves the current inventory to a file using an exclusive (write) lock.
 * @param filename Path to the file where inventory will be saved.
 * @return 0 on success, -1 on failure.
 */
int save_inventory_to_file(const char *filename) {
    int fd = open(filename, O_WRONLY | O_CREAT, 0666);
    if (fd < 0) return -1;

    // Acquire exclusive lock for writing
    if (flock(fd, LOCK_EX) != 0) {
        close(fd);
        return -1;
    }

    // Truncate file before writing new data
    if (ftruncate(fd, 0) != 0) {
        flock(fd, LOCK_UN);
        close(fd);
        return -1;
    }

    FILE *f = fdopen(fd, "w");
    if (!f) {
        flock(fd, LOCK_UN);
        close(fd);
        return -1;
    }

    fprintf(f, "%llu %llu %llu\n", inventory.carbon, inventory.oxygen, inventory.hydrogen);
    fflush(f); // Ensure data is written to disk

    flock(fd, LOCK_UN);
    fclose(f);
    return 0;
}

/**
 * Processes a TCP command string of the form "ADD <ATOM> <NUMBER>".
 * Updates the inventory accordingly.
 * @param cmd The command received from the client.
 * @param response The response buffer to write result into.
 * @param resp_len The maximum length of the response buffer.
 * @return 0 on success, -1 on error.
 */
int process_tcp_command(const char *cmd, char *response, size_t resp_len) {
    char action[16];
    char atom[16];
    unsigned long long number = 0;

    int n = sscanf(cmd, "%15s %15s %llu", action, atom, &number);
    if (n != 3) {
        snprintf(response, resp_len, "ERROR: invalid command format\n");
        return -1;
    }

    if (strcmp(action, "ADD") != 0) {
        snprintf(response, resp_len, "ERROR: unknown action '%s'\n", action);
        return -1;
    }
    if (number == 0) {
        snprintf(response, resp_len, "ERROR: invalid number\n");
        return -1;
    }

    // Ensure inventory consistency between concurrent servers using locking
    if (save_file_path) {
        // Acquire exclusive lock for reading and updating the inventory file
        int fd = open(save_file_path, O_RDWR);
        if (fd < 0) {
            snprintf(response, resp_len, "ERROR: could not open inventory file\n");
            return -1;
        }
        if (flock(fd, LOCK_EX) != 0) {
            close(fd);
            snprintf(response, resp_len, "ERROR: could not lock inventory file\n");
            return -1;
        }

        FILE *f = fdopen(fd, "r+");
        if (!f) {
            flock(fd, LOCK_UN);
            close(fd);
            snprintf(response, resp_len, "ERROR: could not open inventory file stream\n");
            return -1;
        }

        rewind(f);
        unsigned long long c, o, h;
        if (fscanf(f, "%llu %llu %llu", &c, &o, &h) != 3) {
            flock(fd, LOCK_UN);
            fclose(f);
            snprintf(response, resp_len, "ERROR: corrupted inventory file\n");
            return -1;
        }

        inventory.carbon = c;
        inventory.oxygen = o;
        inventory.hydrogen = h;

        // Update inventory based on atom type
        if (strcmp(atom, "CARBON") == 0) {
            inventory.carbon += number;
        } else if (strcmp(atom, "OXYGEN") == 0) {
            inventory.oxygen += number;
        } else if (strcmp(atom, "HYDROGEN") == 0) {
            inventory.hydrogen += number;
        } else {
            flock(fd, LOCK_UN);
            fclose(f);
            snprintf(response, resp_len, "ERROR: unknown atom type '%s'\n", atom);
            return -1;
        }

        // Overwrite updated inventory to file
        rewind(f);
        if (ftruncate(fd, 0) != 0) {
            flock(fd, LOCK_UN);
            fclose(f);
            snprintf(response, resp_len, "ERROR: failed truncating inventory file\n");
            return -1;
        }

        fprintf(f, "%llu %llu %llu\n", inventory.carbon, inventory.oxygen, inventory.hydrogen);
        fflush(f);

        flock(fd, LOCK_UN);
        fclose(f);
    } else {
        // Update in-memory inventory if no file is used
        if (strcmp(atom, "CARBON") == 0) {
            inventory.carbon += number;
        } else if (strcmp(atom, "OXYGEN") == 0) {
            inventory.oxygen += number;
        } else if (strcmp(atom, "HYDROGEN") == 0) {
            inventory.hydrogen += number;
        } else {
            snprintf(response, resp_len, "ERROR: unknown atom type '%s'\n", atom);
            return -1;
        }
    }

    print_inventory();
    snprintf(response, resp_len, "OK: Added atoms. Current inventory: C:%llu O:%llu H:%llu\n",
             inventory.carbon, inventory.oxygen, inventory.hydrogen);
    return 0;
}


/**
 * Processes a UDP command that requests molecule delivery.
 * Supports known molecules like WATER, CO2, GLUCOSE, etc.
 * Updates the inventory after checking if there are enough atoms.
 *
 * @param cmd       The incoming command string.
 * @param response  Output buffer for the server's response.
 * @param resp_len  Maximum length of the response buffer.
 * @return 0 on success, -1 on error with a message in response.
 */
int process_udp_command(const char *cmd, char *response, size_t resp_len) {
    char action[16];
    char molecule[32];
    unsigned long long number = 0;

    int n = sscanf(cmd, "%15s %31s %llu", action, molecule, &number);
    if (n != 3) {
        snprintf(response, resp_len, "ERROR: invalid command format\n");
        return -1;
    }

    if (strcmp(action, "DELIVER") != 0) {
        snprintf(response, resp_len, "ERROR: unknown action '%s'\n", action);
        return -1;
    }
    if (number == 0) {
        snprintf(response, resp_len, "ERROR: invalid number\n");
        return -1;
    }

    unsigned long long c_req=0, o_req=0, h_req=0;

    // Determine atom requirements per molecule
    if (strcmp(molecule, "WATER") == 0) {
        h_req = 2 * number;
        o_req = 1 * number;
    } else if (strcmp(molecule, "CARBON") == 0 || strcmp(molecule, "CARBONDIOXIDE") == 0 ||
               strcmp(molecule, "CARBON_DIOXIDE") == 0 || strcmp(molecule, "CO2") == 0) {
        c_req = 1 * number;
        o_req = 2 * number;
    } else if (strcmp(molecule, "GLUCOSE") == 0) {
        c_req = 6 * number;
        h_req = 12 * number;
        o_req = 6 * number;
    } else if (strcmp(molecule, "ALCOHOL") == 0 || strcmp(molecule, "ETHANOL") == 0) {
        c_req = 2 * number;
        h_req = 6 * number;
        o_req = 1 * number;
    } else {
        snprintf(response, resp_len, "ERROR: unknown molecule '%s'\n", molecule);
        return -1;
    }

    // Check and update inventory under file lock if applicable
    if (save_file_path) {
        int fd = open(save_file_path, O_RDWR);
        if (fd < 0) {
            snprintf(response, resp_len, "ERROR: could not open inventory file\n");
            return -1;
        }
        if (flock(fd, LOCK_EX) != 0) {
            close(fd);
            snprintf(response, resp_len, "ERROR: could not lock inventory file\n");
            return -1;
        }
        FILE *f = fdopen(fd, "r+");
        if (!f) {
            flock(fd, LOCK_UN);
            close(fd);
            snprintf(response, resp_len, "ERROR: could not open inventory file stream\n");
            return -1;
        }
        rewind(f);
        unsigned long long c, o, h;
        if (fscanf(f, "%llu %llu %llu", &c, &o, &h) != 3) {
            flock(fd, LOCK_UN);
            fclose(f);
            snprintf(response, resp_len, "ERROR: corrupted inventory file\n");
            return -1;
        }

        inventory.carbon = c;
        inventory.oxygen = o;
        inventory.hydrogen = h;

        if (inventory.carbon < c_req || inventory.oxygen < o_req || inventory.hydrogen < h_req) {
            flock(fd, LOCK_UN);
            fclose(f);
            snprintf(response, resp_len, "ERROR: not enough atoms to deliver %s %llu\n", molecule, number);
            return -1;
        }

        inventory.carbon -= c_req;
        inventory.oxygen -= o_req;
        inventory.hydrogen -= h_req;

        rewind(f);
        if (ftruncate(fd, 0) != 0) {
            flock(fd, LOCK_UN);
            fclose(f);
            snprintf(response, resp_len, "ERROR: failed truncating inventory file\n");
            return -1;
        }
        fprintf(f, "%llu %llu %llu\n", inventory.carbon, inventory.oxygen, inventory.hydrogen);
        fflush(f);

        flock(fd, LOCK_UN);
        fclose(f);
    } else {
        if (inventory.carbon < c_req || inventory.oxygen < o_req || inventory.hydrogen < h_req) {
            snprintf(response, resp_len, "ERROR: not enough atoms to deliver %s %llu\n", molecule, number);
            return -1;
        }

        inventory.carbon -= c_req;
        inventory.oxygen -= o_req;
        inventory.hydrogen -= h_req;
    }

    snprintf(response, resp_len, "OK: Delivered %s %llu\n", molecule, number);
    print_inventory();

    return 0;
}

/**
 * Processes a console command to simulate drink production.
 * Calculates how many drinks can be created with current inventory.
 *
 * @param cmd       The console command.
 * @param response  Output string with production capacity.
 * @param resp_len  Length of the response buffer.
 * @return 0 on success, -1 if the command is unknown.
 */
int process_console_command(const char *cmd, char *response, size_t resp_len) {
    if (strncmp(cmd, "GEN SOFT DRINK", 14) == 0) {
        unsigned long long max_water = inventory.hydrogen / 6;
        unsigned long long max_oxygen = inventory.oxygen / 3;
        unsigned long long can_make;

        if (max_water < max_oxygen)
            can_make = max_water;
        else
            can_make = max_oxygen;

        snprintf(response, resp_len, "Can produce %llu SOFT DRINK(s)\n", can_make);
        return 0;
    }

    else if (strncmp(cmd, "GEN VODKA", 9) == 0) {
        unsigned long long max_c = inventory.carbon / 3;
        unsigned long long max_h = inventory.hydrogen / 6;
        unsigned long long max_o = inventory.oxygen / 3;
        unsigned long long can_make = max_c;

        if (max_h < can_make)
            can_make = max_h;
        if (max_o < can_make)
            can_make = max_o;

        snprintf(response, resp_len, "Can produce %llu VODKA(s)\n", can_make);
        return 0;
    }

    else if (strncmp(cmd, "GEN CHAMPAGNE", 12) == 0) {
        unsigned long long max_c = inventory.carbon / 3;
        unsigned long long max_h = inventory.hydrogen / 8;
        unsigned long long max_o = inventory.oxygen / 4;
        unsigned long long can_make = max_c;

        if (max_h < can_make)
            can_make = max_h;
        if (max_o < can_make)
            can_make = max_o;

        snprintf(response, resp_len, "Can produce %llu CHAMPAGNE(s)\n", can_make);
        return 0;
    }

    snprintf(response, resp_len, "ERROR: Unknown console command\n");
    return -1;
}


/**
 * Closes open TCP and UDP sockets if they are active.
 * Used to clean up resources on exit.
 */
void cleanup() {
    if (tcp_socket != -1) close(tcp_socket);
    if (udp_socket != -1) close(udp_socket);
}


/**
 * @brief Main function that starts the TCP/UDP server, initializes inventory,
 * handles command-line arguments and manages client connections and requests.
 *
 * @param argc Argument count from command-line.
 * @param argv Argument vector from command-line.
 * @return int Exit status of the program (0 on success, EXIT_FAILURE on error).
 */

int main(int argc, char *argv[]) {
    int opt;
    unsigned int tcp_port = 0, udp_port = 0;
    unsigned long long init_carbon=0, init_oxygen=0, init_hydrogen=0;

    while ((opt = getopt(argc, argv, "T:U:o:c:h:t:f:")) != -1) {
        switch(opt) {
            case 'T': tcp_port = (unsigned int)atoi(optarg); break;
            case 'U': udp_port = (unsigned int)atoi(optarg); break;
            case 'o': init_oxygen = strtoull(optarg, NULL, 10); break;
            case 'c': init_carbon = strtoull(optarg, NULL, 10); break;
            case 'h': init_hydrogen = strtoull(optarg, NULL, 10); break;
            case 't': timeout_seconds = atoi(optarg); break;
            case 'f': save_file_path = strdup(optarg); break;
            default:
                fprintf(stderr, "Usage: %s -T <tcp-port> -U <udp-port> [-o oxygen] [-c carbon] [-h hydrogen] [-t timeout] [-f savefile]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (tcp_port == 0 || udp_port == 0) {
        fprintf(stderr, "Error: TCP and UDP ports must be specified with -T and -U\n");
        exit(EXIT_FAILURE);
    }

    if (save_file_path) {
        if (access(save_file_path, F_OK) == 0) {
            if (load_inventory_from_file(save_file_path) != 0) {
                fprintf(stderr, "Warning: Could not load inventory from %s, starting with initial values\n", save_file_path);
                inventory.carbon = init_carbon;
                inventory.oxygen = init_oxygen;
                inventory.hydrogen = init_hydrogen;
            } else {
                printf("Loaded inventory from %s\n", save_file_path);
            }
        } else {
            inventory.carbon = init_carbon;
            inventory.oxygen = init_oxygen;
            inventory.hydrogen = init_hydrogen;
            if (save_inventory_to_file(save_file_path) != 0) {
                fprintf(stderr, "Warning: Could not create save file %s\n", save_file_path);
            }
        }
    } else {
        inventory.carbon = init_carbon;
        inventory.oxygen = init_oxygen;
        inventory.hydrogen = init_hydrogen;
    }

    print_inventory();

    struct sockaddr_in tcp_addr, udp_addr;

    tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_socket < 0) {
        perror("TCP socket");
        exit(EXIT_FAILURE);
    }
    int optval = 1;
    setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    memset(&tcp_addr, 0, sizeof(tcp_addr));
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_addr.s_addr = INADDR_ANY;
    tcp_addr.sin_port = htons(tcp_port);

    if (bind(tcp_socket, (struct sockaddr*)&tcp_addr, sizeof(tcp_addr)) < 0) {
        perror("bind TCP");
        exit(EXIT_FAILURE);
    }
    if (listen(tcp_socket, MAX_CLIENTS) < 0) {
        perror("listen TCP");
        exit(EXIT_FAILURE);
    }

    udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket < 0) {
        perror("UDP socket");
        exit(EXIT_FAILURE);
    }
    memset(&udp_addr, 0, sizeof(udp_addr));
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    udp_addr.sin_port = htons(udp_port);

    if (bind(udp_socket, (struct sockaddr*)&udp_addr, sizeof(udp_addr)) < 0) {
        perror("bind UDP");
        exit(EXIT_FAILURE);
    }

    if (timeout_seconds > 0) {
        signal(SIGALRM, alarm_handler);
        alarm(timeout_seconds);
    }

    int clients[MAX_CLIENTS];
    for (int i=0; i<MAX_CLIENTS; i++) clients[i] = -1;

    fd_set read_fds;
    int max_fd = tcp_socket > udp_socket ? tcp_socket : udp_socket;
    signal(SIGINT, cleanup_and_exit);

    printf("Server listening on TCP port %u and UDP port %u\n", tcp_port, udp_port);

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(tcp_socket, &read_fds);
        FD_SET(udp_socket, &read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        for (int i=0; i<MAX_CLIENTS; i++) {
            if (clients[i] != -1) {
                FD_SET(clients[i], &read_fds);
                if (clients[i] > max_fd) max_fd = clients[i];
            }
        }

        int ready = select(max_fd+1, &read_fds, NULL, NULL, NULL);
        if (ready < 0) {
            if (errno == EINTR) {
                if (timed_out) {
                    printf("Timeout reached, exiting...\n");
                    cleanup();
                    exit(EXIT_SUCCESS);
                }
                continue;
            }
            perror("select");
            cleanup();
            exit(EXIT_FAILURE);
        }

        // Check for new TCP connection
        if (FD_ISSET(tcp_socket, &read_fds)) {
            int new_client = accept(tcp_socket, NULL, NULL);
            if (new_client >= 0) {
                int added = 0;
                for (int i=0; i<MAX_CLIENTS; i++) {
                    if (clients[i] == -1) {
                        clients[i] = new_client;
                        added = 1;
                        break;
                    }
                }
                if (!added) {
                    close(new_client);
                    printf("Max clients reached, connection refused\n");
                }
            }
        }

        // Check for data from TCP clients
        for (int i=0; i<MAX_CLIENTS; i++) {
            if (clients[i] != -1 && FD_ISSET(clients[i], &read_fds)) {
                char buf[BUFFER_SIZE] = {0};
                ssize_t len = recv(clients[i], buf, sizeof(buf)-1, 0);
                if (len <= 0) {
                    close(clients[i]);
                    clients[i] = -1;
                } else {
                    char response[BUFFER_SIZE];
                    if (process_tcp_command(buf, response, sizeof(response)) == 0) {
                        send(clients[i], response, strlen(response), 0);
                    } else {
                        send(clients[i], response, strlen(response), 0);
                    }
                }
            }
        }

        // Check for data from UDP clients
        if (FD_ISSET(udp_socket, &read_fds)) {
            char buf[BUFFER_SIZE] = {0};
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            ssize_t len = recvfrom(udp_socket, buf, sizeof(buf)-1, 0, (struct sockaddr*)&client_addr, &addr_len);
            if (len > 0) {
                char response[BUFFER_SIZE];
                if (process_udp_command(buf, response, sizeof(response)) == 0) {
                    sendto(udp_socket, response, strlen(response), 0, (struct sockaddr*)&client_addr, addr_len);
                } else {
                    sendto(udp_socket, response, strlen(response), 0, (struct sockaddr*)&client_addr, addr_len);
                }
            }
        }

        // Check for input from console
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            char buf[BUFFER_SIZE] = {0};
            if (fgets(buf, sizeof(buf), stdin) != NULL) {
                buf[strcspn(buf, "\r\n")] = 0;  // הסרת תו newline
                char response[BUFFER_SIZE];
                if (process_console_command(buf, response, sizeof(response)) == 0) {
                    printf("%s", response);
                } else {
                    printf("%s", response);
                }
            }
        }
    }

    cleanup();
    return 0;
}
