#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8083
#define BUFFER_SIZE 1024

unsigned char bin_to_byte(const char *bin) {
    unsigned char val = 0;
    int i;
    for (i = 0; i < 8; i++) {
        val = (val << 1) | (bin[i] - '0');
    }
    return val;
}

unsigned char verify_checksum(const char *frame_str, int len) {
    unsigned int sum = 0;

    int i;

    for (i = 0; i < len; i += 8) {
        unsigned char byte_val = bin_to_byte(&frame_str[i]);
        sum += byte_val;

        while (sum >> 8) {
            sum = (sum & 0xFF) + (sum >> 8);
        }
    }

    return (unsigned char)(~sum & 0xFF);
}

int main() {
    int server_fd;
    char buffer[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }


    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Checksum Verification Server running on port %d...\n\n", PORT);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        memset(response, 0, BUFFER_SIZE);

        int bytes_received = recvfrom(server_fd, buffer, BUFFER_SIZE - 1, 0,
                                      (struct sockaddr *)&client_addr, &addr_len);
        if (bytes_received < 0) {
            perror("Recvfrom failed");
            continue;
        }

        buffer[bytes_received] = '\0';
        int frame_len = strlen(buffer);

        printf("--------------------------------------------------\n");
        printf("Received Frame from [%s:%d]:\n%s\n",
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port),
               buffer);

        unsigned char syndrome = verify_checksum(buffer, frame_len);

        if (syndrome == 0) {
            printf("[SERVER RESULT]: Checksum = 0x00 -> NO ERROR DETECTED.\n");
            snprintf(response, BUFFER_SIZE, "NO ERROR DETECTED: Data transmitted successfully!");
        } else {
            printf("[SERVER RESULT]: Checksum = 0x%02X -> ERROR DETECTED!\n", syndrome);
            snprintf(response, BUFFER_SIZE, "ERROR DETECTED: Checksum failure (Syndrome: 0x%02X). Frame corrupted!", syndrome);
        }

        sendto(server_fd, response, strlen(response), 0,
               (const struct sockaddr *)&client_addr, addr_len);
    }

    close(server_fd);
    return 0;
}
