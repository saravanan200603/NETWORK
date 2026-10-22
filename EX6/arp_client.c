#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8787
#define BUFFER_SIZE 1024

// Request and print active system IPs from server
void fetch_available_ips(int sock) {
    char buffer[BUFFER_SIZE];
    send(sock, "LIST", 4, 0);

    printf("\n================ AVAILABLE SYSTEM IPs ================\n");
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes <= 0) break;

        if (strstr(buffer, "EOF_SIGNAL") != NULL) {
            // Print remaining data before EOF marker
            char *eof_pos = strstr(buffer, "EOF_SIGNAL");
            *eof_pos = '\0';
            printf("%s", buffer);
            break;
        }
        printf("%s", buffer);
    }
    printf("=====================================================\n");
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char ip_query[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\n Invalid address \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\n Connection Failed! Ensure server is running.\n");
        return -1;
    }

    printf("[+] Connected to Dynamic ARP Server.\n");

    // Automatically fetch and show active IPs from server upon connection
    fetch_available_ips(sock);

    printf("\n[+] Type 'list' to refresh active IPs.");
    printf("\n[+] Type 'exit' or 'bye' anytime to disconnect.\n");

    while (1) {
        printf("\nEnter Target IP Address from list above: ");
        memset(ip_query, 0, BUFFER_SIZE);

        if (fgets(ip_query, BUFFER_SIZE, stdin) == NULL) break;
        ip_query[strcspn(ip_query, "\r\n")] = 0;

        if (strncmp(ip_query, "exit", 4) == 0 || strncmp(ip_query, "bye", 3) == 0) {
            send(sock, "exit", 4, 0);
            printf("[-] Disconnecting from ARP server. Goodbye!\n");
            break;
        }

        if (strcmp(ip_query, "list") == 0) {
            fetch_available_ips(sock);
            continue;
        }

        if (strlen(ip_query) == 0) continue;

        // Query server for MAC address of chosen IP
        send(sock, ip_query, strlen(ip_query), 0);

        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);

        if (bytes_received > 0) {
            printf("[RESULT] ARP Resolution for %s:\n", ip_query);
            printf("  -> MAC Address: %s\n", buffer);
        } else {
            printf("[-] Server closed connection or no response.\n");
            break;
        }
    }

    close(sock);
    return 0;
}
