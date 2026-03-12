#ifndef CONNECTION_H
#define CONNECTION_H

#include "buffer.h"

typedef enum {
    CONN_READING_REQUEST,    /* Reading HTTP request from client  */
    CONN_CONNECTING_BACKEND, /* TCP connect() in progress         */
    CONN_FORWARDING_REQUEST, /* Writing request to backend        */
    CONN_READING_RESPONSE,   /* Reading response from backend     */
    CONN_SENDING_RESPONSE,   /* Writing response to client        */
} conn_state_t;

typedef enum {
    CONN_CLIENT,  /* Accepted from a browser / downstream */
    CONN_BACKEND  /* Opened toward the upstream server    */
} conn_type_t;

typedef struct connection {
    int fd;
    conn_type_t  type;
    conn_state_t state;

    buffer_t read_buf;
    buffer_t write_buf;

    struct connection *peer; /* Client - backend link */
    char *request_data;      /* Full HTTP request */
} connection_t;

connection_t *connection_create(int fd, conn_type_t type);
void          connection_destroy(connection_t *conn);
int           set_nonblocking(int fd);

#endif /* CONNECTION_H */

