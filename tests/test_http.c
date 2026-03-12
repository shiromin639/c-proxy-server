#include "../include/http.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static int passed = 0, failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  %-40s", #name); test_##name(); } while (0)
#define PASS()     do { puts("PASS"); passed++; } while (0)
#define FAIL(msg)  do { printf("FAIL  (%s)\n", msg); failed++; return; } while (0)
#define CHECK(cond) if (!(cond)) FAIL(#cond)


static const char *GET_NO_BODY =
    "GET / HTTP/1.1\r\n"
    "Host: localhost\r\n"
    "\r\n";

static const char *POST_WITH_BODY =
    "POST /submit HTTP/1.1\r\n"
    "Host: localhost\r\n"
    "Content-Length: 11\r\n"
    "\r\n"
    "hello world";

static const char *INCOMPLETE_HEADERS =
    "GET / HTTP/1.1\r\n"
    "Host: localh";   /* no \r\n\r\n yet */

static const char *RESPONSE_200 =
    "HTTP/1.1 200 OK\r\n"
    "Content-Length: 13\r\n"
    "\r\n"
    "Hello, world!";

static const char *RESPONSE_NO_BODY =
    "HTTP/1.1 204 No Content\r\n"
    "\r\n";


TEST(request_complete_no_body) {
    size_t cl = 99;
    int r = http_request_is_complete(GET_NO_BODY, strlen(GET_NO_BODY), &cl);
    CHECK(r == 1);
    CHECK(cl == 0);
    PASS();
}

TEST(request_complete_with_body) {
    size_t cl = 0;
    int r = http_request_is_complete(POST_WITH_BODY, strlen(POST_WITH_BODY), &cl);
    CHECK(r == 1);
    CHECK(cl == 11);
    PASS();
}

TEST(request_incomplete_headers) {
    size_t cl = 0;
    int r = http_request_is_complete(INCOMPLETE_HEADERS,
                                     strlen(INCOMPLETE_HEADERS), &cl);
    CHECK(r == 0);
    PASS();
}

TEST(request_incomplete_body) {
    /* POST with body truncated */
    const char *partial =
        "POST /x HTTP/1.1\r\n"
        "Content-Length: 10\r\n"
        "\r\n"
        "short";          /* only 5 of 10 bytes */
    size_t cl = 0;
    int r = http_request_is_complete(partial, strlen(partial), &cl);
    CHECK(r == 0);
    CHECK(cl == 10);
    PASS();
}

TEST(headers_complete) {
    CHECK(http_request_is_complete(GET_NO_BODY, strlen(GET_NO_BODY), &(size_t){0}));
    CHECK(!http_request_is_complete(INCOMPLETE_HEADERS, strlen(INCOMPLETE_HEADERS), &(size_t){0}));
    PASS();
}

TEST(parse_content_length_present) {
    size_t cl = http_parse_content_length(POST_WITH_BODY, strlen(POST_WITH_BODY));
    CHECK(cl == 11);
    PASS();
}

TEST(parse_content_length_absent) {
    size_t cl = http_parse_content_length(GET_NO_BODY, strlen(GET_NO_BODY));
    CHECK(cl == 0);
    PASS();
}

TEST(parse_content_length_case_insensitive) {
    const char *req =
        "GET / HTTP/1.1\r\n"
        "CONTENT-LENGTH: 7\r\n"
        "\r\n"
        "1234567";
    size_t cl = http_parse_content_length(req, strlen(req));
    CHECK(cl == 7);
    PASS();
}

TEST(response_complete_with_body) {
    int r = http_response_is_complete(RESPONSE_200, strlen(RESPONSE_200));
    CHECK(r == 1);
    PASS();
}

TEST(response_complete_no_body) {
    int r = http_response_is_complete(RESPONSE_NO_BODY, strlen(RESPONSE_NO_BODY));
    CHECK(r == 1);
    PASS();
}

TEST(response_incomplete) {
    const char *partial =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 100\r\n"
        "\r\n"
        "only 9 by";
    int r = http_response_is_complete(partial, strlen(partial));
    CHECK(r == 0);
    PASS();
}

TEST(get_method) {
    char buf[16];
    CHECK(http_get_method(GET_NO_BODY, buf, sizeof(buf)) == 0);
    CHECK(strcmp(buf, "GET") == 0);
    CHECK(http_get_method(POST_WITH_BODY, buf, sizeof(buf)) == 0);
    CHECK(strcmp(buf, "POST") == 0);
    PASS();
}

TEST(get_uri) {
    char buf[64];
    CHECK(http_get_uri(GET_NO_BODY, buf, sizeof(buf)) == 0);
    CHECK(strcmp(buf, "/") == 0);
    CHECK(http_get_uri(POST_WITH_BODY, buf, sizeof(buf)) == 0);
    CHECK(strcmp(buf, "/submit") == 0);
    PASS();
}

TEST(error_response_502) {
    char buf[512];
    int len = http_create_error_response(502, buf, sizeof(buf));
    CHECK(len > 0);
    CHECK(strstr(buf, "502") != NULL);
    CHECK(strstr(buf, "Bad Gateway") != NULL);
    CHECK(strstr(buf, "Content-Length:") != NULL);
    PASS();
}

TEST(error_response_unknown_status) {
    char buf[512];
    int len = http_create_error_response(999, buf, sizeof(buf));
    CHECK(len > 0);
    CHECK(strstr(buf, "500") != NULL);
    PASS();
}

int main(void) {
    printf("=== http tests ===\n");
    RUN(request_complete_no_body);
    RUN(request_complete_with_body);
    RUN(request_incomplete_headers);
    RUN(request_incomplete_body);
    RUN(headers_complete);
    RUN(parse_content_length_present);
    RUN(parse_content_length_absent);
    RUN(parse_content_length_case_insensitive);
    RUN(response_complete_with_body);
    RUN(response_complete_no_body);
    RUN(response_incomplete);
    RUN(get_method);
    RUN(get_uri);
    RUN(error_response_502);
    RUN(error_response_unknown_status);

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
