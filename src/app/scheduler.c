#include "app/scheduler.h"

#include <stddef.h>

static bool sched_is_due(uint32_t now_ms, uint32_t next_ms)
{
    return (int32_t)(now_ms - next_ms) >= 0;
}

void sched_init(scheduler_t *s, sched_task_t *tasks, size_t capacity)
{
    if (s == NULL) {
        return;
    }

    s->tasks = tasks;
    s->capacity = capacity;
    s->count = 0u;
}

bool sched_register(
    scheduler_t *s,
    uint32_t period_ms,
    sched_task_fn fn,
    void *ctx,
    const char *name)
{
    if (s == NULL || s->tasks == NULL) {
        return false;
    }

    if (fn == NULL || period_ms == 0u) {
        return false;
    }

    if (s->count >= s->capacity) {
        return false;
    }

    sched_task_t *t = &s->tasks[s->count];

    t->period_ms = period_ms;
    t->next_ms = 0u;
    t->fn = fn;
    t->ctx = ctx;
    t->name = name;

    s->count++;

    return true;
}

bool sched_run_once(scheduler_t *s, uint32_t now_ms)
{
    if (s == NULL || s->tasks == NULL) {
        return false;
    }

    bool any = false;

    for (size_t i = 0u; i < s->count; ++i) {
        sched_task_t *t = &s->tasks[i];

        if (!sched_is_due(now_ms, t->next_ms)) {
            continue;
        }

        t->fn(t->ctx);
        any = true;

        t->next_ms += t->period_ms;

        /*
         * If the task missed several periods, do not try to catch up.
         * Reschedule from now.
         */
        if (sched_is_due(now_ms, t->next_ms)) {
            t->next_ms = now_ms + t->period_ms;
        }
    }

    return any;
}