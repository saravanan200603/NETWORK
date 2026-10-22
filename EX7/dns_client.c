#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main() {
    int sockfd, choice;
    char domain[1024], buffer[1024];
    struct sockaddr_in serverAddr;
    socklen_t addrLen = sizeof(serverAddr);

    // 1. Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        printf("Socket creation failed\n");
        return 1;
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(5000);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    printf("UDP DNS Client initialized.\n");

    while (1) {
        printf("\n1. Look up Domain Name\n");
        printf("2. Exit\n");
        printf("Enter choice: ");
        scanf("%d", &choice);
        getchar(); // consume newline

        if (choice == 2) break;

        if (choice == 1) {
            printf("Enter domain name (e.g., google.com): ");
            fgets(domain, sizeof(domain), stdin);
            domain[strcspn(domain, "\r\n")] = '\0';

            // Send domain to server port (initially 5000, updated dynamically on reply)
            sendto(sockfd, domain, strlen(domain), 0, (struct sockaddr *)&serverAddr, addrLen);

            // Receive response (serverAddr automatically updates to child process's socket port)
            memset(buffer, 0, sizeof(buffer));
            int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&serverAddr, &addrLen);
            if (n < 0) {
                printf("Receive failed\n");
                break;
            }
            buffer[n] = '\0';

            printf("Resolved IP Address: %s\n", buffer);
        } else {
            printf("Invalid choice\n");
        }
    }

    close(sockfd);
    return 0;
}
