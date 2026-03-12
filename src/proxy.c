#include "../include/proxy.h"
#include "../include/http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>

proxy_t *g_proxy = NULL;


proxy_t *proxy_create(int listen_port, const char *backend_host, int backend_port) {
    proxy_t *proxy = calloc(1, sizeof(proxy_t));
    if (!proxy) return NULL;

    proxy->backend_host = strdup(backend_host);
    proxy->backend_port = backend_port;
    proxy->listen_port  = listen_port;
    proxy->listen_fd    = -1;

    proxy->loop = event_loop_create();
    if (!proxy->loop) {
        free(proxy->backend_host);
        free(proxy);
        return NULL;
    }

    return proxy;
}

void proxy_destroy(proxy_t *proxy) {
    if (!proxy) return;
    if (proxy->listen_fd >= 0)
        close(proxy->listen_fd);
    event_loop_destroy(proxy->loop);
    free(proxy->backend_host);
    free(proxy);
}

int proxy_start(proxy_t *proxy) {
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", proxy->listen_port);

    struct addrinfo hints = {
        .ai_family   = AF_INET,
        .ai_socktype = SOCK_STREAM,
        .ai_flags    = AI_PASSIVE,
    };
    struct addrinfo *res = NULL;

    int gai_err = getaddrinfo(NULL, port_str, &hints, &res);
    if (gai_err != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai_err));
        return -1;
    }

    proxy->listen_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (proxy->listen_fd < 0) {
        perror("socket");
        freeaddrinfo(res);
        return -1;
    }

    int opt = 1;
    setsockopt(proxy->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(proxy->listen_fd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("bind");
        freeaddrinfo(res);
        return -1;
    }
    freeaddrinfo(res);

    if (listen(proxy->listen_fd, 128) < 0) {
        perror("listen");
        return -1;
    }

    set_nonblocking(proxy->listen_fd);

    connection_t *listen_conn = connection_create(proxy->listen_fd, CONN_CLIENT);
    event_loop_add(proxy->loop, proxy->listen_fd, EPOLLIN, listen_conn);
    event_loop_set_handler(proxy->loop, proxy_handle_event);

    printf("Proxy listening on :%d → %s:%d\n",
           proxy->listen_port, proxy->backend_host, proxy->backend_port);

    event_loop_run(proxy->loop);
    return 0;
}

void proxy_stop(proxy_t *proxy) {
    event_loop_stop(proxy->loop);
}


void proxy_handle_event(connection_t *conn, uint32_t events) {
    if (conn->fd == g_proxy->listen_fd) {
        proxy_accept_client(g_proxy);
        return;
    }

    if (events & (EPOLLERR | EPOLLHUP)) {
        fprintf(stderr, "Error/HUP on fd %d\n", conn->fd);
        proxy_close_pair(conn);
        return;
    }

    if (conn->type == CONN_CLIENT) {
        if (events & EPOLLIN)  proxy_client_read(conn);
        if (events & EPOLLOUT) proxy_client_write(conn);
    } else {
        if (events & EPOLLOUT) {
            if (conn->state == CONN_CONNECTING_BACKEND)
                proxy_backend_connected(conn);
            else
                proxy_backend_write(conn);
        }
        if (events & EPOLLIN) proxy_backend_read(conn);
    }
}


void proxy_accept_client(proxy_t *proxy) {
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);

    int fd = accept(proxy->listen_fd, (struct sockaddr *)&addr, &addrlen);
    if (fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
            perror("accept");
        return;
    }

    set_nonblocking(fd);
    connection_t *conn = connection_create(fd, CONN_CLIENT);
    event_loop_add(proxy->loop, fd, EPOLLIN, conn);

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
    printf("[+] client  fd=%-4d %s:%d\n", fd, ip, ntohs(addr.sin_port));
}


void proxy_client_read(connection_t *conn) {
    char tmp[4096];
    ssize_t n = read(conn->fd, tmp, sizeof(tmp));

    if (n <= 0) {
        if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK))
            proxy_close_pair(conn);
        return;
    }

    if (buffer_append(&conn->read_buf, tmp, (size_t)n) < 0) {
        fprintf(stderr, "Buffer overflow on client fd=%d\n", conn->fd);
        proxy_close_pair(conn);
        return;
    }

    size_t content_length = 0;
    if (!http_request_is_complete(buffer_data(&conn->read_buf),
                                  buffer_len(&conn->read_buf),
                                  &content_length))
        return;

    size_t req_len = buffer_len(&conn->read_buf);
    conn->request_data = malloc(req_len + 1);
    if (!conn->request_data) {
        proxy_close_pair(conn);
        return;
    }
    memcpy(conn->request_data, buffer_data(&conn->read_buf), req_len);
    conn->request_data[req_len] = '\0';

    char method[16], uri[256];
    http_get_method(conn->request_data, method, sizeof(method));
    http_get_uri(conn->request_data, uri, sizeof(uri));
    printf("[>] %s %s  (client fd=%d)\n", method, uri, conn->fd);

    proxy_connect_backend(conn);
}


static void send_error_to_client(connection_t *client, int status) {
    char err[512];
    int len = http_create_error_response(status, err, sizeof(err));
    if (len > 0)
        buffer_append(&client->write_buf, err, (size_t)len);
    event_loop_mod(g_proxy->loop, client->fd, EPOLLOUT);
}

void proxy_connect_backend(connection_t *client) {
    int bfd = socket(AF_INET, SOCK_STREAM, 0);
    if (bfd < 0) {
        perror("socket");
        send_error_to_client(client, 502);
        return;
    }

    set_nonblocking(bfd);

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(g_proxy->backend_port),
    };
    inet_pton(AF_INET, g_proxy->backend_host, &addr.sin_addr);

    int ret = connect(bfd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0 && errno != EINPROGRESS) {
        perror("connect");
        close(bfd);
        send_error_to_client(client, 502);
        return;
    }

    connection_t *backend = connection_create(bfd, CONN_BACKEND);
    backend->state = CONN_CONNECTING_BACKEND;

    client->peer  = backend;
    backend->peer = client;

    event_loop_add(g_proxy->loop, bfd, EPOLLOUT, backend);
    printf("[~] connecting to backend  fd=%d\n", bfd);
}


void proxy_backend_connected(connection_t *backend) {
    int error = 0;
    socklen_t len = sizeof(error);
    getsockopt(backend->fd, SOL_SOCKET, SO_ERROR, &error, &len);

    if (error != 0) {
        fprintf(stderr, "Backend connect failed: %s\n", strerror(error));
        connection_t *client = backend->peer;
        send_error_to_client(client, 502);
        proxy_close_pair(backend);
        return;
    }

    printf("[~] backend connected fd=%d\n", backend->fd);

    connection_t *client = backend->peer;
    backend->state = CONN_FORWARDING_REQUEST;

    buffer_append(&backend->write_buf,
                  client->request_data, strlen(client->request_data));

    event_loop_mod(g_proxy->loop, backend->fd, EPOLLOUT | EPOLLIN);
}


void proxy_backend_write(connection_t *backend) {
    size_t pending = buffer_len(&backend->write_buf);
    if (pending == 0) return;

    ssize_t n = write(backend->fd,
                      buffer_data(&backend->write_buf), pending);
    if (n < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("write to backend");
            proxy_close_pair(backend);
        }
        return;
    }

    buffer_consume(&backend->write_buf, (size_t)n);

    if (buffer_len(&backend->write_buf) == 0) {
        backend->state = CONN_READING_RESPONSE;
        event_loop_mod(g_proxy->loop, backend->fd, EPOLLIN);
        printf("[>] request forwarded to backend fd=%d\n", backend->fd);
    }
    /* else: more data to send, stay subscribed to EPOLLOUT */
}


void proxy_backend_read(connection_t *backend) {
    char tmp[4096];
    ssize_t n = read(backend->fd, tmp, sizeof(tmp));

    if (n <= 0) {
        if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK))
            proxy_close_pair(backend);
        return;
    }

    if (buffer_append(&backend->read_buf, tmp, (size_t)n) < 0) {
        fprintf(stderr, "Buffer overflow on backend fd=%d\n", backend->fd);
        proxy_close_pair(backend);
        return;
    }

    if (!http_response_is_complete(buffer_data(&backend->read_buf),
                                   buffer_len(&backend->read_buf)))
        return;

    printf("[<] response received from backend fd=%d (%zu bytes)\n",
           backend->fd, buffer_len(&backend->read_buf));

    connection_t *client = backend->peer;
    buffer_append(&client->write_buf,
                  buffer_data(&backend->read_buf),
                  buffer_len(&backend->read_buf));

    client->state = CONN_SENDING_RESPONSE;
    event_loop_mod(g_proxy->loop, client->fd, EPOLLOUT);
}


void proxy_client_write(connection_t *conn) {
    size_t pending = buffer_len(&conn->write_buf);
    if (pending == 0) return;

    ssize_t n = write(conn->fd, buffer_data(&conn->write_buf), pending);
    if (n < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("write to client");
            proxy_close_pair(conn);
        }
        return;
    }

    buffer_consume(&conn->write_buf, (size_t)n);

    if (buffer_len(&conn->write_buf) == 0) {
        printf("[<] response sent to client fd=%d\n", conn->fd);
        proxy_close_pair(conn);
    }
    /* else: stay subscribed to EPOLLOUT for next write */
}


void proxy_close_pair(connection_t *conn) {
    connection_t *peer = conn->peer;

    /* Detach both sides before freeing anything */
    conn->peer = NULL;
    if (peer) peer->peer = NULL;

    event_loop_del(g_proxy->loop, conn->fd);
    event_loop_remove_conn(g_proxy->loop, conn);
    connection_destroy(conn);

    if (peer) {
        event_loop_del(g_proxy->loop, peer->fd);
        event_loop_remove_conn(g_proxy->loop, peer);
        connection_destroy(peer);
    }
}
