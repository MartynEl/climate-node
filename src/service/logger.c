#include "service/logger.h"

#include <stddef.h>

#include "platform/platform.h"

#define LOGGER_BUFFER_SIZE 256u
#define LOGGER_CHUNK_SIZE 32u

static char g_buf[LOGGER_BUFFER_SIZE];
static size_t g_head = 0u;
static size_t g_tail = 0u;
static size_t g_count = 0u;

static log_level_t g_level = LOG_LEVEL_INFO;
static uint32_t g_dropped = 0u;

static bool logger_push_char(char c)
{
    if (g_count >= LOGGER_BUFFER_SIZE) {
        g_dropped++;
        return false;
    }

    g_buf[g_head] = c;
    g_head++;

    if (g_head >= LOGGER_BUFFER_SIZE) {
        g_head = 0u;
    }

    g_count++;

    return true;
}

static bool logger_pop_char(char *c)
{
    if (c == NULL || g_count == 0u) {
        return false;
    }

    *c = g_buf[g_tail];
    g_tail++;

    if (g_tail >= LOGGER_BUFFER_SIZE) {
        g_tail = 0u;
    }

    g_count--;

    return true;
}

void logger_init(void)
{
    g_head = 0u;
    g_tail = 0u;
    g_count = 0u;
    g_dropped = 0u;
    g_level = LOG_LEVEL_INFO;
}

void logger_set_level(log_level_t level)
{
    g_level = level;
}

log_level_t logger_get_level(void)
{
    return g_level;
}

bool logger_level_enabled(log_level_t level)
{
    return level <= g_level;
}

void logger_write_char(char c)
{
    (void)logger_push_char(c);
}

void logger_write_str(const char *s)
{
    if (s == NULL) {
        return;
    }

    while (*s != '\0') {
        logger_write_char(*s);
        s++;
    }
}

void logger_write_u32(uint32_t x)
{
    if (x == 0u) {
        logger_write_char('0');
        return;
    }

    char tmp[10];
    uint8_t n = 0u;

    while (x > 0u) {
        tmp[n] = (char)('0' + (x % 10u));
        x /= 10u;
        n++;
    }

    while (n > 0u) {
        n--;
        logger_write_char(tmp[n]);
    }
}

void logger_write_u32_padded(uint32_t x, uint8_t width)
{
    uint32_t y = x;
    uint8_t digits = 0u;

    do {
        digits++;
        y /= 10u;
    } while (y != 0u);

    while (digits < width) {
        logger_write_char('0');
        digits++;
    }

    logger_write_u32(x);
}

void logger_write_i32(int32_t x)
{
    if (x < 0) {
        logger_write_char('-');

        /*
         * Safe conversion for INT32_MIN.
         */
        uint32_t mag = (uint32_t)(-(x + 1)) + 1u;
        logger_write_u32(mag);
    } else {
        logger_write_u32((uint32_t)x);
    }
}

static void logger_write_fixed_mag(uint32_t mag)
{
    uint32_t integer_part = mag / 100u;
    uint32_t frac_part = mag % 100u;

    logger_write_u32(integer_part);
    logger_write_char('.');

    if (frac_part < 10u) {
        logger_write_char('0');
    }

    logger_write_u32(frac_part);
}

void logger_write_fixed_cd(int32_t value_cd)
{
    if (value_cd < 0) {
        logger_write_char('-');

        uint32_t mag = (uint32_t)(-(value_cd + 1)) + 1u;
        logger_write_fixed_mag(mag);
    } else {
        logger_write_fixed_mag((uint32_t)value_cd);
    }
}

void logger_write_fixed_cp(uint32_t value_cp)
{
    logger_write_fixed_mag(value_cp);
}

void logger_new_line(void)
{
    logger_write_char('\r');
    logger_write_char('\n');
}

void logger_task(void)
{
    char chunk[LOGGER_CHUNK_SIZE];
    size_t n = 0u;

    while (n < LOGGER_CHUNK_SIZE && logger_pop_char(&chunk[n])) {
        n++;
    }

    if (n > 0u) {
        platform_write(chunk, n);
    }
}

void logger_flush(void)
{
    while (g_count > 0u) {
        logger_task();
    }
}

uint32_t logger_dropped_chars(void)
{
    return g_dropped;
}