#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define PACKET_SIZE 64

typedef struct {
    int seq_num;
    int simulate_ack_loss;
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

    printf("[RECEIVER] Waiting for Sender connection on port %d...\n", PORT);
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("Accept failed");
        exit(EXIT_FAILURE);
    }
    printf("[RECEIVER] Sender connected successfully!\n");

    int window_size = 0;

    // Receive window size N from Sender to compute the matching dynamic modulo
    recv(new_socket, &window_size, sizeof(int), 0);
    int modulo = 1;
    while (modulo <= window_size) {
        modulo *= 2;
    }
    printf("[RECEIVER CONFIG] Window Size (N) = %d | Sequence Modulo (M) = %d\n\n", window_size, modulo);

    // --- PHASE 1: INITIAL BURST RECEPTION (seq 0 .. N-1) ---
    printf("--- RECEIVING INITIAL BURST OF %d FRAMES ---\n", window_size);
    int last_burst_seq = 0;
    for (int i = 0; i < window_size; i++) {
        Frame rx_frame;
        int bytes_read = recv(new_socket, &rx_frame, sizeof(Frame), 0);
        if (bytes_read <= 0) break;

        last_burst_seq = rx_frame.seq_num;
        printf("[RECEIVER] Received Burst Frame [%d/%d] -> Seq: %d | Data: \"%s\"\n",
               i + 1, window_size, rx_frame.seq_num, rx_frame.data);
    }

    // The receiver only ever ACKs the OLDEST outstanding frame in the
    // window (that's what tells the sender it may slide by one). Right
    // after the burst, the oldest outstanding frame is seq 0, which this
    // offset formula reduces to.
    int n_offset = window_size - 1;
    int last_ack = (last_burst_seq - n_offset + modulo) % modulo;

    printf("\n--> [INITIAL ACK] Oldest outstanding frame = (%d - %d) mod %d = %d\n",
           last_burst_seq, n_offset, modulo, last_ack);
    printf("--> Transmitting ACK %d to allow the window to start sliding...\n", last_ack);

    AckFrame init_ack;
    init_ack.ack_num = last_ack;
    send(new_socket, &init_ack, sizeof(AckFrame), 0);

    // --- PHASE 2: CONTINUOUS SLIDING WINDOW LOOP ---
    //
    // Key invariant: once the window is full (size N) and sliding one frame
    // at a time, the ONE legitimate "new" frame the receiver should ever be
    // waiting for next has sequence number (last_ack + window_size) % modulo.
    // That's true whether this is the very next fresh frame from the sender,
    // OR the last frame of a Go-Back-N retransmission burst.
    //
    // Anything else arriving (i.e. the earlier frames of a retransmit burst,
    // which the receiver already saw and already accounted for) is simply
    // discarded with no ACK sent - we just keep waiting for the one frame
    // that actually matters.
    while (1) {
        int expected_new_seq = (last_ack + window_size) % modulo;

        Frame rx_frame;
        int bytes_read = recv(new_socket, &rx_frame, sizeof(Frame), 0);

        if (bytes_read <= 0) {
            printf("\n[RECEIVER] Sender disconnected cleanly. Exiting...\n");
            break;
        }

        printf("\n--------------------------------------------------\n");
        printf("[RECEIVER] Frame Arrived -> Seq: %d | Data: \"%s\"\n", rx_frame.seq_num, rx_frame.data);

        if (rx_frame.seq_num != expected_new_seq) {
            printf("--> [DISCARD] Not the frame we're waiting for (expected Seq %d). "
                   "Likely a Go-Back-N retransmit of an already-tracked frame - ignoring, no ACK sent.\n",
                   expected_new_seq);
            continue;
        }

        printf("--> [SUCCESS] This is the fresh frame we expected (Seq %d)!\n", rx_frame.seq_num);

        if (rx_frame.simulate_ack_loss == 1) {
            printf("--> [SIMULATION] ACK Loss simulated! Withholding ACK for Seq %d.\n", rx_frame.seq_num);
            printf("--> Sender will time out and Go-Back-N retransmit the whole window.\n");
            continue; // do NOT advance last_ack - we're still waiting on the same frame
        }

        int new_ack = (last_ack + 1) % modulo;
        printf("--> [AUTO-ACK] Window slides. Transmitting ACK %d to Sender...\n", new_ack);

        AckFrame ack;
        ack.ack_num = new_ack;
        send(new_socket, &ack, sizeof(AckFrame), 0);

        last_ack = new_ack;
    }

    close(new_socket);
    close(server_fd);
    return 0;
}
