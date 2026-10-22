#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8081
#define BUFFER_SIZE 1024

// Helper function to reverse a string in-place
void reverse_string(char *str, int len) {
    int start = 0;
    int end = len - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

int main() {
    int server_fd;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    //Create a UDP Socket
    server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    //  Configure server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    //Bind socket to Port 8080
    if (bind(server_fd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("String Reversal Server running on port %d...\n", PORT);

    //  Main processing loop
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);

        int bytes_received = recvfrom(server_fd, buffer, BUFFER_SIZE - 1, 0,
                                      (struct sockaddr *)&client_addr, &addr_len);
        if (bytes_received < 0) {
            perror("Recvfrom failed");
            continue;
        }

        // Print original received string
        printf("Received from [%s:%d]: \"%s\"\n",
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port),
               buffer);

        // Reverse the string
        reverse_string(buffer, bytes_received);

        printf("Reversed string: \"%s\"\n", buffer);

        // Send reversed string back to client
        sendto(server_fd, buffer, strlen(buffer), 0,
               (const struct sockaddr *)&client_addr, addr_len);
    }

    close(server_fd);
    return 0;
}
