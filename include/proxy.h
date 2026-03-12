#ifndef PROXY_H
#define PROXY_H

#include "event_loop.h"

typedef struct {
    int listen_fd;
    char *listen_port;
    event_loop_t *loop;
    char *backend_host;
    char *backend_port;
} proxy_t;

extern proxy_t *g_proxy;

proxy_t *proxy_create(const char *listen_port, const char *backend_host, const char *backend_port);
void     proxy_destroy(proxy_t *proxy);
int      proxy_start(proxy_t *proxy);
void     proxy_stop(proxy_t *proxy);

void proxy_handle_event(connection_t *conn, uint32_t events);
void proxy_accept_client(proxy_t *proxy);
void proxy_client_read(connection_t *conn);
void proxy_connect_backend(connection_t *client);
void proxy_backend_connected(connection_t *backend);
void proxy_backend_write(connection_t *backend);
void proxy_backend_read(connection_t *backend);
void proxy_client_write(connection_t *conn);
void proxy_close_pair(connection_t *conn);

#endif /* PROXY_H */
