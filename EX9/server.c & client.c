#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8888
#define BUFFER_SIZE 1024
#define HASH_SIZE 10

struct Node
{
    char mac[32];
    char ip[32];
    struct Node* next;
};

struct Node* hashTable[HASH_SIZE];

/* Hash function using MAC address */
unsigned int hash_function(const char* mac)
{
    unsigned int hash = 0;

    while (*mac)
    {
        hash = (hash * 31) + *mac++;
    }

    return hash % HASH_SIZE;
}

/* Insert MAC -> IP into hash table */
void insert_rarp(const char* mac, const char* ip)
{
    unsigned int index = hash_function(mac);

    struct Node* newNode =
        (struct Node*)malloc(sizeof(struct Node));

    strcpy(newNode->mac, mac);
    strcpy(newNode->ip, ip);

    newNode->next = hashTable[index];
    hashTable[index] = newNode;
}

/* Search IP using MAC */
char* lookup_rarp(const char* mac)
{
    unsigned int index = hash_function(mac);

    struct Node* current = hashTable[index];

    while (current != NULL)
    {
        if (strcmp(current->mac, mac) == 0)
        {
            return current->ip;
        }

        current = current->next;
    }

    return NULL;
}

/* Check whether MAC address has valid format */
int is_valid_mac(const char* mac)
{
    if (strlen(mac) != 17)
        return 0;

    for (int i = 0; i < 17; i++)
    {
        if ((i + 1) % 3 == 0)
        {
            if (mac[i] != ':')
                return 0;
        }
        else
        {
            if (!((mac[i] >= '0' && mac[i] <= '9') ||
                  (mac[i] >= 'A' && mac[i] <= 'F') ||
                  (mac[i] >= 'a' && mac[i] <= 'f')))
            {
                return 0;
            }
        }
    }

    return 1;
}

/* Load ARP cache using shell command */
void load_system_arp_cache()
{
    for (int i = 0; i < HASH_SIZE; i++)
        hashTable[i] = NULL;

    FILE *fp =
        popen("ip neigh | awk '/lladdr/ {print $5, $1}'", "r");

    if (fp == NULL)
    {
        printf("Shell command execution failed.\n");
        return;
    }

    char mac[32], ip[32];
    int count = 0;

    while (fscanf(fp, "%31s %31s", mac, ip) == 2)
    {
        insert_rarp(mac, ip);

        printf("  [Shell -> Hash Entry] MAC: %-20s -> IP: %s\n",
               mac, ip);

        count++;
    }

    pclose(fp);

    if (count == 0)
    {
        printf("  [System ARP Cache empty. Fallback localhost entry loaded...]\n");

        insert_rarp("00:00:00:00:00:00", "127.0.0.1");
    }
}

int main()
{
    int server_fd, client_socket;

    struct sockaddr_in address;

    int addrlen = sizeof(address);

    char buffer[BUFFER_SIZE];

    int opt = 1;

    /*
       Load MAC -> IP information
       from system ARP cache
    */

    printf("Executing Shell Command 'ip neigh' to build RARP Hash Table...\n");

    load_system_arp_cache();

    /* Create TCP socket */

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0)
    {
        perror("Socket creation failed");
        exit(1);
    }

    setsockopt(server_fd,
               SOL_SOCKET,
               SO_REUSEADDR,
               &opt,
               sizeof(opt));

    address.sin_family = AF_INET;

    address.sin_addr.s_addr = INADDR_ANY;

    address.sin_port = htons(PORT);

    if (bind(server_fd,
             (struct sockaddr *)&address,
             sizeof(address)) < 0)
    {
        perror("Bind failed");
        exit(1);
    }

    listen(server_fd, 5);

    printf("\nRARP Server running on port %d...\n", PORT);

    /* Iterative server */

    while (1)
    {
        client_socket =
            accept(server_fd,
                   (struct sockaddr *)&address,
                   (socklen_t*)&addrlen);

        if (client_socket < 0)
            continue;

        printf("\nClient Connected!\n");

        while (1)
        {
            memset(buffer, 0, BUFFER_SIZE);

            int bytes_read =
                recv(client_socket,
                     buffer,
                     BUFFER_SIZE - 1,
                     0);

            if (bytes_read <= 0 ||
                strncmp(buffer, "exit", 4) == 0 ||
                strncmp(buffer, "bye", 3) == 0)
            {
                printf("Client Disconnected.\n");
                break;
            }

            buffer[bytes_read] = '\0';

            buffer[strcspn(buffer, "\r\n")] = 0;

            printf("RARP Query MAC: %s\n", buffer);

            /* Check MAC address format */

            if (!is_valid_mac(buffer))
            {
                printf("Invalid MAC address format.\n");

                send(client_socket,
                     "ERROR: This is not a valid MAC address format",
                     strlen("ERROR: This is not a valid MAC address format"),
                     0);

                continue;
            }

            /* MAC -> IP lookup */

            char* ip = lookup_rarp(buffer);

            if (ip != NULL)
            {
                printf("Hash Match Found: %s\n", ip);

                send(client_socket,
                     ip,
                     strlen(ip),
                     0);
            }
            else
            {
                printf("MAC Not Found in Hash Table.\n");

                send(client_socket,
                     "ERROR: IP Not Found in Hash Table",
                     strlen("ERROR: IP Not Found in Hash Table"),
                     0);
            }
        }

        close(client_socket);
    }

    close(server_fd);

    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8888
#define BUFFER_SIZE 1024

int main()
{
    int client_socket;

    struct sockaddr_in server_address;

    char buffer[BUFFER_SIZE];

    /* Create TCP socket */

    client_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (client_socket < 0)
    {
        perror("Socket creation failed");
        exit(1);
    }

    /* Server address */

    server_address.sin_family = AF_INET;

    server_address.sin_port = htons(PORT);

    server_address.sin_addr.s_addr =
        inet_addr("127.0.0.1");

    /* Connect to server */

    if (connect(client_socket,
                (struct sockaddr *)&server_address,
                sizeof(server_address)) < 0)
    {
        perror("Connection failed");
        exit(1);
    }

    printf("Connected to RARP Server.\n");

    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);

        printf("\nEnter MAC Address (or 'exit'): ");

        fgets(buffer, BUFFER_SIZE, stdin);

        buffer[strcspn(buffer, "\r\n")] = 0;

        /* Send MAC address to server */

        send(client_socket,
             buffer,
             strlen(buffer),
             0);

        /* Exit condition */

        if (strcmp(buffer, "exit") == 0 ||
            strcmp(buffer, "bye") == 0)
        {
            break;
        }

        /* Receive IP address or error */

        memset(buffer, 0, BUFFER_SIZE);

        int bytes_read =
            recv(client_socket,
                 buffer,
                 BUFFER_SIZE - 1,
                 0);

        if (bytes_read <= 0)
        {
            printf("Server disconnected.\n");
            break;
        }

        buffer[bytes_read] = '\0';

        printf("RARP Result: %s\n", buffer);
    }

    close(client_socket);

    return 0;
}
