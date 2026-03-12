#include "../include/event_loop.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

event_loop_t *event_loop_create(void) {
    event_loop_t *loop = calloc(1, sizeof(event_loop_t));
    if (!loop) return NULL;

    loop->epoll_fd = epoll_create1(0);
    if (loop->epoll_fd == -1) {
        perror("epoll_create1");
        free(loop);
        return NULL;
    }
    return loop;
}

void event_loop_destroy(event_loop_t *loop) {
    if (!loop) return;
    for (int i = 0; i < loop->conn_count; i++)
        connection_destroy(loop->connections[i]);
    close(loop->epoll_fd);
    free(loop);
}

int event_loop_add(event_loop_t *loop, int fd, uint32_t events, connection_t *conn) {
    struct epoll_event ev = { .events = events, .data.fd = fd };
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1)
        return -1;
    return event_loop_add_conn(loop, conn);
}

int event_loop_mod(event_loop_t *loop, int fd, uint32_t events) {
    struct epoll_event ev = { 
        .events = events, 
        .data.fd = fd 
    };
    return epoll_ctl(loop->epoll_fd, EPOLL_CTL_MOD, fd, &ev);
}

int event_loop_del(event_loop_t *loop, int fd) {
    return epoll_ctl(loop->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
}

void event_loop_set_handler(event_loop_t *loop, event_handler_t handler) {
    loop->handler = handler;
}

int event_loop_add_conn(event_loop_t *loop, connection_t *conn) {
    if (loop->conn_count >= MAX_CONNECTIONS)
        return -1;
    loop->connections[loop->conn_count++] = conn;
    return 0;
}

void event_loop_remove_conn(event_loop_t *loop, connection_t *conn) {
    for (int i = 0; i < loop->conn_count; i++) {
        if (loop->connections[i] == conn) {
            loop->connections[i] = loop->connections[--loop->conn_count];
            loop->connections[loop->conn_count] = NULL;
            return;
        }
    }
}

connection_t *event_loop_find_conn(event_loop_t *loop, int fd) {
    for (int i = 0; i < loop->conn_count; i++)
        if (loop->connections[i]->fd == fd)
            return loop->connections[i];
    return NULL;
}

void event_loop_run(event_loop_t *loop) {
    loop->running = 1;
    while (loop->running) {
        int nfds = epoll_wait(loop->epoll_fd, loop->events, MAX_EVENTS, -1);
        if (nfds == -1) {
            if (errno == EINTR) {
                    continue; 
            }
            perror("epoll_wait");
            break;
        }
        for (int i = 0; i < nfds; i++) {
            int fd = loop->events[i].data.fd;
            uint32_t ev = loop->events[i].events;
            connection_t *conn = event_loop_find_conn(loop, fd);
            if (conn && loop->handler)
                loop->handler(conn, ev);
        }
    }
}

void event_loop_stop(event_loop_t *loop) {
    loop->running = 0;
}
