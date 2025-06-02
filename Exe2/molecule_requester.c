#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUFFER_SIZE 1024

void usage(const char *prog) {
    fprintf(stderr, "Usage: %s <hostname> <port>\n", prog);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server-hostname-or-ip> <port>\n", argv[0]);
        return 1;
    }

    char *hostname = argv[1];
    char *port = argv[2];

    struct addrinfo hints, *res;
    int sockfd;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;        // IPv4
    hints.ai_socktype = SOCK_DGRAM;   // UDP

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

    printf("Enter molecule requests (e.g., DELIVER WATER 5). Type 'exit' to quit.\n");

    char buffer[BUFFER_SIZE];
    char recvbuf[BUFFER_SIZE];
    while (1) {
        printf(">>> ");
        if (!fgets(buffer, sizeof(buffer), stdin)) break;

        // הסרת \n מהקלט
        buffer[strcspn(buffer, "\n")] = 0;

        if (strcmp(buffer, "exit") == 0) break;

        ssize_t sent = sendto(sockfd, buffer, strlen(buffer), 0,
                              res->ai_addr, res->ai_addrlen);
        if (sent < 0) {
            perror("sendto");
            break;
        }

        struct sockaddr_storage from_addr;
        socklen_t from_len = sizeof(from_addr);

        ssize_t recvlen = recvfrom(sockfd, recvbuf, sizeof(recvbuf) - 1, 0,
                                   (struct sockaddr *)&from_addr, &from_len);
        if (recvlen < 0) {
            perror("recvfrom");
            break;
        }
        recvbuf[recvlen] = '\0';

        printf("Server replied: %s\n", recvbuf);
    }

    freeaddrinfo(res);
    close(sockfd);
    return 0;
}
