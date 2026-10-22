#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 5454
#define BUFFER_SIZE 1024

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char message[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];

    // 1. Create Socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 address from text to binary (127.0.0.1 for localhost)
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\n Invalid address / Address not supported \n");
        return -1;
    }

    // 2. Connect to Server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\n Connection Failed! Make sure server is running.\n");
        return -1;
    }

    printf("[+] Connected to Chat Server!\n");
    printf("[+] Type your message below (type 'bye' to quit):\n\n");

    // Chat Loop
    while (1) {
        memset(message, 0, BUFFER_SIZE);
        memset(buffer, 0, BUFFER_SIZE);

        printf("Client: ");
        fgets(message, BUFFER_SIZE, stdin);

        // Send to Server
        send(sock, message, strlen(message), 0);

        if (strncmp(message, "bye", 3) == 0) {
            printf("[-] Disconnecting...\n");
            break;
        }

        // Receive Server Reply
        int bytes_read = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_read <= 0 || strncmp(buffer, "bye", 3) == 0) {
            printf("[-] Server ended the chat.\n");
            break;
        }

        printf("Server: %s", buffer);
    }

    close(sock);
    return 0;
}
