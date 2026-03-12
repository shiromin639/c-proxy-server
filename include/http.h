#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

int    http_request_is_complete(const char *data, size_t len, size_t *content_length);
int    http_request_is_complete(const char *data, size_t len, size_t *content_length);
size_t http_parse_content_length(const char *data, size_t len);
int    http_response_is_complete(const char *data, size_t len);
int    http_get_method(const char *data, char *buf, size_t buf_size);
int    http_get_uri(const char *data, char *buf, size_t buf_size);
int    http_create_error_response(int status, char *buf, size_t buf_size);

#endif /* HTTP_H */
