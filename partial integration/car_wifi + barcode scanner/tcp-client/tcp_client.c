#include <stdio.h>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

#define BUF_SIZE 2048

int main() {
    WSADATA wsa;
    SOCKET client_socket;
    struct sockaddr_in server;
    char server_message[BUF_SIZE];
    char IP_ADDRESS[16];
    char PORT[6];

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Winsock initialization failed. Error Code : %d", WSAGetLastError());
        return 1;
    }

    // Create a socket
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("Could not create socket : %d", WSAGetLastError());
        return 1;
    }

    // Get server IP address and port number
    printf("Enter server IP address: ");
    fgets(IP_ADDRESS, sizeof(IP_ADDRESS), stdin);
    printf("Enter server port number: ");
    fgets(PORT, sizeof(PORT), stdin);

    // Server configuration
    server.sin_addr.s_addr = inet_addr(IP_ADDRESS); // Change this to the server IP address
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(PORT)); // Change this to the server port

    // Connect to remote server
    if (connect(client_socket, (struct sockaddr*)&server, sizeof(server)) < 0) {
        puts("Connect failed");
        return 1;
    }

    puts("Welcome to MyTCPClient 1.0");
    puts("----------------------------");
    puts("You are now connected to the TCP client.");
    puts("This client allows you to receive data from servers over the TCP protocol.");
    puts("For assistance, type 'help' or '?'.");
    puts("");
    puts("Enjoy your session!");

    for (;;) {
        // Receive a reply from the server
        if (recv(client_socket, server_message, sizeof(server_message), 0) < 0) {
            puts("recv failed");
            break;
        }

        printf("Server reply: %s", server_message);
        // Clear the buffer
        memset(server_message, 0, sizeof(server_message));
    }

    // Close the socket and cleanup
    closesocket(client_socket);
    WSACleanup();

    return 0;
}
