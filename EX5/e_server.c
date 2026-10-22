#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8081
#define BUFFER_SIZE 1024

int main() {
    int server_fd;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    // 1. Create a UDP Socket (AF_INET = IPv4, SOCK_DGRAM = UDP)
    server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 2. Clear memory and configure server's address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;                // IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY;         // Listen on any IP address
    server_addr.sin_port = htons(PORT);              // Convert port to Network Byte Order

    // 3. Bind the socket to Port 8080 (Claim apartment window #8080)
    if (bind(server_fd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server running! Waiting for UDP packets on port %d...\n", PORT);

    // 4. Infinite loop: Receive datagram -> Echo back
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);

        // Blocks (pauses) until a packet arrives.
        // Captures the client's address info into 'client_addr'
        int bytes_received = recvfrom(server_fd, buffer, BUFFER_SIZE - 1, 0,
                                      (struct sockaddr *)&client_addr, &addr_len);
        if (bytes_received < 0) {
            perror("Recvfrom failed");
            continue;
        }

        // Print incoming packet source and message to server terminal
        printf("Received from [%s:%d]: %s",
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port),
               buffer);

        // Echo the exact message back to the sender
        sendto(server_fd, buffer, bytes_received, 0,
               (const struct sockaddr *)&client_addr, addr_len);
    }

    close(server_fd);
    return 0;
}
