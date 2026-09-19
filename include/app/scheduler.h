#ifndef APP_SCHEDULER_H
#define APP_SCHEDULER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void (*sched_task_fn)(void *ctx);

typedef struct {
    uint32_t period_ms;
    uint32_t next_ms;
    sched_task_fn fn;
    void *ctx;
    const char *name;
} sched_task_t;

typedef struct {
    sched_task_t *tasks;
    size_t capacity;
    size_t count;
} scheduler_t;

void sched_init(scheduler_t *s, sched_task_t *tasks, size_t capacity);

bool sched_register(
    scheduler_t *s,
    uint32_t period_ms,
    sched_task_fn fn,
    void *ctx,
    const char *name);

bool sched_run_once(scheduler_t *s, uint32_t now_ms);

#endif // APP_SCHEDULER_H