#include "../include/proxy.h"
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

#define LISTEN_PORT   3400
#define BACKEND_HOST  "127.0.0.1"
#define BACKEND_PORT  8081

static void signal_handler(int sig) {
    printf("\nCaught signal %d – shutting down…\n", sig);
    if (g_proxy)
        proxy_stop(g_proxy);
}

int main(void) {
    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN); 
    printf("simple-proxy  :%d → %s:%d\n", LISTEN_PORT, BACKEND_HOST, BACKEND_PORT);

    g_proxy = proxy_create(LISTEN_PORT, BACKEND_HOST, BACKEND_PORT);
    if (!g_proxy) {
        fprintf(stderr, "proxy_create failed\n");
        return EXIT_FAILURE;
    }

    int ret = proxy_start(g_proxy);
    proxy_destroy(g_proxy);
    g_proxy = NULL;

    printf("Proxy stopped.\n");
    return ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
