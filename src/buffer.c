#include "../include/buffer.h"
#include <string.h>

void buffer_init(buffer_t *buf) {
    buf->len = 0;
}

int buffer_append(buffer_t *buf, const char *data, size_t len) {
    if (!buffer_has_space(buf, len))
        return -1;
    memcpy(buf->data + buf->len, data, len);
    buf->len += len;
    return (int)len;
}

void buffer_consume(buffer_t *buf, size_t len) {
    if (len >= buf->len) {
        buf->len = 0;
        return;
    }
    memmove(buf->data, buf->data + len, buf->len - len);
    buf->len -= len;
}

const char *buffer_data(const buffer_t *buf) {
    return buf->data;
}

size_t buffer_len(const buffer_t *buf) {
    return buf->len;
}

void buffer_clear(buffer_t *buf) {
    buf->len = 0;
}

int buffer_has_space(const buffer_t *buf, size_t needed) {
    return (buf->len + needed <= BUFFER_SIZE);
}
