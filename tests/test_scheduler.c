#include <stdint.h>

#include "app/scheduler.h"
#include "test_support.h"

static int g_calls;
static int g_ctx_value;

static void counting_task(void *ctx)
{
    g_calls++;

    if (ctx != NULL) {
        g_ctx_value = *(int *)ctx;
    }
}

static void test_basic_period(void)
{
    sched_task_t tasks[2];
    scheduler_t sched;
    int ctx = 42;

    sched_init(&sched, tasks, 2u);

    g_calls = 0;
    g_ctx_value = 0;

    CHECK_TRUE(sched_register(&sched, 10u, counting_task, &ctx, "count"));

    CHECK_TRUE(sched_run_once(&sched, 0u));
    CHECK_EQ_INT(g_calls, 1);
    CHECK_EQ_INT(g_ctx_value, 42);

    CHECK_FALSE(sched_run_once(&sched, 5u));
    CHECK_EQ_INT(g_calls, 1);

    CHECK_TRUE(sched_run_once(&sched, 10u));
    CHECK_EQ_INT(g_calls, 2);
}

static void test_wrap(void)
{
    sched_task_t tasks[1];
    scheduler_t sched;
    int ctx = 7;

    sched_init(&sched, tasks, 1u);

    g_calls = 0;
    g_ctx_value = 0;

    CHECK_TRUE(sched_register(&sched, 10u, counting_task, &ctx, "count"));

    uint32_t now = (uint32_t)(UINT32_MAX + 10u);
    tasks[0].next_ms = UINT32_MAX - 5u;

    CHECK_TRUE(sched_run_once(&sched, now));
    CHECK_EQ_INT(g_calls, 1);

    CHECK_FALSE(sched_run_once(&sched, now));
    CHECK_EQ_INT(g_calls, 1);
}

static void test_bad_args(void)
{
    sched_task_t tasks[1];
    scheduler_t sched;

    sched_init(&sched, tasks, 1u);

    CHECK_FALSE(sched_register(&sched, 0u, counting_task, NULL, "zero"));
    CHECK_FALSE(sched_register(&sched, 10u, NULL, NULL, "null"));
    CHECK_FALSE(sched_run_once(NULL, 0u));
}

int main(void)
{
    RUN(test_basic_period);
    RUN(test_wrap);
    RUN(test_bad_args);

    return test_report();
}