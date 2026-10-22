#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define PACKET_SIZE 64

typedef struct {
    int seq_num;
    int simulate_ack_loss; // 1 = Drop ACK on return path, 0 = Normal
    char data[PACKET_SIZE];
} Frame;

typedef struct {
    int ack_num;
} AckFrame;

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("[RECEIVER] Waiting for connection on port %d...\n", PORT);
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("Accept failed");
        exit(EXIT_FAILURE);
    }
    printf("[RECEIVER] Sender connected successfully!\n");

    int expected_seq = 0;

    while (1) {
        Frame rx_frame;
        int bytes_read = recv(new_socket, &rx_frame, sizeof(Frame), 0);

        if (bytes_read <= 0) {
            printf("\n[RECEIVER] Sender closed connection cleanly. Exiting...\n");
            break;
        }

        printf("\n--------------------------------------------------\n");
        printf("[RECEIVER] Received Frame -> Seq: %d | Data: \"%s\"\n", rx_frame.seq_num, rx_frame.data);

        // CHOICE 4 FIX: Sequence Number Mismatch Detection
        if (rx_frame.seq_num != expected_seq) {
            printf("--> [SEQUENCE NUMBER MISMATCH DETECTED!] Expected Seq %d, but received Seq %d!\n", expected_seq, rx_frame.seq_num);
            printf("--> Discarding unexpected payload! Resending last valid ACK %d...\n", expected_seq ^ 1);

            AckFrame ack;
            ack.ack_num = expected_seq ^ 1; // Resend last valid ACK
            send(new_socket, &ack, sizeof(AckFrame), 0);
            continue;
        }

        // Correct Expected Frame Received
        printf("--> [SUCCESS] Frame %d verified in-order!\n", rx_frame.seq_num);

        // CHOICE 3 FIX: Receiver Simulates ACK Loss by Skipping send()
        if (rx_frame.simulate_ack_loss == 1) {
            printf("--> [SIMULATION] ACK Loss requested by Sender! Skipping send() for ACK %d...\n", expected_seq);
            printf("--> Sender will now time out waiting for this ACK.\n");
            // DO NOT send ACK back!
        } else {
            printf("--> [AUTO-ACK] Transmitting ACK %d to Sender...\n", expected_seq);
            AckFrame ack;
            ack.ack_num = expected_seq;
            send(new_socket, &ack, sizeof(AckFrame), 0);

            // Advance expected sequence number (0 <-> 1)
            expected_seq ^= 1;
        }
    }

    close(new_socket);
    close(server_fd);
    return 0;
}
