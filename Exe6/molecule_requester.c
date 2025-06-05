#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#define BUFFER_SIZE 1024

/**
 * Prints the usage message and exits the program.
 *
 * @param prog The program name (usually argv[0])
 */
void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [-h <hostname/IP> -p <port>] | [-f <UDS socket file path>]\n", prog);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    char *hostname = NULL;
    char *port = NULL;
    char *uds_path = NULL;
    int opt;

    while ((opt = getopt(argc, argv, "h:p:f:")) != -1) {
        switch (opt) {
            case 'h': hostname = optarg; break;
            case 'p': port = optarg; break;
            case 'f': uds_path = optarg; break;
            default: usage(argv[0]);
        }
    }

    if ((uds_path && (hostname || port)) || (!uds_path && (!hostname || !port))) {
        fprintf(stderr, "Error: Provide either -f <socket_path> OR -h <host> and -p <port>, not both.\n");
        usage(argv[0]);
    }

    int sockfd;
    char buffer[BUFFER_SIZE];
    char recvbuf[BUFFER_SIZE];

    if (uds_path) {
        // --- UDS MODE ---
        struct sockaddr_un server_addr, client_addr;

        sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
        if (sockfd < 0) {
            perror("socket");
            exit(EXIT_FAILURE);
        }


        snprintf(client_addr.sun_path, sizeof(client_addr.sun_path), "/tmp/molecule_client_%d.sock", getpid());
        unlink(client_addr.sun_path);

        memset(&client_addr, 0, sizeof(client_addr));
        client_addr.sun_family = AF_UNIX;


        if (bind(sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
            perror("bind");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sun_family = AF_UNIX;
        strncpy(server_addr.sun_path, uds_path, sizeof(server_addr.sun_path) - 1);

        printf("[UDS] Enter molecule requests (e.g., DELIVER WATER 5). Type 'exit' to quit.\n");
        while (1) {
            printf(">>> ");
            if (!fgets(buffer, sizeof(buffer), stdin)) break;
            buffer[strcspn(buffer, "\n")] = 0;
            if (strcmp(buffer, "exit") == 0) break;

            if (sendto(sockfd, buffer, strlen(buffer), 0,
                       (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                perror("sendto");
                continue;
            }

            struct sockaddr_un from_addr;
            socklen_t from_len = sizeof(from_addr);
            ssize_t recvlen = recvfrom(sockfd, recvbuf, sizeof(recvbuf) - 1, 0,
                                       (struct sockaddr *)&from_addr, &from_len);
            if (recvlen < 0) {
                perror("recvfrom");
                continue;
            }

            recvbuf[recvlen] = '\0';
            printf("Server replied: %s\n", recvbuf);
        }

        close(sockfd);
        unlink(client_addr.sun_path);

    } else {
        // --- UDP MODE ---
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;

        if (getaddrinfo(hostname, port, &hints, &res) != 0) {
            perror("getaddrinfo");
            return 2;
        }

        sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sockfd < 0) {
            perror("socket");
            freeaddrinfo(res);
            return 3;
        }

        printf("[UDP] Enter molecule requests (e.g., DELIVER WATER 5). Type 'exit' to quit.\n");
        while (1) {
            printf(">>> ");
            if (!fgets(buffer, sizeof(buffer), stdin)) break;
            buffer[strcspn(buffer, "\n")] = 0;
            if (strcmp(buffer, "exit") == 0) break;

            if (sendto(sockfd, buffer, strlen(buffer), 0, res->ai_addr, res->ai_addrlen) < 0) {
                perror("sendto");
                continue;
            }

            struct sockaddr_storage from_addr;
            socklen_t from_len = sizeof(from_addr);
            ssize_t recvlen = recvfrom(sockfd, recvbuf, sizeof(recvbuf) - 1, 0,
                                       (struct sockaddr *)&from_addr, &from_len);
            if (recvlen < 0) {
                perror("recvfrom");
                continue;
            }

            recvbuf[recvlen] = '\0';
            printf("Server replied: %s\n", recvbuf);
        }

        close(sockfd);
        freeaddrinfo(res);
    }

    return 0;
}