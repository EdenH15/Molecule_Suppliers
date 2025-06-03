#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUFFER_SIZE 1024

/**
 * Prints the usage message and exits the program.
 *
 * @param prog The program name (usually argv[0])
 */
void usage(const char *prog) {
    fprintf(stderr, "Usage: %s -h <hostname/IP> -p <port>\n", prog);
    exit(EXIT_FAILURE);
}

/**
 * UDP client that sends molecule delivery requests to the server.
 *
 * The program expects command line arguments in the format:
 *  -h <hostname/IP> -p <port>
 *
 * User inputs lines like "DELIVER WATER 5", which are sent via UDP.
 * The server's response is printed to the console.
 */
int main(int argc, char *argv[]) {
    char *hostname = NULL;
    char *port = NULL;
    int opt;

    // Parse command line options using getopt
    while ((opt = getopt(argc, argv, "h:p:")) != -1) {
        switch (opt) {
            case 'h':
                hostname = optarg;
                break;
            case 'p':
                port = optarg;
                break;
            default:
                usage(argv[0]);
        }
    }

    // Validate required options
    if (!hostname || !port) {
        usage(argv[0]);
    }

    struct addrinfo hints, *res;
    int sockfd;

    // Prepare hints for getaddrinfo: IPv4, UDP socket
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    // Resolve the server address and port
    if (getaddrinfo(hostname, port, &hints, &res) != 0) {
        perror("getaddrinfo");
        return 2;
    }

    // Create UDP socket
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

        // Remove trailing newline character
        buffer[strcspn(buffer, "\n")] = 0;

        // Exit condition
        if (strcmp(buffer, "exit") == 0) break;

        // Send request to server
        ssize_t sent = sendto(sockfd, buffer, strlen(buffer), 0, res->ai_addr, res->ai_addrlen);
        if (sent < 0) {
            perror("sendto");
            break;
        }

        // Receive server reply
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
