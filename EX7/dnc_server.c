#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>

#define TABLE_SIZE 10

// Hash Node structure for chaining collision resolution
struct HashNode {
    char domain[100];
    char ip[100];
    struct HashNode *next;
};

// Hash Table structure
struct HashNode *hashTable[TABLE_SIZE];

// Simple Hash Function
unsigned int hash(const char *str) {
    unsigned int hashValue = 0;
    while (*str) {
        hashValue = (hashValue * 31) + *str;
        str++;
    }
    return hashValue % TABLE_SIZE;
}

// Insert domain-IP record into hash table
void insert(const char *domain, const char *ip) {
    unsigned int slot = hash(domain);
    struct HashNode *newNode = (struct HashNode *)malloc(sizeof(struct HashNode));
    strcpy(newNode->domain, domain);
    strcpy(newNode->ip, ip);
    newNode->next = hashTable[slot];
    hashTable[slot] = newNode;
}

// Lookup domain in hash table
const char* search(const char *domain) {
    unsigned int slot = hash(domain);
    struct HashNode *current = hashTable[slot];
    while (current != NULL) {
        if (strcmp(current->domain, domain) == 0) {
            return current->ip;
        }
        current = current->next;
    }
    return NULL;
}

// Initialize Hash Table database
void initDNSDatabase() {
    for (int i = 0; i < TABLE_SIZE; i++) {
        hashTable[i] = NULL;
    }

    insert("vscode.com", "192.168.1.10");
    insert("google.com", "142.250.190.46");
    insert("github.com", "140.82.121.4");
    insert("chatgpt.com", "93.184.216.34");
    insert("openai.com", "104.18.7.162");
    insert("microsoft.com", "151.101.65.69");
    insert("wikipedia.org", "208.80.154.224");
    insert("python.org", "151.101.193.223");
}

void handle_client_session(struct sockaddr_in clientAddr, char *first_domain) {
    // Create dedicated child socket
    int child_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (child_fd < 0) {
        perror("Child socket creation failed");
        exit(1);
    }

    socklen_t addrLen = sizeof(clientAddr);
    char domain[1024];
    strcpy(domain, first_domain);

    while (1) {
        printf("Child [PID %d] processing query for: %s\n", getpid(), domain);

        // Search domain in database
        const char *found_ip = search(domain);
        char response[1024];

        if (found_ip != NULL) {
            strcpy(response, found_ip);
        } else {
            strcpy(response, "Error: Domain not found");
        }

        // Send response back using child socket
        sendto(child_fd, response, strlen(response), 0, (struct sockaddr *)&clientAddr, addrLen);

        // Receive subsequent requests from client in this session
        memset(domain, 0, sizeof(domain));
        int n = recvfrom(child_fd, domain, sizeof(domain) - 1, 0, (struct sockaddr *)&clientAddr, &addrLen);
        if (n <= 0) {
            printf("Child [PID %d]: Client session ended.\n", getpid());
            break;
        }
        domain[n] = '\0';
        domain[strcspn(domain, "\r\n")] = 0;
    }

    close(child_fd);
    exit(0); // Terminate child process when client session ends
}

int main() {
    int main_sockfd;
    char buffer[1024];
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t addrLen = sizeof(clientAddr);

    // Prevent zombie processes
    signal(SIGCHLD, SIG_IGN);

    // Initialize database
    initDNSDatabase();

    // 1. Create UDP socket
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

    // 2. Bind socket
    if (bind(main_sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Bind failed");
        close(main_sockfd);
        return 1;
    }

    printf("Concurrent UDP DNS Server (using fork) running on port 5000...\n");

    // 3. Concurrent parent loop
    while (1) {
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
            close(main_sockfd); // Close inherited main socket handle inside child
            printf("Child process created (PID: %d) for client %s:%d\n",
                   getpid(), inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
            handle_client_session(clientAddr, buffer);
        }
        // PARENT PROCESS loops back immediately to handle other clients on port 5000
    }

    close(main_sockfd);
    return 0;
}
