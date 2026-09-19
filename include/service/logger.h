#ifndef SERVICE_LOGGER_H
#define SERVICE_LOGGER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG
} log_level_t;

void logger_init(void);

void logger_set_level(log_level_t level);
log_level_t logger_get_level(void);
bool logger_level_enabled(log_level_t level);

void logger_write_char(char c);
void logger_write_str(const char *s);

void logger_write_u32(uint32_t x);
void logger_write_u32_padded(uint32_t x, uint8_t width);
void logger_write_i32(int32_t x);

void logger_write_fixed_cd(int32_t value_cd);
void logger_write_fixed_cp(uint32_t value_cp);

void logger_new_line(void);

void logger_task(void);
void logger_flush(void);

uint32_t logger_dropped_chars(void);

#endif // SERVICE_LOGGER_H