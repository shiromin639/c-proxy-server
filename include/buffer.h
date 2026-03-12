#ifndef BUFFER_H
#define BUFFER_H

#include <stddef.h>

#define BUFFER_SIZE 8192

typedef struct {
    char data[BUFFER_SIZE];
    size_t len;
} buffer_t;

void   buffer_init(buffer_t *buf);
int    buffer_append(buffer_t *buf, const char *data, size_t len);
void   buffer_consume(buffer_t *buf, size_t len);
const char *buffer_data(const buffer_t *buf);
size_t buffer_len(const buffer_t *buf);
void   buffer_clear(buffer_t *buf);
int    buffer_has_space(const buffer_t *buf, size_t needed);

#endif /* BUFFER_H */
