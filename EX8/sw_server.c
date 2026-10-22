#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>

#define PORT 8080
#define PACKET_SIZE 64
#define TIMEOUT_SEC 5

typedef struct {
    int seq_num;
    int simulate_ack_loss; // 1 = Drop ACK at receiver, 0 = Normal ACK
    char data[PACKET_SIZE];
} Frame;

typedef struct {
    int ack_num;
} AckFrame;

// --- SINGLY LINKED LIST QUEUE ---
typedef struct Node {
    Frame frame;
    struct Node* next;
} Node;

typedef struct {
    Node* head;
    Node* tail;
} Queue;

void initQueue(Queue* q) {
    q->head = NULL;
    q->tail = NULL;
}

void enqueue(Queue* q, Frame f) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    newNode->frame = f;
    newNode->next = NULL;

    if (q->tail == NULL) {
        q->head = q->tail = newNode;
    } else {
        q->tail->next = newNode;
        q->tail = newNode;
    }
}

void dequeue(Queue* q) {
    if (q->head == NULL) return;

    Node* temp = q->head;
    q->head = q->head->next;

    if (q->head == NULL) {
        q->tail = NULL;
    }

    free(temp);
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Invalid address");
        return -1;
    }

    // Set 5-Second Socket Timeout for Stop-and-Wait
    struct timeval tv;
    tv.tv_sec = TIMEOUT_SEC;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    printf("[SENDER] Connecting to Receiver on port %d...\n", PORT);
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        return -1;
    }
    printf("[SENDER] Connected successfully!\n\n");

    Queue txQueue;
    initQueue(&txQueue);

    int current_seq = 0;

    while (1) {
        char buffer[PACKET_SIZE];
        printf("\n==================================================\n");
        printf("Enter message payload to send (type 'exit' to quit): ");
        fgets(buffer, sizeof(buffer), stdin);
        buffer[strcspn(buffer, "\n")] = 0;

        if (strcmp(buffer, "exit") == 0) {
            printf("[SENDER] Terminating session cleanly...\n");
            break;
        }

        // Construct Frame & Enqueue into Singly Linked List Queue
        Frame f;
        f.seq_num = current_seq;
        f.simulate_ack_loss = 0;
        strncpy(f.data, buffer, PACKET_SIZE);

        enqueue(&txQueue, f);

        int acknowledged = 0;

        // Stop-and-Wait Retransmission Loop
        while (!acknowledged) {
            Frame tx_frame = txQueue.head->frame;
            tx_frame.seq_num = current_seq; // Reset sequence number for retransmission attempt
            tx_frame.simulate_ack_loss = 0;

            printf("\nSelect Simulation Mode for Frame (Seq %d):\n", current_seq);
            printf("  1. Normal Send\n");
            printf("  2. Simulate Frame Loss / Sender Timeout\n");
            printf("  3. Simulate ACK Loss\n");
            printf("  4. Intentional Wrong Sequence Number\n");
            printf("Choice (1-4): ");

            int choice = 1;
            scanf("%d", &choice);
            while (getchar() != '\n'); // Clear buffer

            if (choice == 2) {
                printf("--> [SIMULATION] Dropping Frame %d on forward path! Skipping send()...\n", tx_frame.seq_num);
                // Do NOT call send() -> forces true socket timeout!
            } else {
                if (choice == 3) {
                    printf("--> [SIMULATION] Flags set for ACK Loss! Requesting receiver to drop ACK %d...\n", current_seq);
                    tx_frame.simulate_ack_loss = 1;
                } else if (choice == 4) {
                    printf("--> [SIMULATION] Intentionally flipping Sequence Number (%d -> %d)...\n", current_seq, current_seq ^ 1);
                    tx_frame.seq_num ^= 1; // Transmit wrong sequence number intentionally
                }

                printf("--> Transmitting Frame [Seq: %d, Data: \"%s\"] over network...\n", tx_frame.seq_num, tx_frame.data);
                send(sock, &tx_frame, sizeof(Frame), 0);
            }

            printf("--> Waiting for Auto-ACK %d (Timeout = %ds)...\n", current_seq, TIMEOUT_SEC);

            AckFrame ack;
            int bytes_read = recv(sock, &ack, sizeof(AckFrame), 0);

            if (bytes_read < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    printf("\n--> [TIMEOUT EXPIRED!] No ACK signal received within %ds!\n", TIMEOUT_SEC);
                    printf("--> [STOP-AND-WAIT RE-TRANSMIT] Resending Frame [Seq %d] from Queue Head...\n", current_seq);
                } else {
                    perror("recv error");
                    break;
                }
            } else {
                printf("--> [ACK RECEIVED] Got ACK %d from Receiver.\n", ack.ack_num);

                if (ack.ack_num == current_seq) {
                    printf("--> [SUCCESS] Correct ACK %d received! Dequeuing Head Node.\n", ack.ack_num);
                    dequeue(&txQueue); // Remove frame from Singly Linked List Queue
                    acknowledged = 1;
                    current_seq ^= 1; // Toggle sequence number (0 <-> 1)
                } else {
                    printf("--> [WRONG ACK RECEIVED] Expected ACK %d, got ACK %d. Retransmitting from Queue Head...\n", current_seq, ack.ack_num);
                }
            }
        }
    }

    close(sock);
    return 0;
}
