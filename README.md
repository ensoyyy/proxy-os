# Simple HTTP Proxy — Beginner Guide

Members: John Enzu Inigo, Anjoe Paglinawan, Mae Nisha Carmel Rendon

This is a small program that sits in the middle between your browser (or curl) and real websites. It reads your request, sends it to the website, gets the website’s reply, and gives it back to you.

We will use a single file in this project: `proxy.c` (Windows‑only version in your repo). It handles many clients at the same time using Windows threads.

Tip: This proxy is HTTP‑only (use URLs that start with http://). HTTPS (https://) is not supported in this version.

## Quick start (Windows)
1) Build
```powershell
C:\mingw64\bin\gcc.exe -g "c:\Users\johne\OneDrive\Documents\C code\proxy.c" -lws2_32 -o "c:\Users\johne\OneDrive\Documents\C code\proxy.exe"
```
2) Run
```powershell
.\proxy.exe 8080
```
3) Test with curl
```powershell
curl.exe -x http://127.0.0.1:8080 http://example.com/
```
If you see HTML in the terminal and the proxy window prints logs (new client, connecting, forwarded, complete), it works!

## What the proxy does (in plain steps)
1. Start and listen on the port you type (no hard‑coded port).
2. Wait for a client to connect (any IP address can connect).
3. Read the first line of the request (like: GET http://example.com/ HTTP/1.1).
4. If the request is invalid, reply with 400 Bad Request.
5. If valid, extract three things from the URL: host, port (or 80 by default), and path.
6. Connect to that host and send the same request the client sent.
7. Read the server’s reply and send it back to the client. Then close the connection.

## About the code file
- `proxy.c` is set up in your repo as a Windows‑only proxy using Winsock and a thread‑per‑connection model. It logs each step so you can see what’s happening.

## More curl examples
- See only the headers
```powershell
curl.exe -I -x http://127.0.0.1:8080 http://example.com/
```
- Be verbose (show details) and follow redirects
```powershell
curl.exe -v -L -x http://127.0.0.1:8080 http://neverssl.com/
```
- Test concurrency (run both in separate terminals)
```powershell
curl.exe -s -x http://127.0.0.1:8080 http://example.com/ > $env:TEMP\out1.html
curl.exe -s -x http://127.0.0.1:8080 http://neverssl.com/ > $env:TEMP\out2.html
```

## Where things are in the code (proxy.c)
- `parse_http_request` — reads METHOD, URL, VERSION from the first line.
- `parse_url` — pulls out host, optional port (default 80), and path.
- `connect_to_host` — DNS look‑up and connect using `getaddrinfo`.
- `process_client_request` — the main flow: read → parse → connect → forward → relay.

## Common problems & quick fixes
- “curl” shows PowerShell help: use `curl.exe` to run the real curl.
- No output or errors: try a plain HTTP site like `http://neverssl.com/` or `http://example.com/`.
- HTTPS fails: expected — this proxy does not implement HTTPS CONNECT.
- Build error on Windows: ensure `-lws2_32` is present when compiling.

## Glossary (super short)
- Socket — The endpoint you read/write bytes to over the network.
- Proxy — A middleman program that forwards requests and responses.
- Thread — A lightweight path of execution that lets your program do many things at once.

You now have a working HTTP proxy, a simple way to test it, and a clear map of where to look in the code. If you want, we can add HTTPS support or add timeouts/logging next.
