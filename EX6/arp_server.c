#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8787
#define BUFFER_SIZE 1024

// Helper function to fetch all active IP addresses from live system ARP cache
void send_active_ip_list(int client_socket) {
    FILE *fp = fopen("/proc/net/arp", "r");
    if (fp == NULL) {
        send(client_socket, "ERROR: Unable to read system ARP table\nEOF_SIGNAL", 47, 0);
        return;
    }

    char line[BUFFER_SIZE];
    char ip_list[BUFFER_SIZE * 2] = "";
    char ip[64], hwtype[32], flags[16], mac[64], mask[64], dev[64];
    int count = 0;

    // Skip the header line in /proc/net/arp
    fgets(line, sizeof(line), fp);

    // Read live kernel entries directly
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (sscanf(line, "%s %s %s %s %s %s", ip, hwtype, flags, mac, mask, dev) == 6) {
            // Ignore incomplete / stale entries (0x0 flags)
            if (strcmp(flags, "0x0") != 0 && strchr(mac, ':') != NULL) {
                char entry[256]; // Increased size from 128 to 256 to prevent GCC truncation warning
                snprintf(entry, sizeof(entry), "  %d. %s\n", ++count, ip);

                // Ensure ip_list buffer overflow doesn't occur when appending
                if (strlen(ip_list) + strlen(entry) < sizeof(ip_list) - 1) {
                    strcat(ip_list, entry);
                }
            }
        }
    }
    fclose(fp);

    if (count == 0) {
        snprintf(ip_list, sizeof(ip_list), "[-] No active ARP entries found in system cache.\n");
    }

    // Send formatted list to client followed by completion signal
    send(client_socket, ip_list, strlen(ip_list), 0);
    usleep(20000);
    send(client_socket, "EOF_SIGNAL", 10, 0);
}
// Helper function to query dynamic MAC address from system shell on demand
// Helper function to query dynamic MAC address directly from /proc/net/arp
int get_live_mac(const char *target_ip, char *mac_output, size_t max_len) {
    FILE *fp = fopen("/proc/net/arp", "r");
    if (fp == NULL) return 0;

    char line[BUFFER_SIZE];
    char ip[64], hwtype[32], flags[16], mac[64], mask[64], dev[64];
    int found = 0;

    // Skip the header line in /proc/net/arp
    fgets(line, sizeof(line), fp);

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (sscanf(line, "%s %s %s %s %s %s", ip, hwtype, flags, mac, mask, dev) == 6) {
            if (strcmp(ip, target_ip) == 0 && strchr(mac, ':') != NULL) {
                strncpy(mac_output, mac, max_len - 1);
                mac_output[max_len - 1] = '\0';
                found = 1;
                break;
            }
        }
    }

    fclose(fp);
    return found;
}
int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE];
    char mac_result[64];

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("[+] Live Dynamic ARP Server running on port %d...\n", PORT);

    while (1) {
        printf("\n---------------------------------------------\n");
        printf("[+] Waiting for a client connection...\n");

        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            continue;
        }

        printf("[+] Client connected successfully!\n");

        while (1) {
            memset(buffer, 0, BUFFER_SIZE);
            int bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);

            if (bytes_read <= 0 || strncmp(buffer, "exit", 4) == 0 || strncmp(buffer, "bye", 3) == 0) {
                printf("[-] Client ended session.\n");
                break;
            }

            buffer[strcspn(buffer, "\r\n")] = 0;

            // Handle client request for available live IPs
            if (strcmp(buffer, "LIST") == 0) {
                printf("[+] Client requested active ARP IP list.\n");
                send_active_ip_list(client_socket);
            }
            // Handle client MAC query for specific IP
            else {
                printf("[+] Received MAC lookup request for IP: %s\n", buffer);
                memset(mac_result, 0, sizeof(mac_result));

                if (get_live_mac(buffer, mac_result, sizeof(mac_result))) {
                    printf("[+] Resolved MAC from OS: %s\n", mac_result);
                    send(client_socket, mac_result, strlen(mac_result), 0);
                } else {
                    printf("[-] IP %s not found in system ARP cache.\n", buffer);
                    send(client_socket, "ERROR: MAC Address Not Found in System Cache", 44, 0);
                }
            }
        }

        close(client_socket);
        printf("[+] Connection closed. Server ready for next client.\n");
    }

    close(server_fd);
    return 0;
}
