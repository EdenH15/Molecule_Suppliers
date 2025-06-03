#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    char *hostname = NULL;
    char *port_str = NULL;
    int opt;

    // parse options -h <hostname> -p <port>
    while ((opt = getopt(argc, argv, "h:p:")) != -1) {
        switch (opt) {
            case 'h':
                hostname = optarg;
                break;
            case 'p':
                port_str = optarg;
                break;
            default:
                fprintf(stderr, "Usage: %s -h <hostname/IP> -p <tcp-port>\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (!hostname || !port_str) {
        fprintf(stderr, "Usage: %s -h <hostname/IP> -p <tcp-port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(port_str);
    if (port <= 0) {
        fprintf(stderr, "Invalid port number\n");
        exit(EXIT_FAILURE);
    }

    // Prepare connection to server
    struct addrinfo hints, *res;
    int sockfd;
    char buffer[BUFFER_SIZE];

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;        // IPv4
    hints.ai_socktype = SOCK_STREAM;  // TCP

    if (getaddrinfo(hostname, port_str, &hints, &res) != 0) {
        perror("getaddrinfo");
        exit(EXIT_FAILURE);
    }

    // Create socket
    if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0) {
        perror("socket");
        freeaddrinfo(res);
        exit(EXIT_FAILURE);
    }

    // Connect to server
    if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect");
        close(sockfd);
        freeaddrinfo(res);
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(res);

    printf("Connected to server at %s:%d\n", hostname, port);
    printf("Enter commands (e.g., ADD OXYGEN 2), Ctrl+D to quit:\n");

    // Input from client
    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        // Send to server
        if (send(sockfd, buffer, strlen(buffer), 0) < 0) {
            perror("send");
            break;
        }
    }

    printf("Disconnected.\n");
    close(sockfd);
    return 0;
}
