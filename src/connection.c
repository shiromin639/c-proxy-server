#include "../include/connection.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

connection_t *connection_create(int fd, conn_type_t type) {
    connection_t *conn = calloc(1, sizeof(connection_t));
    if (!conn) return NULL;

    conn->fd    = fd;
    conn->type  = type;
    conn->state = CONN_READING_REQUEST;

    buffer_init(&conn->read_buf);
    buffer_init(&conn->write_buf);

    return conn;
}

void connection_destroy(connection_t *conn) {
    if (!conn) return;
    if (conn->fd >= 0)
        close(conn->fd);
    free(conn->request_data);
    free(conn);
}

int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
        return -1;
    }
    return 0;
}
