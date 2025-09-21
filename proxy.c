/*
 * HTTP Proxy Server
 * Compile: gcc -o proxy proxy.c
 * Run: ./proxy <port>
 * Test: curl -x http://localhost:<port> http://www.example.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <sys/wait.h>

#define BUFFER_SIZE 4096

// Handle zombie processes
void handle_sigchld(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// Parse URL to extract host, port, and path
void parse_url(char *url, char *host, int *port, char *path) {
    *port = 80;  // default HTTP port
    strcpy(path, "/");  // default path
    
    // Work with a copy to avoid modifying the original URL
    char url_copy[1024];
    strcpy(url_copy, url);
    
    // Skip http:// if present
    char *start = (strncmp(url_copy, "http://", 7) == 0) ? url_copy + 7 : url_copy;
    
    // Find path
    char *path_start = strchr(start, '/');
    if (path_start) {
        strcpy(path, path_start);
        *path_start = '\0';  // Terminate host part
    }
    
    // Check for port
    char *port_start = strchr(start, ':');
    if (port_start) {
        *port_start = '\0';
        *port = atoi(port_start + 1);
    }
    
    strcpy(host, start);
}

// Handle client request in child process
void handle_client(int client_sock) {
    char buffer[BUFFER_SIZE];
    char method[16], url[1024], version[16];
    char host[256], path[1024];
    int port, server_sock;
    struct hostent *server;
    struct sockaddr_in server_addr;
    
    // Read request from client
    printf("[CHILD %d] Reading request from client...\n", getpid());
    memset(buffer, 0, BUFFER_SIZE);
    int bytes = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
    
    if (bytes <= 0) {
        printf("[CHILD %d] No data received from client\n", getpid());
        close(client_sock);
        exit(1);
    }
    
    printf("[CHILD %d] Received %d bytes from client\n", getpid(), bytes);
    
    // Parse HTTP request line
    if (sscanf(buffer, "%15s %1023s %15s", method, url, version) != 3) {
        printf("[CHILD %d] Invalid HTTP request format\n", getpid());
        char *error = "HTTP/1.1 400 Bad Request\r\n\r\n";
        send(client_sock, error, strlen(error), 0);
        close(client_sock);
        exit(1);
    }
    
    printf("[CHILD %d] Parsed HTTP request: %s %s %s\n", getpid(), method, url, version);
    
    // Debug: Print the first line of the HTTP request
    char first_line[256];
    strncpy(first_line, buffer, 255);
    first_line[255] = '\0';
    char *newline = strchr(first_line, '\r');
    if (newline) *newline = '\0';
    newline = strchr(first_line, '\n');
    if (newline) *newline = '\0';
    printf("[CHILD %d] HTTP Request Line: '%s'\n", getpid(), first_line);
    
    // Parse URL to extract host and port for connection
    parse_url(url, host, &port, path);
    
    printf("[CHILD %d] Parsed URL - Host: %s, Port: %d, Path: %s\n", getpid(), host, port, path);
    printf("[CHILD %d] Connecting to target server %s:%d...\n", getpid(), host, port);
    
    // Connect to target server
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        printf("[CHILD %d] Failed to create server socket\n", getpid());
        perror("Failed to create server socket");
        char *error = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
        send(client_sock, error, strlen(error), 0);
        close(client_sock);
        exit(1);
    }
    
    printf("[CHILD %d] Resolving hostname %s...\n", getpid(), host);
    server = gethostbyname(host);
    if (!server) {
        printf("[CHILD %d] Failed to resolve host: %s\n", getpid(), host);
        char *error = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
        send(client_sock, error, strlen(error), 0);
        close(server_sock);
        close(client_sock);
        exit(1);
    }
    
    printf("[CHILD %d] Host resolved to IP: %s\n", getpid(), inet_ntoa(*((struct in_addr*)server->h_addr)));
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    server_addr.sin_port = htons(port);
    
    if (connect(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("[CHILD %d] Failed to connect to target server %s:%d\n", getpid(), host, port);
        perror("Failed to connect to target server");
        char *error = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
        send(client_sock, error, strlen(error), 0);
        close(server_sock);
        close(client_sock);
        exit(1);
    }
    
    printf("[CHILD %d] Connected to target server successfully!\n", getpid());
    
    // Forward the original request from client to server
    // As per assignment requirements: "send the HTTP request that it received from the client"
    printf("[CHILD %d] Forwarding client request (%d bytes) to server...\n", getpid(), bytes);
    send(server_sock, buffer, bytes, 0);
    
    printf("[CHILD %d] Request sent to server, now forwarding response back to client...\n", getpid());
    
    // Forward response back to client
    int total_bytes = 0;
    while ((bytes = recv(server_sock, buffer, BUFFER_SIZE, 0)) > 0) {
        send(client_sock, buffer, bytes, 0);
        total_bytes += bytes;
    }
    
    printf("[CHILD %d] Forwarded %d bytes of response data to client\n", getpid(), total_bytes);
    printf("[CHILD %d] Connection completed, cleaning up...\n", getpid());
    
    // Close connections
    close(server_sock);
    close(client_sock);
    exit(0);
}

int main(int argc, char *argv[]) {
    int proxy_sock, client_sock, port, opt = 1;
    struct sockaddr_in proxy_addr, client_addr;
    socklen_t client_len;
    
    // Check command line argument
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    
    port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port number\n");
        exit(1);
    }
    
    // Setup signal handler for zombie prevention
    signal(SIGCHLD, handle_sigchld);
    signal(SIGPIPE, SIG_IGN);
    
    // Create socket
    proxy_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (proxy_sock < 0) {
        perror("socket");
        exit(1);
    }
    
    // Allow socket reuse
    setsockopt(proxy_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Setup proxy address
    memset(&proxy_addr, 0, sizeof(proxy_addr));
    proxy_addr.sin_family = AF_INET;
    proxy_addr.sin_addr.s_addr = INADDR_ANY;
    proxy_addr.sin_port = htons(port);
    
    // Bind socket
    if (bind(proxy_sock, (struct sockaddr *)&proxy_addr, sizeof(proxy_addr)) < 0) {
        perror("bind");
        exit(1);
    }
    
    // Listen for connections
    if (listen(proxy_sock, 10) < 0) {
        perror("listen");
        exit(1);
    }
    
    printf("Proxy server listening on port %d\n", port);
    printf("Waiting for client connections...\n");
    
    // Main server loop
    while (1) {
        client_len = sizeof(client_addr);
        client_sock = accept(proxy_sock, (struct sockaddr *)&client_addr, &client_len);
        
        if (client_sock < 0) continue;
        
        printf("\n[MAIN] New client connected from %s:%d\n", 
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        // Fork child process to handle client
        pid_t pid = fork();
        
        if (pid == 0) {
            // Child process
            printf("[CHILD %d] Handling client request...\n", getpid());
            close(proxy_sock);
            handle_client(client_sock);
        } else if (pid > 0) {
            // Parent process
            printf("[MAIN] Forked child process %d to handle client\n", pid);
            close(client_sock);
        }
    }
    
    close(proxy_sock);
    return 0;
}
