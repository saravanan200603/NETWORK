#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 5454
#define BUFFER_SIZE 1024

int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE];
    char reply[BUFFER_SIZE];

    // 1. Create TCP Socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options to reuse port (helps restart server instantly)
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. Bind socket to IP/Port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 3. Listen for connections
    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("[+] TCP Iterative Chat Server started on port %d...\n", PORT);
    printf("[+] Waiting for clients to connect...\n\n");

    // ITERATIVE LOOP: Serves one client at a time in sequence
    while (1) {
        printf("---------------------------------------------\n");
        printf("[+] Waiting for a new client...\n");

        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            continue;
        }

        printf("[+] Client connected successfully!\n");
        printf("[+] Type 'bye' to end chat with this client.\n\n");

        // Chat Loop with current connected client
        while (1) {
            memset(buffer, 0, BUFFER_SIZE);
            memset(reply, 0, BUFFER_SIZE);

            // Receive message from client
            int bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
            if (bytes_read <= 0 || strncmp(buffer, "bye", 3) == 0) {
                printf("[-] Client disconnected or sent 'bye'.\n");
                break;
            }

            printf("Client: %s", buffer);

            // Server types reply
            printf("Server: ");
            fgets(reply, BUFFER_SIZE, stdin);

            send(client_socket, reply, strlen(reply), 0);

            if (strncmp(reply, "bye", 3) == 0) {
                printf("[-] Closing connection with this client.\n");
                break;
            }
        }

        close(client_socket);
        printf("[+] Connection closed. Server ready for next client in line.\n");
    }

    close(server_fd);
    return 0;
}
