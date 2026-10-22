#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>

void handle_client_chat(struct sockaddr_in clientAddr, char *first_message) {
    // 1. Create a dedicated child UDP socket for this specific client session
    int child_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (child_fd < 0) {
        perror("Child socket creation failed");
        exit(1);
    }

    socklen_t addrLen = sizeof(clientAddr);
    char buffer[1024], message[1024];

    // Copy initial message sent by client
    strcpy(buffer, first_message);

    printf("\n[Child PID %d] Chat session started with client %s:%d\n",
           getpid(), inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

    while (1) {
        // Check exit command from client
        if (strcmp(buffer, "exit") == 0) {
            printf("[Child PID %d] Client disconnected.\n", getpid());
            break;
        }

        printf("\n[Child PID %d] Client: %s\n", getpid(), buffer);

        // Server input for reply
        printf("[Child PID %d] Server reply: ", getpid());
        fgets(message, sizeof(message), stdin);
        message[strcspn(message, "\r\n")] = '\0';

        // Send reply from child process socket back to client
        sendto(child_fd, message, strlen(message), 0, (struct sockaddr *)&clientAddr, addrLen);

        if (strcmp(message, "exit") == 0) {
            printf("[Child PID %d] Chat session ended by server.\n", getpid());
            break;
        }

        // Receive next message from client on child process socket
        memset(buffer, 0, sizeof(buffer));
        int n = recvfrom(child_fd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&clientAddr, &addrLen);
        if (n <= 0) {
            printf("[Child PID %d] Client connection lost.\n", getpid());
            break;
        }
        buffer[n] = '\0';
        buffer[strcspn(buffer, "\r\n")] = 0;
    }

    close(child_fd);
    exit(0); // Exit child process when session terminates
}

int main() {
    int main_sockfd;
    char buffer[1024];
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t addrLen = sizeof(clientAddr);

    // Automatically clean up zombie child processes
    signal(SIGCHLD, SIG_IGN);

    // 1. Create main listening UDP socket
    main_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (main_sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    int opt = 1;
    setsockopt(main_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(5000);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // 2. Bind main socket
    if (bind(main_sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Bind failed");
        close(main_sockfd);
        return 1;
    }

    printf("Concurrent UDP Chat Server (using fork) running on port 5000...\n");

    while (1) {
        // 3. Receive initial datagram from a new client
        memset(buffer, 0, sizeof(buffer));
        int n = recvfrom(main_sockfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&clientAddr, &addrLen);
        if (n < 0) continue;

        buffer[n] = '\0';
        buffer[strcspn(buffer, "\r\n")] = 0;

        pid_t pid = fork();

        if (pid < 0) {
            perror("Fork failed");
        } else if (pid == 0) {
            // CHILD PROCESS
            close(main_sockfd); // Close inherited main listener socket in child
            handle_client_chat(clientAddr, buffer);
        }
        // PARENT PROCESS immediately loops back to listen on port 5000
    }

    close(main_sockfd);
    return 0;
}
