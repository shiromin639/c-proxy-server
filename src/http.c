#include "../include/http.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int http_request_is_complete(const char *data, size_t len, size_t *content_length) {
    const char *end = strstr(data, "\r\n\r\n");
    if (!end)
        return 0;

    *content_length = http_parse_content_length(data, len);

    size_t headers_len = (size_t)(end - data) + 4;
    if (*content_length > 0)
        return (len >= headers_len + *content_length);

    return 1;
}

size_t http_parse_content_length(const char *data, size_t len) {
    (void)len;
    const char *cl = strcasestr(data, "content-length:");
    if (!cl) return 0;

    cl += 15; /* skip "content-length:" */
    while (*cl == ' ') cl++;
    return (size_t)atol(cl);
}

int http_response_is_complete(const char *data, size_t len) {
    const char *end = strstr(data, "\r\n\r\n");
    if (!end) return 0;

    size_t content_length = http_parse_content_length(data, len);
    size_t headers_len    = (size_t)(end - data) + 4;

    if (content_length > 0)
        return (len >= headers_len + content_length);

    return 1;
}

int http_get_method(const char *data, char *buf, size_t buf_size) {
    const char *space = strchr(data, ' ');
    if (!space) return -1;

    size_t method_len = (size_t)(space - data);
    if (method_len >= buf_size) return -1;

    memcpy(buf, data, method_len);
    buf[method_len] = '\0';
    return 0;
}

int http_get_uri(const char *data, char *buf, size_t buf_size) {
    const char *p1 = strchr(data, ' ');
    if (!p1) return -1;
    p1++;

    const char *p2 = strchr(p1, ' ');
    if (!p2) return -1;

    size_t uri_len = (size_t)(p2 - p1);
    if (uri_len >= buf_size) return -1;

    memcpy(buf, p1, uri_len);
    buf[uri_len] = '\0';
    return 0;
}

int http_create_error_response(int status, char *buf, size_t buf_size) {
    const char *msg;
    switch (status) {
        case 502: msg = "Bad Gateway";          break;
        case 503: msg = "Service Unavailable";  break;
        case 504: msg = "Gateway Timeout";      break;
        default:  msg = "Internal Server Error"; status = 500; break;
    }

    char body[256];
    int body_len = snprintf(body, sizeof(body),
        "<html><body><h1>%d %s</h1></body></html>", status, msg);

    return snprintf(buf, buf_size,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        status, msg, body_len, body);
}
