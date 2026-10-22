#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8081
#define SERVER_IP "127.0.1.0"  // Localhost (your own computer)
#define BUFFER_SIZE 1024

int main() {
    int client_fd;
    char buffer[BUFFER_SIZE];
    char message[BUFFER_SIZE];
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);

    // 1. Create a UDP Socket
    client_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (client_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 2. Configure target server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);
    printf("Ready to send to Echo Server at %s:%d\n", SERVER_IP, PORT);
    
    while (1) {
        printf("\nEnter message (type 'exit' to quit): ");
        if (fgets(message, BUFFER_SIZE, stdin) == NULL) break;

        // Check if user wants to quit
        if (strncmp(message, "exit", 4) == 0) {
            printf("Exiting client...\n");
            break;
        }

        // 3. Send message to server window (sendto)
        sendto(client_fd, message, strlen(message), 0,
               (const struct sockaddr *)&server_addr, addr_len);

        // 4. Wait for the server's reply (recvfrom)
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recvfrom(client_fd, buffer, BUFFER_SIZE - 1, 0,
                                      (struct sockaddr *)&server_addr, &addr_len);

        if (bytes_received > 0) {
            printf("Echoed from server: %s", buffer);
        }
    }

    close(client_fd);
    return 0;
}
