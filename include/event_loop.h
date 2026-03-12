#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include "connection.h"
#include <sys/epoll.h>

#define MAX_EVENTS      64
#define MAX_CONNECTIONS 1024

typedef void (*event_handler_t)(connection_t *conn, uint32_t events);

typedef struct {
    int epoll_fd;
    struct epoll_event events[MAX_EVENTS];

    connection_t *connections[MAX_CONNECTIONS];
    int conn_count;

    event_handler_t handler;
    int running;
} event_loop_t;

event_loop_t *event_loop_create(void);
void          event_loop_destroy(event_loop_t *loop);

int  event_loop_add(event_loop_t *loop, int fd, uint32_t events, connection_t *conn);
int  event_loop_mod(event_loop_t *loop, int fd, uint32_t events);
int  event_loop_del(event_loop_t *loop, int fd);

void event_loop_set_handler(event_loop_t *loop, event_handler_t handler);
void event_loop_run(event_loop_t *loop);
void event_loop_stop(event_loop_t *loop);

int           event_loop_add_conn(event_loop_t *loop, connection_t *conn);
void          event_loop_remove_conn(event_loop_t *loop, connection_t *conn);
connection_t *event_loop_find_conn(event_loop_t *loop, int fd);

#endif /* EVENT_LOOP_H */
