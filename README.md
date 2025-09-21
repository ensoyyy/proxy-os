# HTTP Proxy Server

A lightweight, educational HTTP proxy server implementation in C that demonstrates socket programming, process management, and network communication concepts.

## 📋 Table of Contents
- [Overview](#overview)
- [Features](#features)
- [Requirements](#requirements)
- [Installation](#installation)
- [Usage](#usage)
- [How It Works](#how-it-works)
- [Code Structure](#code-structure)
- [Testing](#testing)
- [Troubleshooting](#troubleshooting)
- [Educational Notes](#educational-notes)
- [License](#license)

## 🔍 Overview

This HTTP proxy server acts as an intermediary between HTTP clients (like web browsers or curl) and web servers. When a client makes a request through the proxy, the server forwards the request to the target server and then relays the response back to the client.

```
Client ↔ Proxy Server ↔ Target Server
```

## ✨ Features

- **HTTP/1.1 Support**: Handles standard HTTP requests and responses
- **Concurrent Connections**: Uses process forking to handle multiple clients simultaneously
- **URL Parsing**: Extracts host, port, and path information from URLs
- **DNS Resolution**: Automatically resolves hostnames to IP addresses
- **Error Handling**: Comprehensive error handling with appropriate HTTP status codes
- **Detailed Logging**: Extensive debug output for monitoring and troubleshooting
- **Memory Safe**: Proper memory management and buffer handling
- **Signal Handling**: Prevents zombie processes through proper signal management

## 📋 Requirements

- **Operating System**: Linux/Unix-based system
- **Compiler**: GCC (GNU Compiler Collection)
- **Dependencies**: Standard C libraries (no external dependencies)
- **Architecture**: x86_64 or compatible

### System Requirements
```bash
# Check if you have GCC installed
gcc --version

# Check if you have standard development tools
which make
```

## 🚀 Installation

1. **Clone or download the source code**:
   ```bash
   # If using git
   git clone <repository-url>
   cd <repository-name>
   
   # Or download the proxy.c file directly
   ```

2. **Compile the proxy server**:
   ```bash
   gcc -o proxy proxy.c
   ```

3. **Make it executable** (if needed):
   ```bash
   chmod +x proxy
   ```

## 💻 Usage

### Starting the Proxy Server

```bash
./proxy <port>
```

**Example**:
```bash
./proxy 8080
```

This starts the proxy server listening on port 8080.

### Using the Proxy

#### With curl:
```bash
curl -x http://localhost:8080 http://www.example.com
```

#### With wget:
```bash
wget -e http_proxy=http://localhost:8080 http://www.example.com
```

#### With web browsers:
Configure your browser's HTTP proxy settings to:
- **Proxy server**: `localhost`
- **Port**: `8080` (or whatever port you specified)

### Command Line Options

| Option | Description | Example |
|--------|-------------|---------|
| `<port>` | Port number to listen on (1-65535) | `./proxy 3128` |

### Example Session

```bash
# Terminal 1: Start the proxy
$ ./proxy 8080
Proxy server listening on port 8080
Waiting for client connections...

# Terminal 2: Make a request through the proxy
$ curl -x http://localhost:8080 http://www.example.com
<!doctype html>
<html>
<head>
    <title>Example Domain</title>
...
```

## 🔧 How It Works

### 1. **Server Initialization**
- Creates a TCP socket
- Binds to the specified port
- Listens for incoming connections

### 2. **Client Connection Handling**
- Accepts incoming client connections
- Forks a child process for each client (concurrent handling)
- Parent process continues listening for new connections

### 3. **Request Processing**
```
[Client Request] → [Parse HTTP] → [Extract URL] → [DNS Resolution] → [Connect to Target]
```

### 4. **Data Forwarding**
```
[Client] ←→ [Proxy] ←→ [Target Server]
```

### 5. **Connection Cleanup**
- Closes all sockets
- Terminates child process
- Parent process cleans up zombie processes

## 📁 Code Structure

```
proxy.c
├── Headers & Includes
├── handle_sigchld()      # Zombie process cleanup
├── parse_url()           # URL parsing and validation
├── handle_client()       # Main client handling logic
└── main()               # Server initialization and main loop
```

### Key Functions

| Function | Purpose |
|----------|---------|
| `main()` | Server setup, socket creation, main accept loop |
| `handle_client()` | Process individual client requests |
| `parse_url()` | Extract host, port, and path from URLs |
| `handle_sigchld()` | Clean up terminated child processes |

## 🧪 Testing

### Basic Functionality Test
```bash
# Start proxy
./proxy 8080 &

# Test with simple HTTP request
curl -x http://localhost:8080 http://www.example.com

# Test with different websites
curl -x http://localhost:8080 http://info.cern.ch/
curl -x http://localhost:8080 http://www.google.com
```

### Concurrent Connection Test
```bash
# Test multiple simultaneous connections
curl -x http://localhost:8080 http://www.example.com &
curl -x http://localhost:8080 http://info.cern.ch/ &
curl -x http://localhost:8080 http://www.google.com &
wait
```

### Error Handling Test
```bash
# Test with invalid hostname
curl -x http://localhost:8080 http://this-host-does-not-exist.com

# Test with invalid port
curl -x http://localhost:8080 http://www.example.com:99999
```

## 🔧 Troubleshooting

### Common Issues

#### "Address already in use" error
```bash
# Solution: Kill existing proxy processes
pkill -f "./proxy"
# Or wait a few seconds and try again
```

#### "Permission denied" on low ports
```bash
# Ports below 1024 require root privileges
sudo ./proxy 80
# Better: use ports above 1024
./proxy 8080
```

#### "Connection refused" errors
```bash
# Check if proxy is actually running
ps aux | grep proxy

# Check if port is open
netstat -tlnp | grep :8080
```

### Debug Output

The proxy provides detailed logging output:

```
[MAIN] New client connected from 127.0.0.1:45678
[MAIN] Forked child process 12345 to handle client
[CHILD 12345] Reading request from client...
[CHILD 12345] Received 130 bytes from client
[CHILD 12345] Parsed HTTP request: GET http://www.example.com/ HTTP/1.1
[CHILD 12345] Parsed URL - Host: www.example.com, Port: 80, Path: /
[CHILD 12345] Connecting to target server www.example.com:80...
[CHILD 12345] Resolving hostname www.example.com...
[CHILD 12345] Host resolved to IP: 93.184.216.34
[CHILD 12345] Connected to target server successfully!
[CHILD 12345] Forwarding client request (130 bytes) to server...
[CHILD 12345] Request sent to server, now forwarding response back to client...
[CHILD 12345] Forwarded 1256 bytes of response data to client
[CHILD 12345] Connection completed, cleaning up...
```

## 📚 Educational Notes

### Networking Concepts Demonstrated
- **Socket Programming**: Creating, binding, listening, accepting connections
- **TCP/IP Communication**: Reliable, connection-oriented data transfer
- **DNS Resolution**: Converting hostnames to IP addresses
- **HTTP Protocol**: Parsing and forwarding HTTP requests/responses
- **Process Forking**: Creating child processes for concurrent handling
- **Signal Handling**: Managing process lifecycle and cleanup

### C Programming Concepts
- **Pointers and Arrays**: String manipulation and memory management
- **Structures**: Network address structures (`sockaddr_in`, `hostent`)
- **System Calls**: Low-level operating system interface
- **Error Handling**: Checking return values and handling failures
- **Memory Management**: Proper allocation, copying, and cleanup

### Security Considerations
⚠️ **Educational Purpose Only**: This proxy is designed for learning and should not be used in production environments without additional security measures:

- No authentication or access control
- No HTTPS/SSL support
- No request filtering or validation
- No rate limiting or abuse protection
- Potential for memory leaks in error conditions

## 📄 License

This project is provided for educational purposes. Feel free to use, modify, and distribute for learning and non-commercial purposes.

---

## 📞 Support

If you encounter issues or have questions:

1. Check the [Troubleshooting](#troubleshooting) section
2. Review the debug output for error messages
3. Verify your system meets the [Requirements](#requirements)
4. Test with simple HTTP sites first (avoid HTTPS)

## 🔗 Additional Resources

- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)
- [HTTP/1.1 Specification (RFC 2616)](https://tools.ietf.org/html/rfc2616)
- [Socket Programming Tutorial](https://www.tutorialspoint.com/unix_sockets/index.htm)
- [C Programming Language Reference](https://en.cppreference.com/w/c)

---

**Happy Learning! 🎓**
