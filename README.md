# simple-proxy

A lightweight HTTP reverse proxy written in C using Linux `epoll` for non-blocking I/O.

```
Client ──► :3400 (proxy) ──► :8081 (backend)
```

## Basic flow 

1. The proxy listens for incoming HTTP requests on port 3400
2. Once a full request is received, it opens a connection to the backend and forwards it
3. The backend's response is forwarded back to the client
4. All I/O is non-blocking via `epoll` — a single thread handles many connections concurrently

```
event loop (epoll_wait)
  ├── new client?       → accept, start reading request
  ├── request ready?    → connect to backend, forward request
  ├── backend replied?  → forward response to client
  └── write done?       → close both connections
```

## Project layout

```
simple-proxy/
├── include/
│   ├── buffer.h       # Fixed-size read/write buffer
│   ├── connection.h   # Socket + state per connection
│   ├── event_loop.h   # epoll wrapper
│   ├── http.h         # HTTP/1.1 request & response parsing
│   └── proxy.h        # Top-level proxy type
├── src/
│   ├── buffer.c
│   ├── connection.c
│   ├── event_loop.c
│   ├── http.c
│   ├── main.c
│   └── proxy.c
├── tests/
│   ├── test_buffer.c
│   └── test_http.c
└── mock_backend.go    # Tiny Go backend for testing
```

## Build

```bash
gcc -Wall -g src/*.c -Iinclude -o simple-proxy
```

## Run

**Terminal 1 — start the backend**
```bash
go run mock_backend.go
```

**Terminal 2 — start the proxy**
```bash
./simple-proxy
# Proxy listening on :3400 → 127.0.0.1:8081
```

## Demo (Terminal 3)

**Basic request**
```bash
curl http://localhost:3400/
# Hello from the backend!
```

**Verbose — see request and response headers**
```bash
curl -v http://localhost:3400/
```

**POST with a body**
```bash
curl -X POST http://localhost:3400/ \
     -H "Content-Type: text/plain" \
     -d "hello proxy"
```

**Multiple concurrent requests**
```bash
for i in $(seq 1 10); do
    curl -s http://localhost:3400/ &
done
wait
```

**502 Bad Gateway — kill the backend first, then:**
```bash
curl -v http://localhost:3400/
# < HTTP/1.1 502 Bad Gateway
```

## Unit tests

These test the buffer and HTTP parser in isolation — no running server needed.

```bash
# Buffer tests
gcc -Wall -g tests/test_buffer.c src/buffer.c -Iinclude -o test_buffer && ./test_buffer

# HTTP parser tests
gcc -Wall -g tests/test_http.c src/http.c -Iinclude -o test_http && ./test_http
```

## Configuration

Edit the constants at the top of `src/main.c`:

```c
#define LISTEN_PORT   3400
#define BACKEND_HOST  "127.0.0.1"
#define BACKEND_PORT  8081
```

Rebuild after changing them.

## Known limitations

- No keep-alive: each request opens a new backend connection
- Buffer is fixed at 8 KB — requests or responses larger than this are rejected
- No TLS support
- Chunked transfer encoding is not parsed
