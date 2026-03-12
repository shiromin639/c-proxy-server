#include "../include/buffer.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static int passed = 0, failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  %-40s", #name); test_##name(); } while (0)
#define PASS()     do { puts("PASS"); passed++; } while (0)
#define FAIL(msg)  do { printf("FAIL  (%s)\n", msg); failed++; return; } while (0)
#define CHECK(cond) if (!(cond)) FAIL(#cond)

TEST(init) {
    buffer_t b;
    buffer_init(&b);
    CHECK(buffer_len(&b) == 0);
    CHECK(buffer_has_space(&b, 1));
    PASS();
}

TEST(append_basic) {
    buffer_t b;
    buffer_init(&b);
    int r = buffer_append(&b, "hello", 5);
    CHECK(r == 5);
    CHECK(buffer_len(&b) == 5);
    CHECK(memcmp(buffer_data(&b), "hello", 5) == 0);
    PASS();
}

TEST(append_multiple) {
    buffer_t b;
    buffer_init(&b);
    buffer_append(&b, "foo", 3);
    buffer_append(&b, "bar", 3);
    CHECK(buffer_len(&b) == 6);
    CHECK(memcmp(buffer_data(&b), "foobar", 6) == 0);
    PASS();
}

TEST(append_overflow) {
    buffer_t b;
    buffer_init(&b);
    /* Fill to capacity */
    char zeros[BUFFER_SIZE] = {0};
    int r1 = buffer_append(&b, zeros, BUFFER_SIZE);
    CHECK(r1 == BUFFER_SIZE);
    /* One more byte must fail */
    int r2 = buffer_append(&b, "x", 1);
    CHECK(r2 < 0);
    PASS();
}

TEST(clear) {
    buffer_t b;
    buffer_init(&b);
    buffer_append(&b, "abc", 3);
    buffer_clear(&b);
    CHECK(buffer_len(&b) == 0);
    CHECK(buffer_has_space(&b, BUFFER_SIZE));
    PASS();
}

TEST(consume_partial) {
    buffer_t b;
    buffer_init(&b);
    buffer_append(&b, "hello world", 11);
    buffer_consume(&b, 6); /* remove "hello " */
    CHECK(buffer_len(&b) == 5);
    CHECK(memcmp(buffer_data(&b), "world", 5) == 0);
    PASS();
}

TEST(consume_all) {
    buffer_t b;
    buffer_init(&b);
    buffer_append(&b, "data", 4);
    buffer_consume(&b, 4);
    CHECK(buffer_len(&b) == 0);
    PASS();
}

TEST(consume_more_than_len) {
    buffer_t b;
    buffer_init(&b);
    buffer_append(&b, "hi", 2);
    buffer_consume(&b, 100); /* should clamp to len */
    CHECK(buffer_len(&b) == 0);
    PASS();
}

TEST(has_space) {
    buffer_t b;
    buffer_init(&b);
    CHECK(buffer_has_space(&b, BUFFER_SIZE));
    CHECK(!buffer_has_space(&b, BUFFER_SIZE + 1));
    buffer_append(&b, "x", 1);
    CHECK(buffer_has_space(&b, BUFFER_SIZE - 1));
    CHECK(!buffer_has_space(&b, BUFFER_SIZE));
    PASS();
}

int main(void) {
    printf("=== buffer tests ===\n");
    RUN(init);
    RUN(append_basic);
    RUN(append_multiple);
    RUN(append_overflow);
    RUN(clear);
    RUN(consume_partial);
    RUN(consume_all);
    RUN(consume_more_than_len);
    RUN(has_space);

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
