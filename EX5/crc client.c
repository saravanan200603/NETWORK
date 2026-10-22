#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8083
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024

unsigned char bin_to_byte(const char *bin) {

    int i;
    unsigned char val = 0;
    for ( i = 0; i < 8; i++) {
        val = (val << 1) | (bin[i] - '0');
    }
    return val;
}


void byte_to_bin(unsigned char val, char *output) {

   int i;
   for (i = 7; i >= 0; i--) {
        output[7 - i] = (val & (1 << i)) ? '1' : '0';
    }
    output[8] = '\0';
}


unsigned char compute_checksum(const char *data_str, int len) {
    unsigned int sum = 0;

    int i;
    for (i = 0; i < len; i += 8) {
        unsigned char byte_val = bin_to_byte(&data_str[i]);
        sum += byte_val;

        while (sum >> 8) {
            sum = (sum & 0xFF) + (sum >> 8);
        }
    }

    return (unsigned char)(~sum & 0xFF);
}

int main() {
    int client_fd;
    char data_input[BUFFER_SIZE];
    char frame[BUFFER_SIZE+32];
    char buffer[BUFFER_SIZE];
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);


    client_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (client_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }


    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    printf("Connected to Checksum Sender Client (%s:%d)\n", SERVER_IP, PORT);

    while (1) {
        printf("\n==================================================\n");
        printf("Enter binary data bits (length must be multiple of 8, e.g., 1010101011001100) or 'exit': ");
        if (fgets(data_input, BUFFER_SIZE, stdin) == NULL) break;

        data_input[strcspn(data_input, "\r\n")] = 0;

        if (strcmp(data_input, "exit") == 0) {
            printf("Exiting client...\n");
            break;
        }

        int data_len = strlen(data_input);
        if (data_len == 0 || data_len % 8 != 0) {
            printf("[!] Invalid input! Bit length must be a multiple of 8 (e.g., 8, 16, 24 bits).\n");
            continue;
        }


        unsigned char checksum_val = compute_checksum(data_input, data_len);
        char checksum_bin[9];
        byte_to_bin(checksum_val, checksum_bin);


        snprintf(frame, sizeof(frame), "%s%s", data_input, checksum_bin);

        printf("\n[SENDER CHECKSUM COMPUTATION]\n");
        printf(" -> Calculated Checksum Byte : 0x%02X (%s)\n", checksum_val, checksum_bin);
        printf(" -> Original Frame to Send  : %s\n", frame);


        int choice;
        printf("\nDo you want to inject a bit error before sending? (1 = Yes, 0 = No): ");
        if (scanf("%d", &choice) != 1) choice = 0;

        if (choice == 1) {
            int bit_pos;
            int total_bits = strlen(frame);
            printf("Enter bit position to flip (1 to %d): ", total_bits);
            scanf("%d", &bit_pos);

            if (bit_pos >= 1 && bit_pos <= total_bits) {
                int idx = bit_pos - 1;
                frame[idx] = (frame[idx] == '0') ? '1' : '0';
                printf(" -> Corrupted Frame (Bit %d flipped): %s\n", bit_pos, frame);
            } else {
                printf("[!] Invalid position. Sending frame unchanged.\n");
            }
        }


        while (getchar() != '\n');


        sendto(client_fd, frame, strlen(frame), 0,
               (const struct sockaddr *)&server_addr, addr_len);


        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recvfrom(client_fd, buffer, BUFFER_SIZE - 1, 0,
                                      (struct sockaddr *)&server_addr, &addr_len);

        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            printf("\n[SERVER RESPONSE]: %s\n", buffer);
        }
    }

    close(client_fd);
    return 0;
}
