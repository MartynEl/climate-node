#ifndef TEST_SUPPORT_H
#define TEST_SUPPORT_H

#include <stdio.h>

static int g_failures = 0;

static inline void check_bool(const char *file, int line, const char *expr, int ok)
{
    if (!ok) {
        printf("FAIL %s:%d: %s\n", file, line, expr);
        g_failures++;
    }
}

static inline void check_int(
    const char *file,
    int line,
    const char *expr_a,
    const char *expr_b,
    long long a,
    long long b)
{
    if (a != b) {
        printf("FAIL %s:%d: %s == %s (%lld != %lld)\n", file, line, expr_a, expr_b, a, b);
        g_failures++;
    }
}

static inline int test_report(void)
{
    if (g_failures != 0) {
        printf("FAILURES: %d\n", g_failures);
        return 1;
    }

    printf("PASS\n");
    return 0;
}

#define CHECK(cond) check_bool(__FILE__, __LINE__, #cond, (cond) ? 1 : 0)
#define CHECK_TRUE(cond) CHECK(cond)
#define CHECK_FALSE(cond) CHECK(!(cond))
#define CHECK_EQ_INT(actual, expected) check_int(__FILE__, __LINE__, #actual, #expected, (long long)(actual), (long long)(expected))
#define RUN(test_fn) do { printf("RUN %s\n", #test_fn); test_fn(); } while (0)

#endif // TEST_SUPPORT_H