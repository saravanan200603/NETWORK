#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main() {
    int sockfd;
    char message[1024], buffer[1024];
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

    printf("UDP Chat Client initialized. Type 'exit' to quit.\n");

    while (1) {
        // 2. Type message to server
        printf("\nClient: ");
        fgets(message, sizeof(message), stdin);
        message[strcspn(message, "\r\n")] = '\0';

        // Send message to server (first request goes to port 5000, subsequent to child's port)
        sendto(sockfd, message, strlen(message), 0, (struct sockaddr *)&serverAddr, addrLen);

        if (strcmp(message, "exit") == 0) {
            printf("Disconnected.\n");
            break;
        }

        // 3. Receive reply from child server process (serverAddr automatically updates to child's port)
        memset(buffer, 0, sizeof(buffer));
        int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&serverAddr, &addrLen);
        if (n < 0) {
            printf("Receive failed\n");
            break;
        }
        buffer[n] = '\0';

        if (strcmp(buffer, "exit") == 0) {
            printf("Server has ended the chat.\n");
            break;
        }

        printf("Server: %s\n", buffer);
    }

    close(sockfd);
    return 0;
}
