
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
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <hostname/IP> <tcp-port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *hostname = argv[1];
    int port = atoi(argv[2]);

    // Prepare connection to server
    struct addrinfo hints, *res;
    int sockfd;
    char buffer[BUFFER_SIZE];

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(hostname, argv[2], &hints, &res) != 0) {
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