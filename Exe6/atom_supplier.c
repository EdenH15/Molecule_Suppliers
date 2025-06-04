#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <errno.h>

#define BUFFER_SIZE 1024
#define MAX_PATH 108

int main(int argc, char *argv[]) {
    char *hostname = NULL;
    char *port_str = NULL;
    char *uds_path = NULL;
    int opt;

    // parse options: -h <hostname> -p <port> OR -f <UDS socket file path>
    while ((opt = getopt(argc, argv, "h:p:f:")) != -1) {
        switch (opt) {
            case 'h':
                hostname = optarg;
                break;
            case 'p':
                port_str = optarg;
                break;
            case 'f':
                uds_path = optarg;
                break;
            default:
                fprintf(stderr, "Usage: %s -h <hostname> -p <tcp-port> OR -f <UDS socket file path>\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if ((hostname || port_str) && uds_path) {
        fprintf(stderr, "Error: cannot specify both TCP (-h/-p) and UDS (-f) options\n");
        exit(EXIT_FAILURE);
    }

    int sockfd;
    char buffer[BUFFER_SIZE];

    if (uds_path) {
        // Connect using Unix Domain Socket
        struct sockaddr_un addr;
        if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
            perror("socket");
            exit(EXIT_FAILURE);
        }

        memset(&addr, 0, sizeof(struct sockaddr_un));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, uds_path, sizeof(addr.sun_path) - 1);

        if (connect(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1) {
            perror("connect (UDS)");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        printf("Connected to UDS server at %s\n", uds_path);

    } else {
        // Validate input
        if (!hostname || !port_str) {
            fprintf(stderr, "Usage: %s -h <hostname> -p <tcp-port> OR -f <UDS socket file path>\n", argv[0]);
            exit(EXIT_FAILURE);
        }

        int port = atoi(port_str);
        if (port <= 0) {
            fprintf(stderr, "Invalid port number\n");
            exit(EXIT_FAILURE);
        }

        // Prepare connection to server (TCP)
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;        // IPv4
        hints.ai_socktype = SOCK_STREAM;  // TCP

        if (getaddrinfo(hostname, port_str, &hints, &res) != 0) {
            perror("getaddrinfo");
            exit(EXIT_FAILURE);
        }

        if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0) {
            perror("socket");
            freeaddrinfo(res);
            exit(EXIT_FAILURE);
        }

        if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
            perror("connect (TCP)");
            close(sockfd);
            freeaddrinfo(res);
            exit(EXIT_FAILURE);
        }

        freeaddrinfo(res);

        printf("Connected to TCP server at %s:%d\n", hostname, port);
    }

    printf("Enter commands (e.g., ADD OXYGEN 2), Ctrl+D to quit:\n");

    // Input from client
    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        if (send(sockfd, buffer, strlen(buffer), 0) < 0) {
            perror("send");
            break;
        }
    }

    printf("Disconnected.\n");
    close(sockfd);
    return 0;
}