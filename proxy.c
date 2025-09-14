/**
Members:
John Enzu Inigo
Anjoe Paglinawan
Mae Nisha Carmel Rendon
 */

/**
 * Simple HTTP Proxy Server (Windows-only)
 *
 * Notes
 * - Handles HTTP (plaintext) only. HTTPS CONNECT tunneling is NOT implemented.
 * - Multi-client handling: thread-per-connection (Windows threads).
 * 
 * Build (MinGW):
 * C:\\mingw64\\bin\\gcc.exe -g "c:\\Users\\johne\\OneDrive\\Documents\\C code\\proxy.c" -lws2_32 -o "c:\\Users\\johne\\OneDrive\\Documents\\C code\\proxy.exe"
 * 
 * Run:
 * .\proxy.exe 8080
 * 
 * Test:
 * Browser: configure HTTP proxy 127.0.0.1:8080; open http://example.com (HTTP only).
 * Curl: 
 *  -Fetch a simple HTTP page: curl.exe -x http://127.0.0.1:8080 http://example.com/
 *  -Fetch headers only (quieter): curl.exe -I -x http://127.0.0.1:8080 http://example.com/
 *  -Verbose mode (see request/response details)
 *      >Useful for debugging: curl.exe -I -x http://127.0.0.1:8080 http://example.com/
 *  -Follow redirects (some sites redirect)
 *      >Add -L to follow Location headers: curl.exe -v -L -x http://127.0.0.1:8080 http://example.com/
 *  -Compare direct vs via proxy 
 *    >Direct(no proxy): curl.exe -I http://example.com/
 *    >Direct(via proxy): curl.exe -I -x http://127.0.0.1:8080 http://example.com/
 *  -Concurrent requests (prove threading works)
 *     >Fetch two pages simultaneously: curl.exe -s -x http://127.0.0.1:8080 http://example.com/ > $env:TEMP\out1.html 
 *                                      curl.exe -s -x http://127.0.0.1:8080 http://neverssl.com/ > $env:TEMP\out2.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <winsock2.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>   // CloseHandle
#include <ws2tcpip.h>
#include <process.h>  // _beginthreadex
#pragma comment(lib, "ws2_32.lib")

#define BUFFER_SIZE 4096
#define MAX_PENDING 10
#define DEFAULT_HTTP_PORT 80

typedef SOCKET socket_t;
#define CLOSESOCK closesocket

// URL components
typedef struct {
    char host[256];
    int port;
    char path[1024];
} URL_Info;

// Forward declarations
static void process_client_request(socket_t client_sock);

// Parse the request line: METHOD URL VERSION
static int parse_http_request(const char *request, char *method, char *url, char *version) {
    char temp[BUFFER_SIZE];
    strncpy(temp, request, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char *token = strtok(temp, " "); // METHOD
    if (!token) return -1;
    strcpy(method, token);

    token = strtok(NULL, " "); // URL
    if (!token) return -1;
    strcpy(url, token);

    token = strtok(NULL, "\r\n"); // VERSION
    if (!token) return -1;
    strcpy(version, token);

    return 0;
}

// Parse URL without modifying input
static int parse_url(const char *url, URL_Info *url_info) {
    if (!url || !*url) return -1;

    url_info->host[0] = '\0';
    url_info->port = DEFAULT_HTTP_PORT;
    strcpy(url_info->path, "/");

    const char *p = url;
    const char *scheme = "http://";
    size_t scheme_len = strlen(scheme);
    if (strncmp(p, scheme, scheme_len) == 0) p += scheme_len;

    const char *slash = strchr(p, '/');
    const char *hostport_end = slash ? slash : p + strlen(p);

    if (slash && *(slash) != '\0') {
        strncpy(url_info->path, slash, sizeof(url_info->path) - 1);
        url_info->path[sizeof(url_info->path) - 1] = '\0';
    }

    const char *colon = NULL;
    for (const char *it = p; it < hostport_end; ++it) {
        if (*it == ':') { colon = it; break; }
    }

    if (colon) {
        size_t host_len = (size_t)(colon - p);
        if (host_len >= sizeof(url_info->host)) host_len = sizeof(url_info->host) - 1;
        memcpy(url_info->host, p, host_len);
        url_info->host[host_len] = '\0';

        const char *port_str = colon + 1;
        char port_buf[16];
        size_t port_len = (size_t)(hostport_end - port_str);
        if (port_len >= sizeof(port_buf)) port_len = sizeof(port_buf) - 1;
        memcpy(port_buf, port_str, port_len);
        port_buf[port_len] = '\0';
        int parsed_port = atoi(port_buf);
        if (parsed_port > 0 && parsed_port <= 65535) url_info->port = parsed_port;
    } else {
        size_t host_len = (size_t)(hostport_end - p);
        if (host_len >= sizeof(url_info->host)) host_len = sizeof(url_info->host) - 1;
        memcpy(url_info->host, p, host_len);
        url_info->host[host_len] = '\0';
    }

    return url_info->host[0] ? 0 : -1;
}

// Connect using getaddrinfo; returns INVALID_SOCKET on error
static socket_t connect_to_host(const char *hostname, int port) {
    struct addrinfo hints; 
    struct addrinfo *res = NULL, *rp = NULL;
    char port_str[16];

    snprintf(port_str, sizeof(port_str), "%d", port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    int gai_err = getaddrinfo(hostname, port_str, &hints, &res);
    if (gai_err != 0 || !res) {
        fprintf(stderr, "Error resolving host %s: %s\n", hostname, gai_strerrorA(gai_err));
        return INVALID_SOCKET;
    }

    socket_t connected = INVALID_SOCKET;
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        socket_t s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (s == INVALID_SOCKET) continue;
        if (connect(s, rp->ai_addr, (int)rp->ai_addrlen) == SOCKET_ERROR) {
            CLOSESOCK(s);
            continue;
        }
        connected = s;
        break;
    }

    freeaddrinfo(res);

    if (connected == INVALID_SOCKET) {
        fprintf(stderr, "Error connecting to remote server\n");
    } else {
        printf("[Proxy] Connected to %s:%d\n", hostname, port);
    }
    return connected;
}

static void send_error_response(socket_t client_sock, int error_code, const char *error_msg) {
    char body[256];
    int body_len = snprintf(body, sizeof(body),
        "<html><body><h1>%d %s</h1></body></html>\r\n", error_code, error_msg);
    if (body_len < 0) body_len = 0;

    char header[256];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.0 %d %s\r\nContent-Type: text/html\r\nContent-Length: %d\r\nConnection: close\r\n\r\n",
        error_code, error_msg, body_len);
    if (header_len > 0) send(client_sock, header, (int)header_len, 0);
    if (body_len > 0) send(client_sock, body, (int)body_len, 0);
}

// Thread entry: handle one client connection
static unsigned __stdcall client_thread(void *arg) {
    socket_t s = (socket_t)(uintptr_t)arg;
    process_client_request(s);
    CLOSESOCK(s);
    return 0;
}

static void process_client_request(socket_t client_sock) {
    char buffer[BUFFER_SIZE];
    char method[16], url[1024], version[16];
    char request[BUFFER_SIZE];
    URL_Info url_info;

    int bytes_received = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0) {
        printf("[Proxy] Client disconnected or error receiving data\n");
        return;
    }
    buffer[bytes_received] = '\0';

    printf("[Proxy] Received request from client:\n%s\n", buffer);
    strncpy(request, buffer, sizeof(request) - 1);
    request[sizeof(request) - 1] = '\0';

    if (parse_http_request(buffer, method, url, version) < 0) {
        printf("[Proxy] Invalid HTTP request\n");
        send_error_response(client_sock, 400, "Bad Request");
        return;
    }

    printf("[Proxy] Method: %s, URL: %s, Version: %s\n", method, url, version);

    if (strcmp(method, "GET") != 0 && strcmp(method, "POST") != 0 && strcmp(method, "HEAD") != 0) {
        printf("[Proxy] Unsupported method: %s\n", method);
        send_error_response(client_sock, 501, "Not Implemented");
        return;
    }

    if (parse_url(url, &url_info) < 0) {
        printf("[Proxy] Error parsing URL\n");
        send_error_response(client_sock, 400, "Bad Request");
        return;
    }

    printf("[Proxy] Connecting to host: %s, port: %d, path: %s\n", url_info.host, url_info.port, url_info.path);

    socket_t server_sock = connect_to_host(url_info.host, url_info.port);
    if (server_sock == INVALID_SOCKET) {
        send_error_response(client_sock, 502, "Bad Gateway");
        return;
    }

    int bytes_sent = send(server_sock, request, (int)strlen(request), 0);
    if (bytes_sent < 0) {
        fprintf(stderr, "Error sending request to server: %d\n", WSAGetLastError());
        send_error_response(client_sock, 502, "Bad Gateway");
        CLOSESOCK(server_sock);
        return;
    }

    printf("[Proxy] Forwarded request to server\n");

    int n;
    while ((n = recv(server_sock, buffer, BUFFER_SIZE, 0)) > 0) {
        int m = send(client_sock, buffer, n, 0);
        if (m < 0) {
            fprintf(stderr, "Error sending response to client: %d\n", WSAGetLastError());
            break;
        }
    }

    printf("[Proxy] Transaction complete, closing connections\n");
    CLOSESOCK(server_sock);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Error: Invalid port number\n");
        return 1;
    }

    WSADATA wsaData;
    int wsaerr = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaerr != 0) {
        fprintf(stderr, "WSAStartup failed with error: %d\n", wsaerr);
        return 1;
    }

    socket_t proxy_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (proxy_sock == INVALID_SOCKET) {
        fprintf(stderr, "Error creating socket: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    int opt = 1;
    if (setsockopt(proxy_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) < 0) {
        fprintf(stderr, "Error setting socket options: %d\n", WSAGetLastError());
        CLOSESOCK(proxy_sock);
        WSACleanup();
        return 1;
    }

    struct sockaddr_in proxy_addr;
    memset(&proxy_addr, 0, sizeof(proxy_addr));
    proxy_addr.sin_family = AF_INET;
    proxy_addr.sin_addr.s_addr = INADDR_ANY;
    proxy_addr.sin_port = htons(port);

    if (bind(proxy_sock, (struct sockaddr *)&proxy_addr, sizeof(proxy_addr)) < 0) {
        fprintf(stderr, "Error binding socket: %d\n", WSAGetLastError());
        CLOSESOCK(proxy_sock);
        WSACleanup();
        return 1;
    }

    if (listen(proxy_sock, MAX_PENDING) < 0) {
        fprintf(stderr, "Error listening on socket: %d\n", WSAGetLastError());
        CLOSESOCK(proxy_sock);
        WSACleanup();
        return 1;
    }

    printf("[Proxy] Windows Proxy Server started on port %d\n", port);
    printf("[Proxy] Waiting for client connections...\n");

    while (1) {
        struct sockaddr_in client_addr;
        int client_len = sizeof(client_addr);
        socket_t client_sock = accept(proxy_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock == INVALID_SOCKET) {
            fprintf(stderr, "Error accepting connection: %d\n", WSAGetLastError());
            continue;
        }

        printf("[Proxy] New client connected from %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Spawn a detached thread to handle the client, allowing concurrent clients
        unsigned threadId = 0;
        uintptr_t th = _beginthreadex(NULL, 0, client_thread, (void*)(uintptr_t)client_sock, 0, &threadId);

        if (th == 0) {
            // Fallback to synchronous handling if thread creation fails
            fprintf(stderr, "Failed to create thread; handling client synchronously\n");
            process_client_request(client_sock);
            CLOSESOCK(client_sock);
        } else {
            CloseHandle((HANDLE)th); // Detach thread handle
        }
    }

    CLOSESOCK(proxy_sock);
    WSACleanup();
    return 0;
}
