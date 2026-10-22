#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>

#define PORT 8080
#define PACKET_SIZE 64
#define TIMEOUT_SEC 4

typedef struct {
    int seq_num;
    int simulate_ack_loss;   // set to 1 to tell the receiver to swallow this frame's ACK
    char data[PACKET_SIZE];
} Frame;

typedef struct {
    int ack_num;
} AckFrame;

// --- SINGLY LINKED LIST WINDOW QUEUE ---
// Holds exactly the frames currently "in flight" (unacknowledged). Its head
// is always the oldest outstanding frame, i.e. the one we are waiting on an
// ACK for. The queue size stays equal to the window size N at all times
// once the window has been filled.
typedef struct Node {
    Frame frame;
    struct Node* next;
} Node;

typedef struct {
    Node* head;
    Node* tail;
    int count;
} Queue;

void initQueue(Queue* q) {
    q->head = NULL;
    q->tail = NULL;
    q->count = 0;
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
    q->count++;
}

void dequeue(Queue* q) {
    if (q->head == NULL) return;

    Node* temp = q->head;
    q->head = q->head->next;

    if (q->head == NULL) {
        q->tail = NULL;
    }

    free(temp);
    q->count--;
}

// Go-Back-N recovery: resend every frame currently sitting in the window,
// in order, starting from the head. Always clear simulate_ack_loss on the
// retransmitted copy so we don't loop forever simulating loss.
void retransmitQueue(int sock, Queue* q) {
    printf("\n--> [GO-BACK-N] Timeout/mismatch! Retransmitting entire window (%d frames) from head...\n", q->count);
    Node* curr = q->head;
    while (curr != NULL) {
        Frame f = curr->frame;
        f.simulate_ack_loss = 0;
        printf("    Retransmitting Frame [Seq: %d, Data: \"%s\"]\n", f.seq_num, f.data);
        send(sock, &f, sizeof(Frame), 0);
        curr = curr->next;
    }
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

    int window_size;
    printf("Enter Go-Back-N Window Size N (e.g., 2, 4, 6): ");
    scanf("%d", &window_size);
    while (getchar() != '\n');

    send(sock, &window_size, sizeof(int), 0);

    // Dynamic modulo: smallest power of 2 strictly greater than N, so
    // sequence numbers never alias within one window's worth of frames.
    int modulo = 1;
    while (modulo <= window_size) {
        modulo *= 2;
    }
    printf("[SENDER CONFIG] Window Size (N) = %d | Sequence Modulo (M) = %d\n\n", window_size, modulo);

    Queue txQueue;
    initQueue(&txQueue);

    int raw_seq_counter = 0; // ever-increasing; actual wire seq = raw_seq_counter % modulo

    // --- STEP 1: INITIAL WINDOW FILL (burst-send N frames, seq 0..N-1) ---
    printf("--- INITIAL WINDOW FILL (%d frames) ---\n", window_size);
    for (int i = 0; i < window_size; i++) {
        char buffer[PACKET_SIZE];
        int current_seq = raw_seq_counter % modulo;

        printf("Enter payload for Frame (Seq %d) [%d/%d]: ", current_seq, i + 1, window_size);
        fgets(buffer, sizeof(buffer), stdin);
        buffer[strcspn(buffer, "\n")] = 0;

        Frame f;
        f.seq_num = current_seq;
        f.simulate_ack_loss = 0;
        strncpy(f.data, buffer, PACKET_SIZE);

        enqueue(&txQueue, f);
        raw_seq_counter++;

        printf("--> Sending Initial Frame [Seq: %d, Data: \"%s\"]\n", f.seq_num, f.data);
        send(sock, &f, sizeof(Frame), 0);
    }

    // --- STEP 2: CONTINUOUS SLIDING WINDOW LOOP ---
    // The receiver only ever ACKs the oldest outstanding frame (the queue
    // head). One ACK slides the window by exactly one frame.
    while (1) {
        int expected_ack = txQueue.head->frame.seq_num;
        printf("\nWaiting for ACK on Queue Head (Seq %d)...\n", expected_ack);

        AckFrame ack;
        int bytes_read = recv(sock, &ack, sizeof(AckFrame), 0);

        if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("\n--> [TIMEOUT EXPIRED!] No ACK received within %ds!\n", TIMEOUT_SEC);
                retransmitQueue(sock, &txQueue);
            } else {
                perror("recv error");
                break;
            }
            continue;
        }

        if (bytes_read == 0) {
            printf("[SENDER] Receiver closed connection.\n");
            break;
        }

        printf("--> Received ACK %d from Receiver.\n", ack.ack_num);

        if (ack.ack_num != expected_ack) {
            printf("--> [UNEXPECTED ACK] Expected %d, got %d. Treating as loss.\n", expected_ack, ack.ack_num);
            retransmitQueue(sock, &txQueue);
            continue;
        }

        printf("--> [ACK MATCH] Sliding window: dequeuing Seq %d.\n", expected_ack);
        dequeue(&txQueue);

        char buffer[PACKET_SIZE];
        int next_seq = raw_seq_counter % modulo;

        printf("\n==================================================\n");
        printf("Window Slid! Enter payload for next Frame (Seq %d) ('exit' to quit): ", next_seq);
        fgets(buffer, sizeof(buffer), stdin);
        buffer[strcspn(buffer, "\n")] = 0;

        if (strcmp(buffer, "exit") == 0) {
            printf("[SENDER] Terminating session cleanly...\n");
            break;
        }

        Frame f;
        f.seq_num = next_seq;
        f.simulate_ack_loss = 0;
        strncpy(f.data, buffer, PACKET_SIZE);

        enqueue(&txQueue, f);
        raw_seq_counter++;

        printf("\nSelect Simulation Mode for Frame (Seq %d):\n", next_seq);
        printf("  1. Normal Send\n");
        printf("  2. Simulate ACK Loss\n");
        printf("  3. Intentional Wrong Sequence Number\n");
        printf("Choice (1-3): ");

        int choice = 1;
        scanf("%d", &choice);
        while (getchar() != '\n');

        Frame tx_frame = txQueue.tail->frame; // copy: corrupting it for the wire must not corrupt the stored copy

        if (choice == 2) {
            printf("--> [SIMULATION] Flagging ACK Loss for Frame Seq %d...\n", tx_frame.seq_num);
            txQueue.tail->frame.simulate_ack_loss = 1; // stored copy remembers, for correctness of future retransmits
            tx_frame.simulate_ack_loss = 1;
        } else if (choice == 3) {
            int corrupted_seq = (tx_frame.seq_num + 1) % modulo;
            printf("--> [SIMULATION] Intentionally corrupting Seq Num on the wire (%d -> %d)...\n", tx_frame.seq_num, corrupted_seq);
            tx_frame.seq_num = corrupted_seq; // stored copy keeps the correct seq for retransmission
        }

        printf("--> Transmitting Frame [Seq: %d, Data: \"%s\"] over network...\n", tx_frame.seq_num, tx_frame.data);
        send(sock, &tx_frame, sizeof(Frame), 0);
    }

    close(sock);
    return 0;
}
